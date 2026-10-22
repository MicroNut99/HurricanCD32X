# 8 MB and 16 MB cart releases (design)

Hurrican CD32X on the paged SRAM cart
(the 16MB-PCB-CD32X board, rev 0.5). The mechanism is
the one designed for Jazz, the reference:
[OpenJazzCD32X-REV `docs/CART_8MB_16MB.md`](https://github.com/MicroNut99/OpenJazzCD32X-REV/blob/main/docs/CART_8MB_16MB.md).
It covers the home and extended pages, the page tails, the board probe and saves with W1 = 1.
The boot-stack side is the shared proposal in the docs repo (CMD 68, variant B: no listener
change). This file gives Hurrican's own
numbers and changes.

**Status:** design only. The pack sizes are *measured* (P2a packs, `cd32x/subcpu/ROMS`).

**Reviewed:** an independent review (Claude Fable, 19 Oct 2026) corrected the common pack's
end, the home slot, CMD 38's destination and the list of files to change; fixed here.

## The 8 MB and 16 MB test builds (built, not yet run on a console)

```
cd /mnt/s/KoboPort/sega-toolchain-12.1/sega/HurricanCD32X && \
CART=16 bash CART/tools/make_disc.sh
```

- **The source** is this repository (the P1q line), unpacked into `HurricanCD32X\CART\`, so it
  uses the title's `HurricanData`.
- **The disc** goes to `discs\HurricanP1q-16MB\`. `CART=8` gives `HurricanP1q-8MB`.
- **Without `CART`** it is the 4 MB P1q build, unchanged.

The mechanism is OpenJazz R5's (`port/cartcache.c`, the same file with the title "HURR"):
- **The common pack** sits at 0x060000. The engine (352 KB) must end before it, and the build
  checks that.
- **The board probe** runs at the first stage load.
- **Each stage pack, and the story,** go into pages 2-3 (8 MB) or 2-7 (16 MB), with a record in
  each page's tail.
- **A stage is used again** after its disc compare and checksum pass. When no page has room,
  the page used longest ago is cleared.
- **On a 4 MB cart** the home slot at 0x200000 is used, as before.

**The test builds do not use the home slot as a cache.** On a 4 MB cart it stays the fallback.
So the 8 MB build keeps about 5 stages (4 MB of pages), not the 9 of the plan below; the 16 MB
build keeps all 9 and the story.

**Reviewed (Claude Fable, 19 Oct 2026), and fixed:**
- **The common pack would not have loaded on the console.** The SH2 asked for file 4 / 3
  instead of IMAGE8.RAW (file 8). The PC model never runs that path, so it could not show it.
- **Packs now start on 32 KB boundaries.** The Sub-CPU's compare (CMD 47) sets the 1 MB bank
  once per 32 KB chunk, so a chunk crossing a bank would have read the wrong memory and cleared
  good pages.
- **A damaged page record can no longer point past the page.**
- **Hurrican: the story resets the stage lookup.** The story's load can clear the stage's
  page, so the stage is looked up again afterwards.

**Checked on the PC** (`make -C host CART=16`, then `HB_CART_MB=4|8|16` for the modelled board
and `HB_CART_FILE=<file>` for the pages between runs):
- **Stage 1 from a cached page:** the same pictures as the 4 MB build, and a second run took it
  from the cart.
- **8 MB:** all 10 packs loaded in turn, with the oldest page cleared when the cart was full.
  The 3 packs that stayed came from the cart in the next run.
- **The engine** builds for CART=8 and 16 (the 4 MB build has none of the new code), and the
  Sub-CPU compiles with CMD 68.
- **Not run here:** the whole disc (Hurrican's data is not in this workspace).

## 1. What changes for the player

| Cart | Stages in the cart | Loading |
|---|---|---|
| 4 MB (today) | the stage being played | every stage change loads its pack (12-23 s at the measured 42 KB/s) |
| 8 MB | all 9 stages | none after each stage's first load (or after INSTALL TO CART, about 3 min) |
| 16 MB | all 9 stages and the story | none at all after the first load |

## 2. The packs (*measured*)

| Pack | Bytes | Holds |
|---|---|---|
| engine (OPENLARA.32X) | 360,448 | |
| IMAGE8.RAW (common) | 1,252,964 | menus, fonts, resident sprites, sound tables |
| OJ001-OJ009 (stages) | 497,692-980,872, together 6,364,744 | SSPRITES, MAP, TILES, LIGHT, OBJECTS ... |
| OJ010 (story) | 388,828 | six pictures, the text |

The stage packs share nothing with each other (a check of every file and picture in them found no
duplicate). So Jazz's sharing trims (T3 / T4) give nothing here.

## 3. Memory plan

**Home pages (W1 = 1), the 4 MB view.** Today the common pack starts at 0x100000, behind 1 MB
kept for the engine (`PLAT_CART_DATA_OFFSET`). The engine is 352 KB, so the common pack moves
down to 0x060000:

| Cart (W1 = 1) | Size | Holds |
|---|---|---|
| `0x000000`-`0x05FFFF` | 384 KB | engine (352 KB now) |
| `0x060000`-`0x191FFF` | 1.2 MB | common pack (ends at 0x191E64) |
| `0x192000`-`0x3EFFFF` | 2,482,176 bytes | **home slot** |
| `0x3F0000`-`0x3FFFFF` | 64 KB | the shared save area |

**Extended pages:** 8 MB: pages 2-3; 16 MB: pages 2-7. Each holds 2,031,616 bytes of packs:
the last 64 KB of every page is its tail (the owner record).

### 8 MB: placement of the stages (no pack crosses a page)

| Place | Stages | Bytes | Spare |
|---|---|---|---|
| home slot | 7, 8, 9 | 2,428,192 | 53,984 |
| page 2 | 1, 2, 4 | 1,960,148 | 71,468 |
| page 3 | 3, 5, 6 | 1,976,404 | 55,212 |

**All nine stages fit.** The story (388,828 bytes) does not: the 8 MB cart has room for
packs of (2,482,176 + 2 × 2,031,616 = 6,545,408), and stages + story are 6,753,572: 208,164
short. That leaves two choices:

- **(a) Default, nothing to trim.** The story stays as it is today. When STORY is picked in
  OPTIONS, it is read into page 3 over stage 3 (about 9 s, as now). Page 3's tail marks stage 3
  as gone, and it reloads the next time it is played.
- **(b) Trim the stages by 208 KB or more (3.3 %)**, so the story fits too. Candidates:
  - `MAP.BIN` (2.0 MB over the 9 stages) packs to 0.5 MB with zlib, so its cell table and block
    lists have room for a tighter layout the SH2 can still read in place.
  - `TILEXT.BIN` (104 KB) packs to 21 KB.

  Both are changes in `tools/hur_convert.py` and the readers in `game/p1.c`.

### 16 MB

Everything is resident, with no trims:

- **Home slot:** stages 7, 8, 9.
- **Pages 2 and 3:** as in the 8 MB plan.
- **Page 4:** the story.
- **7.6 MB stay free** (pages 5-7 and the spares) for later content.

## 4. Code changes (besides the shared mechanism)

| Part | Change |
|---|---|
| engine end at 0x060000 | `port/plat.h` `PLAT_CART_DATA_OFFSET`; `port/mars/oj.ld` rom LENGTH (0xF8000 → 0x58000); `tools/make_oj_rom.py` `ENGINE_END`; `tools/hur_convert.py` `RESIDENT_MAX` / `STAGE_MAX`; `game/host.c` (the PC build reads the common pack at 0x100000); `plat_mars.c` `PACK_SLOT`; a new engine-size check in `build.sh` (it checks only IMAGE8 today) |
| `cd32x/subcpu/main.c` | CMD 68 (shared proposal, variant B); CMD 38's destination offset (COMM2 bit 15 + COMM8L = cart offset), because IMAGE8 at 0x060000 and stages at arbitrary offsets are not 128 KB slots; W1 = 1 before CMD 60 |
| `cd32x/subcpu/hw_md.s` | no change (CMD 68 uses the existing `chilly_rv_memcpy`) |
| `game/p1.c` `load_stage` | `stage_in_cart` and `STAGE_CART` give way to the directory lookup. A stage in the cart is checked by checksum (≥ 0.2 s per MB, up to 0.2 s for the largest stage: Sonic CD32X may overwrite pages 2-7 without touching their tails) and mounted after `plat_cart_page` with no loading screen; one that is not is placed and loaded with CMD 38's destination |
| `game/story.c` | the story pack through the same lookup (8 MB: plan (a) or (b)) |
| `build.sh` | checks: engine ≤ 0x060000, common ≤ 0x192000, every stage fits the place the plan gives it |

Hurrican has no cart save traffic during play (its high scores are in the Sega CD's backup RAM,
CMD 56 / 59), and the game itself writes no save pieces; the port settings reader in `plat_mars.c` still goes
through the W1 = 1 wrapper.

## 5. To verify on the first board

The Jazz list (section 9 of its design), plus:

- With an extended page mapped, the stage plays the same as from the home slot: frame times
  (Start → TIMING ON), sprites, map, lights.
- The attract demos play from a stage in an extended page.
