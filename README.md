# Hurrican for Sega CD + 32X (Hurrican CD32X)

Hurrican (2007), the freeware Turrican-style run-and-gun by Poke53280, on the **Sega CD32X**.
The engine was rebuilt from scratch for the 32X: the PC game uses floating point and the 32X's
SH2 processors have none, so Hurrican's rules, enemies and bosses were rewritten in fixed point.
The whole game runs from one CD. Each stage is loaded from the disc into a 4 MB RAM cart, with
CD music, sound effects and a high-score table.

> **No game data is included.** The build converts Hurrican's own data, which Poke53280 released
> as freeware with the source code. Download it yourself; see *What you need*.

## Hardware
- Sega Genesis / Mega Drive
- Sega CD / Mega-CD
- Sega 32X
- **A 4MB RAM cart** in the 32X cartridge slot (required)
- A **6-button pad** (recommended). With a 3-button pad, the grenade, power line and weapon
  change (X, Y, Z) are missing, and Down + A rolls the wheel.

Emulators: Kega Fusion cannot emulate the 4MB RAM cart, so the game is made for real hardware.

## What you need
| What | Where it goes |
|---|---|
| Hurrican's source and data: `Hurrican-master.zip` from https://github.com/HurricanGame/Hurrican (Code → Download ZIP) | `HurricanCD32X/` (unpacked into `HurricanData/` on the first build) |
| Chilly Willy's Sega toolchain (`/opt/toolchains/sega`), Python 3 with numpy and Pillow, gcc | your build machine (Linux / WSL) |

The music needs nothing extra: the build turns Hurrican's own music modules into CD audio tracks.

## Building
Full instructions: `docs/HURRICAN_CD32X_HANDOFF.md`. In short:
1. Put `Hurrican-master.zip` in the `HurricanCD32X` folder.
2. `cd HurricanCD32X && bash tools/make_disc.sh`
3. **Burn `HurricanCD32X/discs/<name>/<name>.cue`** (with its `.bin`). The build prints the
   exact path when it finishes. The `.iso` alone has no music.

## Starting the game
Put the RAM cart in the 32X, the disc in the Sega CD, and switch on. A loading picture with a
bar appears while the game is copied into the cart, and again while each stage loads from the
disc. Then the title menu appears; a minute without a button starts a recorded demo.

## Controls (6-button pad)
| Button | Action |
|---|---|
| **Left / Right** | walk |
| **Down** | duck |
| **Up** | aim up; with Left / Right: aim diagonally (standing, as in Hurrican) |
| **B** | shoot (hold for continuous fire) |
| **C** | jump (hold for a higher jump) |
| **A** | lightning (Left / Right turn it); **A + B**: charge the beam, release to fire |
| **X** | next weapon: spread, laser, bounce |
| **Y** | grenade |
| **Z** | power line; **Up + Z**: smart bomb |
| **Down + Z** (or **Down + A**) | the wheel (B lays bombs, C turns back) |
| **Start** | pause |

**Pause menu:** CONTINUE, MAIN MENU, TIMING (frame time on screen), 2ND CPU (the second SH2
helps with each frame; on is smoother), BACKDROP (the Genesis far background).

## Menus
- **START GAME**: from the stage and with the lives set in OPTIONS.
- **OPTIONS**: LEVEL (stage 1–9), LIVES (1–99), MUSIC (on / off), CONTROLS, STORY (Hurrican's
  intro with its pictures and song), HIGH SCORES, CREDITS (with a player for the disc's music).

## Saving
The high-score table (the best ten: name, score, stage reached) is kept in the Sega CD's backup
RAM as a small file, `HURRICAN_HS` (2 blocks), so it survives power-off; the BACKDROP setting
is kept in it too. If the backup RAM is
full, the table says NOT SAVED; free some space with the Sega CD's memory manager (switch on
without a disc). A score good enough asks for a name after the game: Up / Down
choose the letter, Left / Right the place, B the next letter, Start done. There are no saved
positions; LEVEL in OPTIONS starts any stage.

## What is in this version
- All 9 stages, from the jungle to the final boss, with their bosses.
- All weapons: spread, laser and bounce shots at every level, the lightning and beam, grenades,
  the power line, smart bombs and the wheel.
- CD music: Hurrican's own soundtrack, a track for each stage, each boss, the menus and the story.
- Sound effects from the original game.
- Hurrican's story intro, its credits, and 6 recorded attract demos (stages 1 and 2), one at
  random.
- A high-score table in the Sega CD's backup RAM.
- Both 32X processors work on each frame.
- A far background on the Genesis layer behind the 32X picture in stages 1 and 7, with a colour
  fade; on at start (PAUSE → BACKDROP turns it off) and it costs no frames.

## Known limitations
- About 20 frames per second.
- The stage 3 elevator boss is replaced by a 45-second stream of stilt walkers and boulders: the
  boss was too big for the frame rate.
- Fewer enemies at once than the PC game (at most 2 bats and 4 bees, a limit on enemies on
  screen); enemies far from the player move at half rate.
- Hurrican's tree and ice layers between the far background and the level are not drawn. Only
  stages 1 and 7 have the Genesis backdrop: the other stages' walls cover the sky.
- One player only.

## Credits
- **Hurrican**: Poke53280. Jörg "Eiswuxe" Winterstein (programming, game design, sound, music,
  levels) and Thomas "Turri" Schreiter (graphics, game design, enemies, levels):
  https://www.winterworks.de/project/hurrican/
- **Hurrican SDL port**: Pickle136, Stefan "thrimbor" Schmidt, Leandro "drfiemost" Nini:
  https://github.com/HurricanGame/Hurrican
- **Sega CD / 32X boot and command framework**: from the OpenLara CD32X and OpenJazz CD32X ports,
  built on the Kobo Deluxe CD32X project and Chilly Willy's Sega CD and 32X examples and toolchain
- **D32XR 32X code**: Victor Luchits (viciious)
- **4MB 32X RAM cart**: Victor Luchits, Joseph Fenton, Leo Oliveira, Tiido Priimägi and Eric Witt
- **OpenJazz** (the credits plasma): AlisterT and contributors
- **libxmp-lite** (music conversion): the libxmp authors
- Hurrican CD32X port: MicroNut99

Licences: Hurrican's code and assets were released by Poke53280 for free use (not for sale); its
original source files are under the MIT licence. OpenJazz is GPL-2.0. See each upstream project's
own licence.
