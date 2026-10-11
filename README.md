# Hurrican CD32X

Work towards **Hurrican** (the freeware Turrican-style game by Poke53280, open source as
`HurricanGame/Hurrican`) on the **Sega Mega Drive / Genesis + Mega-CD / Sega CD + 32X** with a
**4 MB RAM cart**, built on the boot, cart and Sub-CPU layers of the OpenJazz CD32X port.

The feasibility study (`docs/CD32X_FEASIBILITY_HURRICAN.md`) found that the PC engine cannot simply
be recompiled (its game logic is floating point; the SH2 has no FPU), and that a rebuilt engine
could reach Jazz-like frame rates *if* the 32X can draw Hurrican's scenes fast enough. B1 measured
it on the console; P1 is the first playable build.

## P1o: all stages, all weapons, the bosses, recorded attract demos

The game starts at a menu:

- **START GAME** - from the stage and with the lives set in OPTIONS.
- **OPTIONS**
  - **LEVEL**: Stage 1-9 (level select).
  - **LIVES**: 1-99 (Left / Right, hold for faster; Left from 1 goes straight to 99).
  - **MUSIC**: On / Off.
  - **CONTROLS**: the buttons (the table below).
  - **STORY**: Hurrican's intro - its six pictures, the story typed out line by line in green,
    its song (CD track 24); any button returns.
  - **CREDITS**: Hurrican's credits and the port's, scrolling over a plasma (OpenJazz's own and
    Sonic 1+1's 18 colour-cycling patterns), with a music player for the disc's CD tracks (their
    song titles from the modules). Left / Right choose a track, A plays, B stops, C the next
    plasma, Up / Down the plasma's speed, Start back. No pause: the unchanged Sub-CPU program
    has play and stop only.
  - **BACK**.

Up / Down choose, Left / Right change, B, C, A or Start select. A minute in the menus without a
button starts the attract mode: one of the recorded demos, picked at random (three of stage 1,
three of stage 2, up to about a minute each, made along the routes of recordings of the original game);
any button returns. The recorded demos are not in the menu. After a stage is cleared the
next one loads; after game over (or the last stage) the menu returns. Every life starts with all
weapons at their top level and Hurrican's stock of grenades, power lines and smart bombs.
Playing the stage loaded
last again (after game over or MAIN MENU) starts at once: its pack is still in the RAM cart, so
it is not read from the disc again.

| Button | |
|---|---|
| **Left / Right** | walk |
| **Down** | duck |
| **Up** | aim up; with Left / Right: aim diagonally |
| **B** | shoot (hold for continuous fire) |
| **C** | jump (hold for a higher jump) |
| **A** | the lightning (Left / Right turn it); **A + B**: load the beam, let go to fire |
| **X** | next weapon: spread, laser, bounce |
| **Y** | grenade |
| **Z** | power line; **Up + Z**: smart bomb |
| **Down + Z** (or **Down + A**) | the wheel (a ball; B lays bombs, C turns back) |
| **Start** | pause: CONTINUE / MAIN MENU / TIMING ON-OFF |

X, Y and Z need a 6-button pad; with a 3-button pad Down + A rolls the wheel.

- The player, Hurrican's map blocks (walls, platforms, damage tiles, water, walls that can be
  shot away), the spread shot (more shots per level, up to 7; drawn turned the way it flies),
  diamonds, 1-ups, the hidden power blocks, rope bridges, moving platforms, secret areas and the
  level end.
- The enemies and traps of all 9 stages: P1b's 20 kinds in `game/p1.c` and 56 more in
  `game/k2/` (bats, cannons, climbing and fire spiders, egg layers, drones, wall and ceiling
  crabs, mutants, cockroaches, wasps, caterpillars, rocket launchers, boulders, bombs, stilt
  walkers, lava crabs and lava men, side rockets, ski walkers, mini dragons, icicles, fish,
  slime balls, presses, stamping stones, fire traps ...), the elevator of stage 3 (the screen
  holds it, as in Hurrican), the floating platforms, lifts and mushrooms. `game/k2/README.md`
  says how a kind is added. They are deliberately lean (no particles, glows or smoke): P2 ported
  all of Hurrican's object code with its whole framework and ran at 4-5 fps on the console.
- Kept lean for the frame rate: one difficulty - Hurrican's medium (its objects, damage x 0.6);
  at most 10 enemies at once (a big one counts twice; the next ones wait and come in as these
  are shot); every other bat; at most 16 enemy shots and 6 explosions at once.
- The stage 3 elevator: every other boulder, falling bomb, eye, side rocket and stilt walker; at
  most 5 enemies at once while the lift runs. Left behind by the lift (off the screen for 1.5 s),
  the player dies and comes back on it, as in Hurrican.
- The bosses of all 9 stages (`game/k2/b1.c` - `b4.c`): the steel fist, the pharaoh head, the
  big fish, the elevator boss, the golem, the spider machine, the giant spider, the dragon, the
  UFO, the snow king, the Rollmops, the enemy storm, the metal head, Skeletor and the wall - each
  locks the screen on its arena, plays its boss song (CD tracks 15-23), shows its energy bar.
- All of Hurrican's weapons: spread, laser and bounce (8 levels each), the lightning and its
  beam, grenades, power lines, smart bombs and the wheel.
- The air vents (rising smoke that lifts the player to higher platforms) are drawn as smoke
  columns and lift as in Hurrican.
- The HUD as the original's, in numbers and short names: energy, lives, score, gems; the four
  weapons with their levels (the selected one in yellow), grenades / power lines / smart bombs.
  The stilt walkers and lava crabs on the lift are hit by straight shots (Hurrican's boxes were
  out of the spread shot's reach - its player had upward weapons) and have half their energy.
- Not in this build: the far background layers (backdrop colour instead; planned for the
  Genesis planes), particles, glows and turned pictures.
- The loading screen after Hurrican's own (its star field and spiral, LOADING, the bar in the
  original's colours) with the logo on top, centred on the loader's progress bar (the bar is the
  boot path's, x 56-236; tools/make_splash.py and tools/splash_bar_colours.py). The same picture
  shows at power-on and while a stage loads.
- Sound: the sound effects normalised to one level (envelope 140); the music, CD audio tracks
  rendered from Hurrican's modules at build time by the bundled libxmp-lite (2 menu, 3 stage
  clear, 4 game over, 5-13 the songs of stages 1-9, 14 the credits, 15-23 the bosses, 24 the
  story), 10 dB lower so the effects are heard. The
  music stops while a stage pack is read from the disc.
- The rules are Hurrican's own, in its own units (the PC game's 640 x 480 pixels, 60 steps a
  second, fixed point); the screen shows them at half size. The tile lighting of the map
  (Hurrican lights each tile) is kept: caves are dark, walls stand out.
- Music: CD audio tracks rendered from Hurrican's modules at build time by the bundled
  libxmp-lite: 2 menu, 3 stage clear, 4 game over, 5-13 the songs of stages 1-9. The music
  stops while a stage pack is read from the disc.

The timing line (pause menu, TIMING ON), two lines: `FPS 19.9  ENEMIES 6  SHOTS 3` (frames per
second, enemies at once, enemy shots), then `AI6 MAP0 CPY3 SPR2 OVR1 HUD1 WAIT30` - the milliseconds
of each part of a frame, as below, plus HUD (the HUD, the ring's end copy and handing the frame
over) and WAIT (waiting for the last frame to reach the screen). Before P1g: `FPS 14.8 AI 12 MAP 3 CPY 4 SPR 6 OVR 1` - frames per second and the
milliseconds a frame spends on the game logic (AI), on the tile strips that came into view
(MAP), on copying them and the places sprites were (CPY), on the sprites (SPR) and on the
walls over them (OVR).

### How the screen is drawn

The Jazz port's scrolling screen (J19b `plat_present_scroll`), used as one linear ring: world
pixel (x, y) lives at `(y * 384 + x) mod 76800` of the frame buffer's ring, so the 320 x 200 view
never overlaps itself and the camera never has to re-anchor (B1's spikes). The whole 200 lines
are the view (the HUD is drawn into it, as Hurrican does). A frame makes only the tile strips
that came into view (into a clean copy in SDRAM), copies them and exactly the rectangles the
buffer's last frame drew sprites into, draws the sprites, and redraws wall tiles only where a
sprite is under one. Effects are keyed, and fewer (at most 12 at once).

## The boot path is the Jazz port's, unchanged

Everything from power-on to the start of the game is byte for byte the OpenJazz port's
**J19b** build, the last disc proven on the console: `cd32x/` (Sub-CPU program, 68000
listener, SH2 loader), `port/` (start-up code, linker script, platform layer, C++ start-up
checks), `tools/make_oj_rom.py` and `tools/make_boot_image.py`, compiled with the J19b engine's
flags and linked the same way. The program therefore carries the J19b module name
("OpenJazz 32X") and goes on the disc as `OPENLARA.32X`. The disc holds the same kinds of files
as the Jazz disc at boot (splash, program, resident pack `IMAGE8.RAW`, sound pack `SFX00.BIN`
made from Hurrican's sounds). B1's benchmark started at `main()` and only used the platform
layer's existing calls; its screen was the Jazz port's scrolling screen (167-line view, 33-line
panel, ring 384 x 200).

P1 keeps that: the game (`game/p1.c`) starts at `main()` and only uses the platform layer's
existing calls (`plat_present_scroll` with a 200-line view, `plat_sfx`, `plat_music`, ...). The
only build switch it adds is the one the J19b engine build used for music
(`-DOJ32X_MUSIC=1`, CD audio tracks).

## Loading, organised for short loads

The lesson from the Jazz port is built in from the start:

| Cart area | Holds | Loaded |
|---|---|---|
| `0x000000` | the program | at power-on (skipped when already in the cart) |
| `0x100000` | **resident pack** `IMAGE8.RAW`: every picture the game loads at start-up - player, HUD, fonts, menus, shots, explosions, common enemies | once (skipped when already in the cart) |
| `0x280000` | **stage pack** `OJ001.PAK`-`OJ009.PAK`: only what the stage adds - tiles, map, light, its enemies, objects, palette | per stage |
| `0x3F0000` | saves | |

- Nothing is in two packs: what more than one stage uses is resident.
- Nothing raw: pictures are stored as the 32X draws them (halved, 256 colours, sprites packed
  as runs of visible pixels), maps block-compressed (stage 1: 2.0 MB of map file -> 216 KB).
- The colours: 1-96 are the resident pictures' (the same in every stage), 97-255 the stage's.

## Building

Needs:
- the Sega toolchain in `/opt/toolchains/sega` (sh-elf and m68k-elf gcc 12.1 with newlib, as
  for the OpenJazz CD32X port - the "sega-toolchain-12.1" setup), make, gcc for the PC tools;
- Python 3 with numpy and Pillow;
- Hurrican's data: `Hurrican-master.zip` (the source download of `HurricanGame/Hurrican`) next
  to this folder - `tools/make_disc.sh` unpacks `Hurrican/data` and `Hurrican/lang` from it - or
  `HURRICAN_DATA=<Hurrican/data>` with `lang/` beside it. None of Hurrican's pictures, maps or
  music are in this repository.

    HURRICAN_DATA=<Hurrican's data folder> ./build.sh 2>&1 | tee build_log.txt

or `tools/make_disc.sh`, which also takes the data from `Hurrican-master.zip` next to this folder
and copies the disc to `sega/HurricanP1o/`. The disc: `cd32x/subcpu/HurricanP1o.cue` / `.bin`
(data track + 23 audio tracks).

The recorded demos (`game/demo_data.h`): the buttons of every 60 Hz step. In a demo the world is
run step by step (the camera and the enemies' wake-up each step, the random numbers from the
demo's seed, the drawing leaves them alone), so the replay is the same at any frame rate and on
the PC and the 32X; once a second the player's place is checked against the recording and a demo
that ever went off its path just ends. They are made on the PC by `game/demorec.c`
(`make -C host demorec`): `HB_PATH` finds a way through a stage with the game's own movement code,
`HB_ROUTE` then plays it with enemies (looking ahead in fork()ed copies of the game where a found
move fails), and `tools/make_demo_data.py` writes the header. Made with the stage packs of the
Hurrican data in `Hurrican-master.zip`; other data would need the demos made again.

PC checks of the same game code (`game/host.c` stands in for the 32X):
- `make -C host`: the game on the PC, frames saved as pictures (see `host/Makefile`);
  `make -C host asan`: the same with the address and undefined-behaviour checkers;
- `make -C host reach`: a search over the places the player can reach with the game's own
  movement code - stage 1: the level end is reachable.

## Licence and credits

- **Hurrican** - Poke53280 (Eiswuxe); SDL port and further work by Pickle136, Stefan Schmidt
  (thrimbor) and Leandro Nini (drfiemost). The code is MIT; the game's content is copyright by
  its author and is not part of this repository - the disc is built from the data you supply.
  Distributing a disc with Hurrican's content needs the author's written permission
  (Hurrican/readme.txt).
- **OpenJazz CD32X port** - the boot path, cart and Sub-CPU layers (`cd32x/`, `port/`), taken over.
- **CD32X boot** - from the OpenLara (Tomb Raider) CD32X port; **D32XR** 32X code by Victor
  Luchits; **Sega CD / 32X framework** by Chilly Willy.
- **Plasma** (OPTIONS - CREDITS) - OpenJazz's credits plasma (`game/ojsin.h`, its formula in
  `game/credits.c`; GPL-2.0) and Sonic 1+1's colour-cycling patterns. A disc containing it falls
  under GPL-2.0.
- **libxmp-lite** (`tools/libxmp-lite`, LGPL 2.1, see its COPYING.LIB) - renders the music
  modules to WAV at build time; nothing of it is on the console.
- **Hurrican CD32X** - Micronut99.
