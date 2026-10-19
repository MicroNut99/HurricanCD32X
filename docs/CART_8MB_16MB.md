# 8 MB and 16 MB cart releases (design)

Hurrican CD32X on the paged SRAM cart
([16MB-PCB-CD32X](https://github.com/MicroNut99/16MB-PCB-CD32X), rev 0.5). The mechanism is
the one designed for Jazz, the reference:
[OpenJazzCD32X-REV `docs/CART_8MB_16MB.md`](https://github.com/MicroNut99/OpenJazzCD32X-REV/blob/main/docs/CART_8MB_16MB.md).
It covers the home and extended pages, the page directory in save piece 14, the board probe,
CMD 68 / `cart_page` (68000 routine 42), and saves with W1 = 1. This file gives Hurrican's own
numbers and changes.

**Status:** design only. The pack sizes are *measured* (P2a packs, `cd32x/subcpu/ROMS`).

**Reviewed:** an independent review (Claude Fable, 19 Oct 2026) corrected the common pack's
end, the home slot, CMD 38's destination and the list of files to change; fixed here.

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
| `0x3F0000`-`0x3FFFFF` | 64 KB | the shared save area (piece 14: the page directory) |

**Extended pages:** 8 MB: pages 2-3 (2 × 2,097,152 bytes); 16 MB: pages 2-7.

### 8 MB: placement of the stages (no pack crosses a page)

| Place | Stages | Bytes | Spare |
|---|---|---|---|
| home slot | 7, 9, 4 | 2,432,900 | 49,276 |
| page 2 | 5, 2, 8 | 2,054,804 | 42,348 |
| page 3 | 1, 6, 3 | 1,877,040 | 220,112 |

**All nine stages fit.** The story (388,828 bytes) does not: the 8 MB cart has 6,668,288 bytes
for packs (2,482,176 + 2 × 2,097,152 = 6,676,480), and stages + story are 6,753,572: 77,092
short. That leaves two choices:

- **(a) Default, nothing to trim.** The story stays as it is today. When STORY is picked in
  OPTIONS, it is read into page 3, over stage 3 (about 9 s, as now). Stage 3 is
  marked as gone in the directory and reloads the next time it is played.
- **(b) Trim the stages by 77 KB or more (1.2 %)**, so the story fits too. Candidates:
  - `MAP.BIN` (2.0 MB over the 9 stages) packs to 0.5 MB with zlib, so its cell table and block
    lists have room for a tighter layout the SH2 can still read in place.
  - `TILEXT.BIN` (104 KB) packs to 21 KB.

  Both are changes in `tools/hur_convert.py` and the readers in `game/p1.c`.

### 16 MB

Everything is resident, with no trims:

- **Home slot:** stages 7, 9, 4.
- **Pages 2 and 3:** as in the 8 MB plan.
- **Page 4:** the story.
- **7.9 MB stay free** (pages 5-7 and the spares) for later content.

## 4. Code changes (besides the shared mechanism)

| Part | Change |
|---|---|
| engine end at 0x060000 | `port/plat.h` `PLAT_CART_DATA_OFFSET`; `port/mars/oj.ld` rom LENGTH (0xF8000 → 0x58000); `tools/make_oj_rom.py` `ENGINE_END`; `tools/hur_convert.py` `RESIDENT_MAX` / `STAGE_MAX`; `game/host.c` (the PC build reads the common pack at 0x100000); `plat_mars.c` `PACK_SLOT`; a new engine-size check in `build.sh` (it checks only IMAGE8 today) |
| `cd32x/subcpu/main.c` | CMD 68; CMD 38's destination argument (COMM2 bit 15 + COMM8L = cart offset), because IMAGE8 at 0x060000 and stages at arbitrary offsets are not 128 KB slots; CMD 61 with W1 = 1; W1 = 1 before CMD 60 |
| `cd32x/subcpu/hw_md.s` | `MD_CMD_END` is 36 here: entries 36-41 `cmd_done`, 42 `cart_page` (RV = 1, interrupts off) |
| `game/p1.c` `load_stage` | `stage_in_cart` and `STAGE_CART` give way to the directory lookup. A stage in the cart is mounted after `plat_cart_page` with no loading screen; one that is not is placed and loaded with CMD 38's destination |
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
