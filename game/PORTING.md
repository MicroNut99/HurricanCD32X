# Porting Hurrican's code to the CD32X game

The CD32X game (`game/p1.c`) is a C engine that runs Hurrican's own rules. The parts of
Hurrican's C++ source (`/tmp/claude-0/hur/Hurrican-master/Hurrican/src`) are ported into C files
that the engine includes. Read `game/hur.h` first: it has the porting rules and every engine
function you may call. The examples `game/en/base/diamant.c`, `game/en/base/walker.c`,
`game/en/base/endlevel.c` show the style.

## The target hardware: keep it lean
- The game runs on a 23 MHz SH2 with no FPU: fixed point only (`fx`, 8.8). No float, no double,
  no `sqrt`, `sin`, `atan2` from libm (use `isqrt`, `sin_d`, `cos_d`, `atan2_d`). No 64-bit
  arithmetic in inner loops (FMUL / FDIV are fine now and then).
- SDRAM is tight (about 40 KB left): no big arrays. An object's own state goes into its `Ent`
  (`f[8]` fx, `i[4]` int32). A boss may keep one small static struct (< 1 KB).
- Drawing is the costliest part: draw what Hurrican draws, but no extra glow layers, no
  additive copies, at most one sprite per real part. Rotated sprites: the plain frame.
- The screen is 640 x 400 of Hurrican's 640 x 480 window (the lower 400; `RENDERHEIGHT` is
  480 as in Hurrican for the game logic).

## Hurrican -> C (in short; the full list is at the top of hur.h)
| Hurrican | here |
|---|---|
| `class GegnerX : GegnerClass`, constructor, `DoKI`, `GegnerExplode`, `DoDraw` | `x_init`, `x_ki`, `x_explode`, `x_draw` and `TYPEDEF(ID) = { init, ki, explode, draw, load };` |
| the object number(s) a class serves: see `GegnerListClass::PushGegner` in GegnerClass.cpp | one `TYPEDEF(ID)` per number (several may share functions) |
| member variables beyond GegnerClass's | `e->f[n]` (float), `e->i[n]` (int/bool/pointer-as-slot-index); list them in the file's header comment |
| `xPos`, `ySpeed`, `Energy`, `AnimPhase`, `Handlung`, `blocku`... | the same names in `Ent` (fx where float) |
| `BlickRichtung = DirectionEnum::LINKS` | `e->blick = -1` (RECHTS 1) |
| `Timer.sync(x)` | `SYNC(x)` (x fx) |
| `GetRandom(n)` | `GetRandom(n)` |
| `Player[0]`, `pAim`, `pParent` | `pl` (fields with Hurrican's names: `pl.xpos`, `pl.Handlung`, `pl.Energy` ...) |
| `pAim->DamagePlayer(x)` | `DamagePlayer(x, 0)` |
| `TileEngine.XOffset` | `cam_xoff()` |
| `Gegner.PushGegner(...)` | `gegner_push(x, y, ART, v1, v2)` (returns `Ent *`, never NULL) |
| `Projectiles.PushProjectile(x, y, ART)` | `proj_push(x, y, ART)` (`WinkelUebergabe = w` -> `proj_winkel = FX(w)`) |
| `PartikelSystem.PushPartikel(x, y, ART)` | `part_push(x, y, ART)` |
| `SoundManager.PlayWave(.., SOUND::X)` | `snd(SOUND_X)` |
| `pGegnerGrafix[GegnerArt]->RenderSprite(...)` | `draw_sprite(&GegnerPic[e->GegnerArt], frame, x, y, mirrored)` (world coordinates) |
| extra textures a class loads (`DirectGraphicsSprite X; X.LoadImage("file.png", ...)`) | `static Pic P_x;` filled in your type's `load` function with `pic("file.png")` |
| other enemies (`for (auto& enemy: Gegner.enemies)`) | `for (k = 0; k < MAX_LIVE; k++) { Ent *o = &ents[k]; if (o->GegnerArt < 0) continue; ... }` |
| a pointer to another object kept over time | its slot index `o - ents` in an `i[]` field (check its `GegnerArt` before use) |
| boss music / screen locking / boss bar | `music_boss()`, `music_stage()`, `scroll_level()`, `tile_zustand()`, `boss_bar(e->Energy, max)` (call boss_bar every step while the boss's bar shows) |
| `StageClear()` | `stage_clear()` |

Not available (leave out, say so in the file header): lighting, glows, colour fades, screen
shaking, text messages, the tutorial, two players, the time limit.

## Files
- Objects: one C file per Hurrican source file, in your group's folder `game/en/<group>/`, named
  like the source (`gegner_stahlmuecke.c`). Every function and static variable name starts with
  the file's own prefix (all files end up in one translation unit).
- The player: `game/player_full.c`; projectiles: `game/proj_full.c`; particles:
  `game/part_full.c` (each replaces the first versions `player.c` / `proj.c` / `part.c` when
  done - do not edit those).
- Do not change: `game/p1.c`, `game/hur.h`, `game/ents.c`, `game/hur_enums.h`,
  `game/hur_tables.h`, `port/`, `cd32x/`, `mars/`, `tools/` (if you need something from the
  engine that is not there, write a small helper in your own file, or note it in your report).

## Building and testing (PC)
The packs of all 9 stages are in `/tmp/claude-0/p1pk`. Build with only your own folder so
other groups' unfinished files do not get in your way, into your own output folder:

    cd /tmp/w/HurricanCD32X/host
    make OUT=/tmp/claude-0/b_<group> EN="../game/en/base ../game/en/<group>" asan
    HB_DIR=/tmp/claude-0/p1pk HB_STAGE=<n> HB_WARP=<x>,<y> HB_OUT=<frames folder> HB_SAVE_EVERY=10 \
      HB_FRAMES=600 HB_GOD=1 HB_TRACE=1 HB_PAD="0:-,20:RIGHT+B" /tmp/claude-0/b_<group>/hurrican_p1_asan

(`game/en/base/` holds the examples; `EN` folders are searched with their subfolders.)
`HB_STAGE` starts that stage directly, `HB_WARP` puts the player at x,y (Hurrican pixels),
`HB_GOD=1` no damage, `HB_PAD` the buttons by frame (UP DOWN LEFT RIGHT A B C X Y Z START, `+`
joins, `-` none), frames are saved as PPM (convert with Python/PIL to look at them).
The player / projectile / particle ports build with
`PORTS="-DPLAYER_C='\"player_full.c\"'"` (or `PROJ_C` / `PART_C`) added to the make line.
The map's objects with their positions: `tools/hur_convert.py`'s `read_map` (Python) or the
OBJECTS.BIN in a pack.

Check every object type you port in a stage that has it: it appears, moves and acts as in
Hurrican, can be killed (if Hurrican's can), no address-sanitizer errors. Also compile for the
console once at the end: `make -C /tmp/w/HurricanCD32X/mars` is NOT for you (shared); instead:

    /opt/toolchains/sega/sh-elf/bin/sh-elf-gcc -m2 -mb -O2 -std=gnu11 -fsyntax-only -Wall \
      -DOJ32X -DOJ32X_MARS -I/tmp/w/HurricanCD32X/port -I/tmp/w/HurricanCD32X/game \
      -I/tmp/claude-0/b_<group>/gen /tmp/w/HurricanCD32X/game/p1.c

## Report
At the end, report: the files written, the object numbers covered, what was left out or
simplified (and why), any engine function you missed, any shared global you had to invent.
