# Hurrican CD32X - handoff

## Where things stand
- `docs/CD32X_FEASIBILITY_HURRICAN.md`: the study. A straight port of the PC engine runs at
  0.15-2.3 fps (float logic without an FPU); a rebuilt engine (fixed point, logic on the slave
  SH2, parallax on the Genesis planes, the Jazz line-table renderer, no blending) is estimated
  at about 10 fps. The drawing cost on the 32X was the least certain number: B1 measures it.
- **B1**: the benchmark disc (no game logic) - run on the console, results below.
- **P1** (this build): stage 1 playable, with sound effects and music. Disc
  `sega/HurricanP1c/HurricanP1c.cue`.

## B1 design
- **Boot path = the OpenJazz port's J19b, byte for byte** (the last disc proven on the console):
  `cd32x/`, `port/` (crt0_oj.s, oj.ld, plat_mars.c, mars_syscalls.c, pack.c, mars_boottest.cpp),
  `tools/make_oj_rom.py`, `tools/make_boot_image.py`, `tools/mars_boot.bin`; compiled with the
  J19b engine's flags, linked with g++ as J19b. Module name "OpenJazz 32X", disc file
  `OPENLARA.32X`, resident pack `IMAGE8.RAW` at 0x100000 (quick check as Jazz), `SFX00.BIN`
  (Hurrican's sounds in the Jazz pack format) loaded at start-up as Jazz does.
- B1's first build (a modified platform layer: no C++ start-up, ring 480 x 200 present, no sound
  pack, own module name) gave a black screen on the console from the start. Rule since then: the
  boot path is never edited; new code only runs from `main()` and uses existing calls.
- Program: `bench/bench.c` (C) on the J19b calls: `plat_scroll_ring`, `plat_scroll_panel`,
  `plat_back_index`, `plat_present_scroll`, `plat_wait_flip`, `plat_border_colour`,
  `plat_ticks_ms`, `plat_prof_us`, `plat_read_pad`, `plat_load_pack`, `plat_cart_ptr`.
  Drawing routines in SDRAM (`.sdcode.fast`, as the Jazz port's FAST_CODE).
- Screen: the Jazz port's: ring 384 x 200 at 0x200 (38 x 20 cells of 10 x 10), view 167 lines,
  panel 320 x 33 at 0x12E00, 12 border lines above and below. Only 5 spare columns: moving
  sideways past them re-anchors the ring (whole view redrawn) - counted as "A" on the panel.
  A wider ring needs its own present function: a later step, after B1's results.
- Restores: whole cells under the rectangles the buffer's last frame drew; walls (Hurrican's
  overlay tiles) drawn again only over this frame's sprites.
- Pixels: 16-bit frame buffer writes only (Kobo rule 4), read-modify-write at odd edges.
  Sprites are runs of visible pixels; the additive effects go through a 64 KB table in the cart.

## Data (`tools/hur_convert.py`)
- Picture list with frame sizes: `tools/data/hurrican_textures.txt` (recorded from the PC game
  by the feasibility probe, `tools/data/feasibility_probe.patch`): R = loaded at game start
  (resident), Sn = loaded for stage n.
- Halved, 256 colours: 0 transparent / backdrop, 1-96 resident, 97-255 stage.
- Pictures without transparency (glows, smoke) are Hurrican's additive ones: their alpha is made
  from brightness, and they are flagged (texture table flags bit 0).
- Map layers as Hurrican draws them (Tileengine.cpp): under the sprites = back tile unless wall
  (or wall with a covering front tile) + front tile unless covering/wall; over the sprites = back
  tile of walls + front tile of covering/wall cells. Lighting colours, animated and water tiles
  are not converted yet.
- Camera path: `tools/data/stage1_path.txt` (player positions from a scripted PC run).
- Sizes (stage 1): resident 1199 KB, stage 829 KB (tiles 135 KB, map 216 KB, pictures 411 KB,
  blend table 64 KB).

## Checks done (PC)
- `host/` builds the same bench.c with a stand-in platform (`bench/host.c`) that reads the line
  table back as the 32X does and saves frames: RING, ADD, FULL and sprites-off frames looked at.
- Same run under the address and undefined-behaviour checkers: 4000 frames with every button,
  the whole camera path (wrapping), no error.
- Boot files compared with the J19b commit: all byte-identical.
- SH2: compiled and linked with sh-elf gcc 12.1 / binutils 2.42 / newlib 4.1 built from source
  here (the console toolchain is gcc 12.1 too). The Sub-CPU step needs the m68k compiler (not
  here); it is J19b's, unchanged.

## B1b on the console (videos h2, h3, h7)
Boots and runs (the first B1, with an edited boot path, gave a black screen). Stage 1, load 1:

| Mode | fps | ms/frame | parts (ms) |
|---|---|---|---|
| RING, effects keyed | 8.1-11.3 | 88-122 | MAP 1.8-27.8, RST 13-39, SPR 4-6.7, FX 31.6-36.2, OVR 8-26 |
| FULL, sprites off | 7.4-8.5 | 116-133 | BG 84.6-85.1, MAP 9.6-17.6, OVR 7.4-26.1 |

- Effects (about 42,000 pixels a frame, the PC game's average) cost ~34 ms: ~19 cycles per
  keyed pixel from the cart - the study's middle estimate (16).
- Three background layers on the 32X cost 85 ms: they must go to the Genesis planes.
- RST and OVR (cells under / over the sprites, whole 10 x 10 cells) grow with the number of
  sprites; MAP spikes are re-anchors (only 5 spare columns in the Jazz ring: A rose to ~480).
- Loading: stage pack 829 KB in 19.8 s with the loading screen = 42 KB/s; start to benchmark
  5.2 s (resident pack already in the cart).
- Not yet measured: additive effects (C), loads 2 / 3.

## P1 design (after B1's results)
- Boot path: still J19b byte for byte (checked with the md5 loop against commit 9f033cc). The
  only build switch added is J19b's own music build (`-DOJ32X_MUSIC=1`, CD audio tracks 2-4).
- Screen (B1: re-anchors cost up to 28 ms, whole-cell restores 13-39 ms): one linear ring
  `lin = (y * 384 + x) mod 76800` with the existing `plat_present_scroll(pal, (cam_y + cam_x /
  384) % 200, cam_x % 384, 200)`; the 336 bytes after the ring (0x12E00, the Jazz panel's place)
  copy the ring's start each frame, for lines that run past its end. No panel; the HUD is in the
  view. A clean copy of the tile layers in SDRAM (76,800 bytes) gets only the strips that come
  into view; each frame buffer copies from it the strips new to it and exactly the rectangles
  its last frame drew sprites into (clipped to the view: outside it, places are shared).
- Effects (B1: 34 ms for the PC's 42,000 pixels): keyed, at most 12, only the game's own
  explosions / hits; no blend table (the stage pack lost ADD.LUT and the backgrounds: 829 ->
  687 KB with the light map).
- Tile lighting: Hurrican lights tiles (colour per tile, smoothed over the corners); 15 light
  colours (k-means over the map) as SHADE.LUT rows, a 4-bit light per cell (LIGHT.BIN, 8x8
  blocks deduplicated, 50 KB). Back tiles always, front tiles where the map says so.
- Game logic: Hurrican's (Player.cpp, Tileengine.cpp Block* tests, GegnerClass::Run, the
  enemies' DoKI) in Hurrican's units, fixed point, 60 steps a second (at most 6 a frame).
  Enemies wake when they reach the screen and run while within a screen of it.
- Walls shot away: a bit per map cell in SDRAM and the changed cells (the map is in the cart).
- PC checks: `make -C host asan` (20,000 frames of random input, no error), `make -C host reach`
  (the level end is reachable with the game's movement code: walk / jump searches from every
  place stood on).

## P1b: menu, level select, lives
- Menu at start (resident pictures only: the logo, the 3x5 font): START GAME, OPTIONS (LEVEL
  Stage 1-9, LIVES 1-99, BACK). Stage clear loads the next stage; game over returns to the menu.
- All 9 stage packs are built (472-913 KB each, all under the 1.47 MB stage area); the
  broken-wall bitmap grew to 36 KB for the largest map (eis, 700 x 400). SDRAM heap left: 40 KB.
- Music: tracks 2-13 (menu, clear, game over, the 9 stage songs named in the map files). A stage
  load stops the music first (no CD data reads while CD audio plays), then starts the stage's.
- Reachability (game movement, walls pre-broken): stage 1 end reached; 2-9 not (missing lifts,
  doors, switches, bosses; several start on or next to such objects).

## Not yet in P1
- The mid-boss (Stahlfaust, object 120), falling rocks (20), fans (167/168), the shrine (148),
  rain particles (140), light flares (170); other weapons, the wheel, smart bombs.
- Far background layers (planned: Genesis planes - touches the boot stack, at the very end).
- Bridges do not sag; the walker is squashed when jumped on (no waddling).

## Next
1. P1 on the console: playability, the timing line (X) in busy places, the music tracks.
2. The other stages' packs (the converter takes any stage number); boss logic.
3. At the very end: the Genesis background planes (68000 / Sub-CPU changes).

## P1c (reverted from P2)
P2 (all objects, the full player, projectiles and particles ported) ran at 4-5 fps on the
console against above 15 for P1b. P1c is P1b's code (commit 9dda38d) with only two things from
P2 added back: the pause menu (CONTINUE / MAIN MENU / TIMING ON-OFF) and the sound balance
(music rendered 10 dB lower, each sound effect normalised to the same peak, envelope 176).
P2 stays in the history (commit 0ebe8c0, branch master) for a later, cheaper port.
Disc: `sega/HurricanP1c/HurricanP1c.cue`.

## P1d
- Main menu shows the controls; OPTIONS has DIFFICULTY (Easy / Normal / Hard: Hurrican's Skill
  0-2, damage x 0.3 / 0.6 / 1.0 and the map objects' skill filter), MUSIC On / Off and DEMO (the
  chosen stage plays itself: `demo_pad()` in p1.c, no damage except falls and crushes).
- The spread shot is drawn turned to its direction (tools/hur_convert.py tag ROT16: 16 turned
  frames; p1.c Shot.rot).
- The added enemy kinds: game/k2/ (README there). 56 kinds in 4 groups (caves, factory,
  temple, ice) plus the falling rock; the engine hooks are X-macros over `K2_LIST` / `K2_CODE`.
  `cam_lock` lets a kind hold the camera (the stage 3 elevator).
- Sound effects envelope 176 -> 140 (a little loud).
- Disc: `sega/HurricanP1d/HurricanP1d.cue`.
- Open: the bosses; the boot bar fills twice at power-on (boot path - left for the end, with
  the Genesis planes); stage 3's lift is a big moving picture (about 26,000 restore pixels a
  frame on the PC count) - check its timing line on the console.

## P1e
- The controls moved from the main menu to OPTIONS - CONTROLS (menu_page 2).
- No reload of the stage already in the cart: `stage_in_cart` in load_stage (the pack at
  0x280000 stays; the SH2 only reads the cart) - the same stage again is only re-mounted.
- Disc: `sega/HurricanP1e/HurricanP1e.cue`.

## P1f
- The spread shot starts at the gun as in Hurrican (CalcFlamePos + CalcAustrittsPunkt offsets per pose and direction; P1b had rough offsets, about 40 px off upwards and 34 px off facing left).
- Disc: `sega/HurricanP1f/HurricanP1f.cue`.

## P1g (frame rate)
- One difficulty: MAP_SKILL 0 (Hurrican's easy object set), skill 1 rules (damage x 0.6).
- ENEMY_BUDGET 8 in world_activate (enemy_weight: 0 pick-ups / platforms / triggers, 2 for
  pictures over 80 x 80 full size); a kind coming into view over budget is dropped.
- Every other bat (map object 53); MAX_ENEMY_SHOTS 16; MAX_FX 6.
- Timing line: two lines, with ENEMIES / SHOTS and the HUD and WAIT parts (the 19.9 fps photo
  accounted for only 11 of 50 ms - the rest is HUD + WAIT).
- PC counts at the 4 most crowded spots of each stage: copied + sprite pixels 10-70% lower.
- Disc: `sega/HurricanP1g/HurricanP1g.cue`.

## P1h (stage 3 elevator)
- Thinned on stage 3: map objects 33, 35, 18, 37, 62 one in three, 34 one in two (stage_start).
- LIFT_BUDGET 3 while a kind holds the camera (cam_lock_held).
- STELZSACK box 20,5-80,130 (body 20,5-80,60 for touch), energy 40; LAVAKRABBE box 0,-24-70,34
  (body 0,0-70,34), energy 40: Hurrican's boxes could not be reached by the spread shot, so
  they piled up on the lift.
- PC checks: HB_DMG=1 (what hurts the player, what dies), HB_DEMO_HURT=1 (the demo player is hurt).
- Disc: `sega/HurricanP1h/HurricanP1h.cue`.

## P1i
- MAP_SKILL back to 1 (medium objects); ENEMY_BUDGET 10, LIFT_BUDGET 5; over budget an enemy
  waits (stays inactive) instead of being dropped; elevator thinning one in two (not the lava
  crabs). P1g/P1h were too empty.
- Death: the spread shot one level down (Hurrican runExplode).
- Off the held screen for 90 steps: dies (Hurrican CheckLevelExit), comes back on the lift.
- Disc: `sega/HurricanP1i/HurricanP1i.cue`.

## P1j
- OPTIONS - CREDITS: game/credits.c (plasma: OpenJazz's own via game/ojsin.h, GPL-2.0, and Sonic
  1+1's 18 patterns into CLEAN with palette 160-223 turned; credits in menufont.png; the CD
  player: Left / Right, A play (CMD 45), B stop (CMD 54), C plasma, Up / Down speed (not shown),
  Start back). Track 14 = credits.it.
- Attract mode: 60 s idle in the menus -> the demo on a random stage, back to the main menu.
- Demo exit fix: the frame's pad_prev is the player's own pad (the demo's held B / Right / C
  hid those presses).
- Splash: tools/make_splash.py centres the logo (visible part) and text on the loader's bar
  (x 146); the B1 text line is gone.
- Disc: `sega/HurricanP1j/HurricanP1j.cue`.

## P1k
- Player weapons (game/p1.c): spread / laser / bounce (X), lightning + beam (A, A+B), grenades
  (Y), power line (Z), smart bomb (Up+Z), wheel (Down+Z or Down+A); full at every life.
- Bosses: game/k2/b1.c - b4.c (engine: boss_lock, boss_bar, music_boss, Kind.boss, CAM_DY).
- Air vents: game/k2/vents.c (map object 140, Value1 5/6 with Value2 0, 19).
- HUD after the original (numbers and short names).
- OPTIONS - STORY: game/story.c, tools/hur_story.py -> OJ010.PAK (any OJnnn.PAK is opened by
  name; no Sub-CPU change), intro.it = track 24.
- Loading screen: tools/make_splash.py (Hurrican's star field, spiral, LOADING, loading.png) +
  tools/splash_bar_colours.py (palette 253-255 of IMAGE.RAW). Levels-only would need the
  boot-path platform code (IMAGE.RAW is read by both).
- Disc: `sega/HurricanP1k/HurricanP1k.cue`.

## P1l
- Attract mode (60 s idle on the title): one recorded demo at random, never the same twice in a
  row (game/demo_data.h - stage 1: three, stage 2: three). OPTIONS - DEMO: the demo robot plays the
  stages in turn from LEVEL. A recorded demo: DemoRec {stage, start x/y, seed, steps, run-length
  pads, a checked place every 60 steps}; p1.c runs camera_follow + world_activate per step while
  one plays, saves/restores rng_state around the drawing, resets enemy_load / camera locks /
  pl_off_t in stage_start, memsets pl at the start. Off its path (> 64 px) -> the demo ends.
- game/demorec.c (make -C host demorec): HB_MAPIMG (map picture), HB_BLK / HB_ENTS (tiles and
  objects of an area), HB_PATH (path search, prints a route of moves), HB_ROUTE (records: moves
  tried in a fork()ed copy; way points by a look-ahead search over 8 button patterns, 3 deep,
  potential = distance along the rest of the route), w:n pauses, HB_PLAY (replay, places per
  second). tools/make_demo_data.py -> game/demo_data.h.
- Laser: rotated frames (ROT16, non-square pictures padded to a square), the middle beam on the
  barrel line (the big spread shot's centre).
- Story: the picture change done per step (the per-frame fade never reached 0 on the console).
- Credits: 12 px/s, creditfont.png (menu font 11x11, hard edges), no band, Y hides all but the
  track line.
- Menu: START GAME / OPTIONS centred under the logo; "HOLD: FASTER" removed.
- Loading screen: the spiral in the middle, the frame at (9,9,9) so the edges cut cleanly.
- Disc: `sega/HurricanP1l/HurricanP1l.cue`.
- P1l update: the title and menus on black (palette 0 = 0x0400); OPTIONS has no DEMO entry any
  more (the recorded demos are the attract mode only; the demo robot stays for PC checks,
  HB_DEMO=1); bats: at most MAX_BATS (5) awake near the screen (world_activate waits the others),
  the last boss's bat storm adds one only while fewer than 5 are awake.

