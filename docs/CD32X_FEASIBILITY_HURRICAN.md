# Hurrican on the Sega CD32X - feasibility study

Question: can Hurrican (the SDL2/OpenGL fork, `HurricanGame/Hurrican`) run on a Mega Drive +
Mega-CD + 32X with the 4 MB RAM cart, the way Jazz Jackrabbit does in the OpenJazz CD32X port?

Short answer:

- **A straight port is not feasible.** Recompiling the engine as it is for the SH2 gives an
  estimated **0.15-2.3 frames per second**, with the game itself running at 3-46 % of normal speed.
  The main cause is not the graphics: Hurrican's game logic is written in floating point, and the
  SH2 has no floating-point unit.
- **The data fits**, but only with packed (run-length) sprites and recompressed maps.
- **A re-engineered version is possible on paper:** fixed-point logic on the second SH2, the
  parallax backgrounds on the Genesis video chip, the Jazz line-table renderer for the tiles, no
  alpha blending. Estimate: **about 10 fps in most stages (range 4-25)**, in the same class as
  Jazz (10-13 fps). That is a rewrite of the engine core, not a port.

All numbers below were measured on the PC build of the uploaded source, with a probe patch
(`tools/probe.patch`), unless marked as an estimate.

---

## 1. Method

1. Hurrican built on Linux (SDL2, OpenGL 2 through Mesa llvmpipe, headless under Xvfb).
2. Probe patch (`tools/probe.patch`, about 200 lines):
   - every draw call: the screen area of its triangles (clipped to the 640 x 480 screen), its blend
     mode (normal / additive / white), textured or not, the vertex alpha, and which part of the
     frame drew it (parallax, back tiles, front tiles, enemies, player, shots, particles,
     water/overlay/shadow, HUD, lightning, other);
   - every picture loaded (name, size, frames), and the level load;
   - per frame: logic time, logic steps, number of enemies, shots and particles in the lists;
   - automation: `HUR_LEVEL=n` starts stage n directly, `HUR_SCRIPT=1` plays (run, turn when
     stuck, jump, fire, grenades), `HUR_SWEEP=<file>` teleports the player through a list of spots,
     `HUR_FRAMES=n` stops, `HUR_NOGL=1` skips the actual GL drawing (for the CPU profile). The
     player is invulnerable.
3. Two runs per stage:
   - scripted play from the start (9000 frames);
   - a sweep: the player is placed on 38-114 standing spots spread over the whole map (made from
     the map's wall and floor flags, `tools/spots.py`), about one second each, firing.
   Both give a similar picture (totals within 20 %; in stage 3 the script got stuck early); the
   tables use the sweep, which covers the whole map.
4. CPU profile: valgrind (callgrind) on a non-LTO build with `HUR_NOGL=1`, counting x86
   instructions inside `GameLoop()` for 6 stages, each instruction classified as integer or
   floating point (`tools/cgparse.py`). The probe's own code, OpenGL and SDL are excluded.
5. Calibration: the OpenJazz CD32X PC build (the same code that runs on the console) under the
   same profiler: **763,000 x86 instructions per frame** in Turtle Terror, level 1. On the console
   that code runs at 10-13 fps, i.e. 1.77-2.30 million SH2 cycles per frame. So one x86
   instruction of this kind of game code costs **2.3-3.0 SH2 cycles** on the 32X (memory waits
   and drawing included).
6. Pixel cost on the 32X (cycles per pixel at 8 bits): low = hand-written assembly at bus speed
   (opaque 3, keyed 6, blended 14); high = what the Jazz renderer measured on the console (full
   redraw 3-7 fps against 10-13 fps with the line table: about 135 ms for about 64,000 tile pixels,
   roughly 40 cycles per pixel); middle in between.

Stages are named by number and map file: Hurrican's menus show only "Stage n".

| Stage | Map | Map size (tiles of 20 x 20) | Objects in the map |
|---|---|---|---|
| 1 | jungle.map | 650 x 256 | 1139 |
| 2 | temple.map | 400 x 256 | 886 |
| 3 | elevator.map | 64 x 1040 | 357 |
| 4 | mpde.map | 430 x 290 | 710 |
| 5 | spinnenfabrik.map | 650 x 350 | 681 |
| 6 | tower.map | 350 x 400 | 1029 |
| 7 | eis.map | 700 x 400 | 924 |
| 8 | cave.map | 440 x 320 | 727 |
| 9 | endboss.map | 400 x 600 | 702 |

---

## 2. Graphics per stage at 320 x 240, 256 colours

Every picture the game loads, halved in both directions, 1 byte per pixel. "Packed" = only the
visible pixels plus about 12 % run overhead (sprite sheets are 43 % fully transparent).

| | Pictures | Full | Packed |
|---|---|---|---|
| Resident (loaded at game start: player, HUD, fonts, menus, common enemies, explosions) | 218 | 1821 KB | 1120 KB |

| Stage | Stage pictures, full | packed | of which parallax backgrounds |
|---|---|---|---|
| 1 jungle | 954 KB | 561 KB | 262 KB |
| 2 temple | 849 KB | 535 KB | 262 KB |
| 3 elevator | 823 KB | 443 KB | 262 KB |
| 4 mpde | 705 KB | 445 KB | 262 KB |
| 5 spinnenfabrik | 1167 KB | 711 KB | 262 KB |
| 6 tower | 901 KB | 438 KB | 262 KB |
| 7 eis | 1131 KB | 654 KB | 262 KB |
| 8 cave | 786 KB | 377 KB | 262 KB |
| 9 endboss | 1233 KB | 641 KB | 262 KB |

Tile sets are small: 256 x 256 sheets of 144 tiles each; a stage uses 836-1561 different tiles,
which at 10 x 10 pixels is 82-152 KB.

Quality notes:
- Halving the art: the player becomes about 25 x 40 pixels. The game logic can stay at 640 x 480
  and be drawn at half scale; the 32X shows 320 x 224, i.e. 640 x 448 of the game's view.
- 256 colours for everything: Hurrican is true colour with soft alpha edges (9 % of tile-set
  pixels and most smoke, fire and laser pictures are partly transparent). Edges become hard; smoke
  and glow need new art or dithering.
- Tiles carry a light colour per corner (per-tile lighting). At 8 bits this has to become a few
  shade levels (the maps below are sized with 4 shades) or be dropped.

## 3. Level maps and memory

The PC engine keeps a fixed 1024 x 1600 tile array of 32 bytes per tile: **52 MB**. On the 32X
the map must live in the cart, compressed:

| Stage | Raw map (12 B/tile) | 8x8-tile blocks, deduplicated, no light | with 4 shade levels |
|---|---|---|---|
| 1 jungle | 2.00 MB | 216 KB | 263 KB |
| 2 temple | 1.23 MB | 186 KB | 200 KB |
| 3 elevator | 0.80 MB | 113 KB | 127 KB |
| 4 mpde | 1.50 MB | 231 KB | 257 KB |
| 5 spinnenfabrik | 2.73 MB | 214 KB | 230 KB |
| 6 tower | 1.68 MB | 165 KB | 180 KB |
| 7 eis | 3.36 MB | 350 KB | 372 KB |
| 8 cave | 1.69 MB | 172 KB | 212 KB |
| 9 endboss | 2.88 MB | 221 KB | 241 KB |

Destructible walls change tiles at run time: they need a small change list in SDRAM, not a copy
of the map.

### Cart layout (4 MB)

| | Unpacked graphics | Packed graphics |
|---|---|---|
| Program (estimate: the x86 code is 1.05 MB at -O2; OpenJazz's 445 KB x86 became about 420 KB on the SH2) | ~1000 KB | ~1000 KB |
| Resident graphics | 1821 KB | 1120 KB |
| Largest stage (graphics + map) | 1503 KB (eis) | 1026 KB (eis) |
| Saves and settings | 64 KB | 64 KB |
| **Total** | **~4390 KB - does not fit** | **~3210 KB - fits, ~880 KB spare** |

Against the 1.4 MB stage slot of the Jazz layout (1472 KB): unpacked, eis (1503 KB) and endboss
(1474 KB) do not fit; packed, every stage fits (377-711 KB of graphics + map).

### 32X work memory (256 KB SDRAM)

| | Measured | Memory as written |
|---|---|---|
| Objects in the enemy list (all of the map's objects stay in the list) | 277-995 per stage on average, peak 1094 | 149 enemy classes, average 173 bytes, largest 792: **50-190 KB** |
| Particles | average 58-665, peak per stage (90th percentile) 100-1249, overall peak 2258 | 80 bytes each: up to **100-180 KB** |
| Shots | average under 18, peak 80 | 88 bytes each: small |
| Player object | | 24.5 KB |

Objects and particles alone can exceed the whole SDRAM. A 32X version must keep the object
list in the cart and create objects only near the view, and cap particles (a few hundred).

## 4. Pixels drawn per frame

At 320 x 240 (one screen = 76,800 pixels), averaged over the sweep, in thousands of pixels:

| Stage | Total | 90 % | Max | Parallax | Tiles | Water/overlay/shadow | Enemies, player, shots | Particles, lightning | HUD | of all: translucent | additive |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 jungle | 312 | 355 | 483 | 220 | 25 | 31 | 9 | 13 | 13 | 16 | 42 |
| 2 temple | 328 | 375 | 472 | 212 | 43 | 39 | 9 | 11 | 14 | 20 | 45 |
| 3 elevator | 374 | 408 | 462 | 265 | 35 | 36 | 8 | 16 | 12 | 12 | 48 |
| 4 mpde | 330 | 370 | 462 | 220 | 37 | 37 | 11 | 10 | 13 | 15 | 42 |
| 5 spinnenfabrik | 331 | 373 | 488 | 206 | 44 | 36 | 11 | 18 | 14 | 17 | 49 |
| 6 tower | 311 | 369 | 454 | 222 | 21 | 27 | 10 | 17 | 14 | 25 | 42 |
| 7 eis | 324 | 377 | 451 | 216 | 33 | 38 | 12 | 11 | 14 | 15 | 45 |
| 8 cave | 415 | 456 | 541 | 206 | 43 | 128 | 12 | 11 | 13 | 35 | 50 |
| 9 endboss | 315 | 370 | 453 | 223 | 25 | 27 | 14 | 13 | 12 | 12 | 38 |

- Every frame covers the screen **4-5.4 times**. Most of it (2.7-3.5 screens) is the four
  parallax layers (a full static picture, two full 640 x 480 layers, a cloud strip), redrawn
  every frame.
- 16-22 % of all pixels are blended (translucent or additive): glow, shots, explosions, smoke.
- Stage 8 (cave) draws a full-screen shadow overlay (the torch light), about 128,000 pixels.
- 161-319 draw calls per frame.

For comparison: the Jazz port draws per frame only new tile strips, changed tiles, the sprites and
the background under last frame's sprites - a small fraction of one screen.

## 5. CPU work per frame

x86 instructions per frame on the PC, without OpenGL, split into game logic (player, enemies,
collision, level update) and draw preparation (the engine's own work to build the draw lists):

| Stage | Logic steps per frame (PC) | Logic, x86 instructions per step: integer + float | float share | Draw preparation per frame: integer + float |
|---|---|---|---|---|
| 1 jungle | 1.21 | 346 k + 56 k | 14 % | 250 k + 34 k |
| 5 spinnenfabrik | 1.86 | 689 k + 142 k | 17 % | 314 k + 48 k |
| 6 tower | 1.55 | 312 k + 53 k | 14 % | 219 k + 17 k |
| 7 eis | 1.35 | 101 k + 16 k | 13 % | 226 k + 27 k |
| 8 cave | 1.30 | 241 k + 42 k | 15 % | 312 k + 24 k |
| 9 endboss | 1.59 | 187 k + 32 k | 15 % | 231 k + 27 k |

(Packed SSE operations count as 4. Raw data: `tools/results/cgres_L*.json`.) For scale: the whole
Jazz frame is 763 k x86 instructions.

The floating-point share looks small in instructions but dominates on the SH2: without an FPU
each float addition is a library call of roughly 50-150 cycles (multiplication 50-130,
comparison 20-60, conversion 30-90), against 2.3-3.0 cycles for an ordinary instruction. The
source has 3355 `float` declarations and 292 trigonometry/sqrt calls; OpenJazz has 9.

Hot spots worth knowing: `TileEngineClass::BlockSlopes` and `BlockUntenNormal` test slopes
pixel by pixel with a float conversion per pixel (in stage 5 BlockSlopes alone is 476 k x86
instructions per logic step); `GegnerClass::Run` is called for every object in the list, every
step, even far away.

The engine also runs extra logic steps when frames get slow: one step may cover at most 0.3 of
a 1/10 s unit, so at 10 fps it runs 4 logic steps per frame, and below 5 fps the game slows down.

## 6. Frame-rate estimate

### A. Straight port (the engine as it is, compiled for the SH2, drawing in software)

Estimate, milliseconds on the SH2 at 23 MHz, low / middle / high:

| Stage | Logic per step | Draw preparation | Pixels | Result: fps (game speed) |
|---|---|---|---|---|
| 1 jungle | 131 / 198 / 314 | 100 / 150 / 235 | 92 / 226 / 526 | 0.9 / 0.6 / 0.3 (18 / 11 / 7 %) |
| 5 spinnenfabrik | 325 / 496 / 801 | 137 / 207 / 325 | 99 / 243 / 565 | 0.4 / 0.25 / 0.15 (8 / 5 / 3 %) |
| 6 tower | 124 / 187 / 299 | 59 / 85 / 128 | 94 / 230 / 532 | 1.0 / 0.6 / 0.4 (20 / 12 / 7 %) |
| 7 eis | 37 / 56 / 88 | 82 / 122 / 189 | 95 / 235 / 549 | 2.3 / 1.3 / 0.7 (46 / 27 / 15 %) |
| 8 cave | 98 / 149 / 238 | 81 / 116 / 175 | 127 / 313 / 727 | 1.1 / 0.7 / 0.4 (22 / 14 / 8 %) |
| 9 endboss | 76 / 115 / 183 | 81 / 121 / 188 | 90 / 224 / 526 | 1.4 / 0.9 / 0.5 (28 / 17 / 10 %) |

Pixels for stages 2-4: 96-109 / 237-270 / 556-636 ms. Even with zero drawing, one logic step
(needed about 33 times per second of game time) takes longer than a whole Jazz frame.

### B. Re-engineered version

Assumptions:
- all game logic in fixed point (each float operation becomes about two integer instructions);
- the slope and floor tests rewritten per tile instead of per pixel (90 % of their cost removed);
- the logic on the slave SH2, the drawing on the master SH2 (in Jazz the slave gained nothing
  because both CPUs drew; logic runs mostly from cache and SDRAM);
- the far parallax layers on the Genesis VDP planes (as tried in the yatssd branch), the tile
  layer kept in a line-table ring like Jazz;
- sprites, overlay tiles and particles drawn keyed, with the background restored under them
  (factor 1.6); no translucency or additive blending (glow as keyed or dithered art);
- frame = the slower of the two CPUs, plus 15 % for sharing the bus.

| Stage | Master: drawing per frame (ms) | Slave: one logic step (ms) | fps, 1 step per frame (step size vs. PC) | fps, 2 steps per frame |
|---|---|---|---|---|
| 1 jungle | 34 / 76 / 176 | 28 / 32 / 37 | 25 / 11 / 5 (x0.9) | 15 / 11 / 5 |
| 2 temple | 38 / 86 / 201 | (not profiled) | | |
| 3 elevator | 37 / 84 / 194 | (not profiled) | | |
| 4 mpde | 37 / 83 / 193 | (not profiled) | | |
| 5 spinnenfabrik | 39 / 90 / 211 | 33 / 38 / 43 | 22 / 10 / 4 (x1.0) | 13 / 10 / 4 |
| 6 tower | 34 / 76 / 176 | 21 / 24 / 27 | 25 / 11 / 5 (x0.9) | 21 / 11 / 5 |
| 7 eis | 37 / 85 / 198 | 8 / 9 / 11 | 23 / 10 / 4 (x1.0) | 23 / 10 / 4 |
| 8 cave | 76 / 189 / 458 | 15 / 17 / 20 | 11 / 5 / 2 (x2.2) | 11 / 5 / 2 |
| 9 endboss | 34 / 76 / 177 | 14 / 16 / 18 | 25 / 11 / 5 (x0.9) | 25 / 11 / 5 |

- The middle case gives **about 10-11 fps** in every profiled stage but the cave; the drawing on
  the master decides, as in Jazz.
- One logic step per frame at 10 fps is a step 3 times larger than the engine allows (0.3);
  collision would need to cope with that, or the logic runs 2-3 steps per frame where the slave
  has time.
- The cave's shadow overlay has to become a palette effect (darkening colours instead of drawing
  a full-screen layer); then it is like the other stages.

## 7. Other points

- **Audio:** music is 8.3 MB of tracker modules: rendered to CD audio tracks like the Jazz port
  (the PC can render them with libopenmpt). Sound effects (2.8 MB): resampled for the PCM chip.
- **Licence:** the code is MIT. The content is "copyright by the author (poke53280)", may be
  passed on only unmodified, and *"for distribution on any data storage medium, a written
  permission of the author is required"* (Hurrican/readme.txt). A disc with converted graphics is
  both. Building the disc from the player's own download (the Jazz model) avoids distributing the
  data, but asking the author (information@poke53280.de) before a release is the clean way.
- **Work involved (option B):** fixed-point rewrite of the player, collision and 149 enemy and
  boss classes; a new renderer; the asset converter (halving, palette, packing, maps); object
  streaming; the CD32X boot, cart and sound layers can be reused from the Jazz port.

## 8. Verdict and options

1. **Straight port: no.** 0.15-2.3 fps and slow motion; the float logic alone is too slow.
2. **Re-engineered Hurrican: possible, Jazz-like frame rate (about 10 fps estimated), large
   effort.** The data fits with packing; memory needs object streaming.
3. **Before committing:** a hardware benchmark disc with no game logic: stage 1's tile layer in
   the line-table ring, two parallax layers on the Genesis planes, and the measured load of
   sprites, overlay and particles (about 90,000 keyed pixels per frame). Its frame rate replaces
   the low/middle/high pixel guesses with a measurement, and decides between option 2 and
   dropping the idea. Second step: the player and tile collision in fixed point on the slave.

## Files

- `tools/probe.patch` - instrumentation and automation (apply in the Hurrican source folder:
  `patch -p0 < probe.patch` from the folder that contains `Hurrican/`).
- `tools/maps.py`, `mapstats.py` - map headers, map compression sizes.
- `tools/spots.py` - standing spots for the sweep.
- `tools/texbudget.py`, `alpha.py` - picture sizes, transparency.
- `tools/analyse.py` - per-frame pixel statistics from the probe output.
- `tools/cgparse.py` - callgrind output split into integer and floating-point instructions.
- `tools/model.py` - the frame-time estimate (constants at the top).
- `tools/results/` - the measured data the tables come from.
