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
- **A 4MB RAM cart** in the 32X cartridge slot (required), or the **8 MB / 16 MB cart**
  ([16MB-PCB-CD32X](https://github.com/MicroNut99/16MB-PCB-CD32X)) with the `CART=8` / `CART=16`
  build (see *Loading*)
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
4. For the 16 MB cart: `CART=16 bash tools/make_disc.sh` (disc `discs/HurricanP2a2-16MB/`;
   `CART=8` for the 8 MB cart).

## Starting the game
Put the RAM cart in the 32X, the disc in the Sega CD, and switch on. A loading picture with a
bar appears while the game is copied into the cart (see *Loading*). Then the title menu appears;
a minute without a button starts a recorded demo.

## Loading
Every stage is a pack of 0.4-1 MB on the disc. The game plays a stage from the RAM cart, so the
pack must be copied there first.

### The preload (8 MB / 16 MB cart)
With the bigger cart, the first boot copies the whole game into it, so later no stage has to come
from the disc:
- **What:** all nine stages and the story, 6.8 MB, in play order. The 16 MB cart has room for all
  of it; the 8 MB cart takes what fits, and the rest loads from the disc when it is played.
- **When:** once, on the first boot with a blank cart. It adds about
  30 seconds: the first boot takes ~95 s to the menu (measured in the ares emulator). A stage
  already in the cart from an older build is not copied again; it is checked against the disc
  when it is played and replaced if it changed.
- **On screen:** "COPYING TO THE CART - START: SKIP" under the bar on the title picture.
- **Start** stops it. The stages not copied yet are copied the first time they are played.
- **Later boots** see that everything is there: no copying text, nothing copied, the menu as fast
  as a normal boot (~30 s).
- **Playing a stage** from the cart takes about 2 seconds (the check below), no loading picture.
- **Another game in the same cart** (OpenJazz CD32X uses the cart the same way): when its data
  fills the pages, the preload stops where there is no room. A stage that is not in the cart is
  read from the disc when it is played and takes the page used longest ago, so the cart fills
  with Hurrican again as you play.

### The loading screens
- **The title picture with a bar:** the boot. The game program and its common pack (menus,
  fonts, sprites, sounds) are copied into the cart.
- **"COPYING TO THE CART - START: SKIP" under the bar** (8 MB / 16 MB cart): the preload above.
  Each pack goes into one of the cart's extra 2 MB pages, with a note of what it holds at the end
  of the page. Shown only while something is copied (P2a2: a later boot no longer flashes it).
- **The loading picture with a bar between stages:** the stage is read from the disc. This
  happens:
  - always with a 4 MB cart;
  - with an 8 / 16 MB cart, only when the stage is not in the cart (copying skipped, or the
    cart was used by another game meanwhile).
- **No loading picture:** the stage is in the cart. The game checks it in about 2 seconds:
  - the first and last 32 KB are compared with the disc, so a disc from a newer build replaces
    an old copy;
  - a checksum shows whether another game wrote over the page.

  Then the stage starts.

### Fast loading
The Sega CD reads the disc at 150 KB a second, but the old loading reached only about 41 KB a
second. It stopped the drive after every 32 KB and sent it back to the next sector, so the
drive had to find its place on the disc again for every piece.

Now the drive reads each file straight through:
- The Sub-CPU keeps taking the sectors as they arrive, even while the 68000 copies the last piece
  into the cart. They wait in a 64 KB buffer instead of being lost.
- Each sector's number is checked. If one is missed, the drive goes back for it.

Loading runs at 104-145 KB a second, close to the drive's own speed. Measured in the ares
emulator, not yet on a console:

| | Before | Now (fast loading) |
|---|---|---|
| a stage from the disc (stage 1) | 14.6 s | 5.3 s |
| the common pack at boot | 37 s | 15 s |
| first boot to the menu, blank 16 MB cart | ~65 s | ~95 s, with all stages copied |
| later boots to the menu | ~31 s | ~30 s |
| a stage already in the cart | - | ~2 s |

With a 4 MB cart, every stage still loads from the disc, but in about a third of the time.
With a 16 MB cart, after the first boot no stage loads from the disc at all. The extra 30
seconds of the first boot are spent once.

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
