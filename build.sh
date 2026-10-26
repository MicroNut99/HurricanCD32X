#!/bin/bash
# Hurrican CD32X - builds the P1 disc (stage 1, playable). The steps are the OpenJazz port's
# J19b build (the last disc proven on the console), with Hurrican's packs in place of Jazz's:
#   splash -> packs + sound effects -> music (CD audio tracks 2-13) -> program (mars/, with J19b's
#   music switch) -> ROM check (OpenLara's prep_openlara_cd.py) -> SH2 loader -> Sub-CPU/68000
#   -> disc image with the audio tracks, then the same checks.
# The boot path is J19b's, unchanged: cd32x/ (loader, Sub-CPU, 68000 listener), port/ (start-up,
# platform layer), tools/make_oj_rom.py, tools/make_boot_image.py. The program goes on the disc
# as OPENLARA.32X (file 20), with the J19b engine's module name.
#   HURRICAN_DATA=<Hurrican's data folder> ./build.sh 2>&1 | tee build_log.txt
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
trap 'echo; echo "*** BUILD FAILED at line $LINENO: $BASH_COMMAND"; echo "*** look above for the first error (grep -n -B6 Error build_log.txt)"' ERR
MARKER="SH2-OPENLARA-CD-OL1-LOADER"
ROMS="$HERE/cd32x/subcpu/ROMS"
DATA="${HURRICAN_DATA:-$HERE/HurricanData}"
mkdir -p "$ROMS" "$HERE/build"

[ -d "$DATA/textures" ] && [ -f "$DATA/levels/jungle.map" ] \
  || { echo "*** no Hurrican data in $DATA (set HURRICAN_DATA=<Hurrican's data folder with textures/ and levels/>) - stopping"; exit 1; }
python3 -c "import numpy, PIL" 2>/dev/null \
  || { echo "*** Python needs numpy and Pillow: sudo apt install python3-numpy python3-pil - stopping"; exit 1; }
grep -q '^#define CDL_TARGET_OPENLARA 1' "$HERE/cd32x/sh2/cdloader_sh2.c" \
  || { echo "*** cd32x/sh2/cdloader_sh2.c is not in OpenLara mode - stopping"; exit 1; }

# CART=8 / CART=16: the 8 / 16 MB cart test builds (docs/CART_8MB_16MB.md): the common pack at
# cart 0x060000, the stages kept in the cart's other pages, Sub-CPU CMD 68 (window 1's page) and
# CMD 38's destination. Unset: the 4 MB build, unchanged. DISC_NAME: the disc's file name.
CART="${CART:-}"
# ED: CART=ED - the Mega EverDrive PRO as the 16 MB paged cart (CD32X BootCamp docs/12; the listener's
# MODE1-9 turns the page writes into its window registers; pages 1-6 - page 7 is the EverDrive's own).
#   CART=ED EDCD=1 bash build.sh      the disc + cd32x/subcpu/EDCD_HURRICAN.32X, the launcher started from the
#                                     EverDrive menu with the disc in the Sega CD (BOOTCAMP=<folder>,
#                                     default ../CD32X-BootCamp)
# Saves still go to the cart's page 1 (the EverDrive wipes its PSRAM at its menu: nothing persists yet).
case "$CART" in ""|8|16|ED) ;; *) echo "*** CART must be 8, 16 or ED (or unset) - stopping"; exit 1;; esac
[ "$CART" = ED ] && echo "==> cart: the Mega EverDrive PRO (16 MB, pages 1-6)"
[ -n "$CART" ] && echo "==> cart: the $CART MB test build"
DISC_NAME="${DISC_NAME:-HurricanP1q}"
echo "==> splash";      python3 "$HERE/tools/make_splash.py" "$DATA" "$HERE/build/Splash.png"
                        python3 "$HERE/tools/make_boot_image.py" "$HERE/build/Splash.png" "$ROMS/IMAGE.RAW"
                        python3 "$HERE/tools/splash_bar_colours.py" "$ROMS/IMAGE.RAW"
echo "==> data packs";  rm -f "$ROMS"/IMAGE8.RAW "$ROMS"/IMAGE9.RAW "$ROMS"/OJ*.PAK "$ROMS"/SFX*.BIN
                        python3 "$HERE/tools/hur_convert.py" "$DATA" "$ROMS" 1 2 3 4 5 6 7 8 9
                        python3 "$HERE/tools/hur_story.py" "$DATA" "$ROMS/OJ010.PAK"      # OPTIONS - STORY
SIZE=$(stat -c %s "$ROMS/IMAGE8.RAW")
[ "$SIZE" -le $((0x180000)) ] || { echo "*** IMAGE8.RAW is $SIZE bytes: more than the 1.5 MB before the stage pack area - stopping"; exit 1; }
if [ -n "$CART" ]; then [ "$SIZE" -le $((0x190000)) ] || { echo "*** IMAGE8.RAW is $SIZE bytes: more than the $((0x190000)) page 0 has for it (0x060000-0x1F0000) - stopping"; exit 1; }; fi
echo "==> sound effects"; python3 "$HERE/tools/make_hur_sfx.py" "$DATA" "$ROMS/SFX00.BIN"
# music: Hurrican's modules rendered to CD audio tracks with the bundled libxmp-lite
# (tools/libxmp-lite, LGPL) - one pass each (the console repeats a track):
#   2 menu, 3 stage clear, 4 game over, 5-13 stages 1-9 (the songs the stages' map files name),
#   14 the credits (OPTIONS - CREDITS, game/credits.c), 15-23 the bosses of stages 1-9,
#   24 the intro (OPTIONS - STORY, game/story.c)
echo "==> music";       MUS="$HERE/build/music"; mkdir -p "$MUS"
                        gcc -O2 -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -I"$HERE/tools/libxmp-lite/include" -I"$HERE/tools/libxmp-lite/src" \
                            -o "$HERE/build/it2wav" "$HERE/tools/it2wav.c" "$HERE"/tools/libxmp-lite/src/*.c "$HERE"/tools/libxmp-lite/src/loaders/*.c -lm
                        T=2
                        for SONG in menu stageclear gameover JungleBeats Aztech elevator cave factory Tower eis Blue loststation credits \
                                    boss_robofist boss_pharao boss_lift boss_golem boss_spider boss_dragon boss_ice boss_onslaught boss_last intro; do
                          "$HERE/build/it2wav" "$DATA/music/$SONG.it" "$MUS/Track$(printf %02d $T).wav" 300 1 0.32   # -10 dB
                          T=$((T + 1))
                        done
echo "==> engine";      rm -rf "$HERE"/mars/obj "$HERE"/mars/obj_cart*     # the music switch is compiled into port/plat_mars.c
                        rm -f "$HERE/mars/hurrican32x.elf" "$HERE/mars/hurrican32x.bin" "$HERE/mars/OPENJAZZ.32X"
                        make -C "$HERE/mars" EXTRA_DEFS="-DOJ32X_MUSIC=1" CART="$CART"
[ -f "$HERE/mars/OPENJAZZ.32X" ] || { echo "*** mars/OPENJAZZ.32X was not made - stopping"; exit 1; }
PREP_MODE1=""; if [ "${EDCD:-0}" = 1 ]; then PREP_MODE1="--mode1"; fi   # MODE1-1 (EDCD=1, below): the RAM jump table too
echo "==> ROM check";   python3 "$HERE/cd32x/ref/openlara/tools/prep_openlara_cd.py" $PREP_MODE1 "$HERE/mars/OPENJAZZ.32X" "$ROMS/OPENLARA.32X"
echo "==> SH2 loader";  cd "$HERE/cd32x/sh2"    && make -f Makefile.cdloader clean && make -f Makefile.cdloader
CART_DEFS="${CART:+-DCART_PAGED}"; [ "$CART" = ED ] && CART_DEFS="-DCART_PAGED -DCART_ED"
echo "==> Sub-CPU";     cd "$HERE/cd32x/subcpu" && make clean && make cd CART_DEFS="$CART_DEFS"

ISO="$HERE/cd32x/subcpu/CDROMPlayer.iso"
[ -f "$ISO" ] || { echo "*** $ISO was not made - stopping"; exit 1; }
grep -a -q "$MARKER" "$ISO" || { echo "*** the disc does NOT contain the loader - stopping"; exit 1; }
for f in APP.BIN CDLOADER.BIN OPENLARA.32X IMAGE.RAW IMAGE8.RAW OJ001.PAK OJ002.PAK OJ003.PAK OJ004.PAK OJ005.PAK OJ006.PAK OJ007.PAK OJ008.PAK OJ009.PAK OJ010.PAK SFX00.BIN; do
  [ -f "$HERE/cd32x/subcpu/cd/$f" ] || { echo "*** $f is not on the disc - stopping"; exit 1; }
done
NAME="$(dd if="$HERE/cd32x/subcpu/cd/OPENLARA.32X" bs=1 skip=$((0x3C0)) count=8 2>/dev/null)"
[ "$NAME" = "OpenJazz" ] || { echo "*** cd/OPENLARA.32X does not carry the J19b module name ($NAME) - stopping"; exit 1; }
cmp -s "$HERE/cd32x/subcpu/cd/OPENLARA.32X" "$ROMS/OPENLARA.32X" || { echo "*** the disc has an old program - stopping"; exit 1; }
echo "==> disc checked: loader + program + splash + packs + sound effects"

# the mixed-mode image: data track + audio tracks 2-13 (as the J19b build with music)
cd "$HERE/cd32x/subcpu" && python3 make_mixed_cd2.py CDROMPlayer.iso "$MUS"/Track*.wav --out "$DISC_NAME"
echo
echo "==> done:"
echo "    disc: cd32x/subcpu/$DISC_NAME.cue (with its .bin)"

# EDCD=1 (MODE1-1, CD32X BootCamp docs/12): EDCD.32X, made by the BootCamp's edboot/build_edcd.sh around
# this disc's Sub-CPU program built a second time for the cartridge boot (MODE1=1 M1PAGE=1, in a copy
# of cd32x/subcpu: the disc keeps the normal program)
if [ "${EDCD:-0}" = 1 ]; then
  BC="${BOOTCAMP:-$HERE/../CD32X-BootCamp}"
  [ -f "$BC/edboot/build_edcd.sh" ] || { echo "*** EDCD=1 needs the CD32X BootCamp next to this folder (or BOOTCAMP=<folder>): not at $BC"; exit 1; }
  echo "==> EDCD_HURRICAN.32X: the disc started from the EverDrive PRO (cartridge boot)"
  T=$(mktemp -d); mkdir -p "$T/subcpu/ROMS" "$T/sh2"
  find "$HERE/cd32x/subcpu" -maxdepth 1 -type f -exec cp {} "$T/subcpu/" \;
  cp "$HERE/cd32x/sh2/CDLOADER.32x" "$T/sh2/"
  ( cd "$T/subcpu" && make clean > /dev/null && make CDROMPlayer.bin MODE1=1 M1CTRL0=0x8000 M1PAGE=1 CART_DEFS="$CART_DEFS" > "$T/mode1.log" 2>&1 ) \
    || { tail -20 "$T/mode1.log"; echo "*** the MODE1 Sub-CPU program did not build"; exit 1; }
  bash "$BC/edboot/build_edcd.sh" "$T/subcpu/CDROMPlayer.bin" "$HERE/cd32x/sh2/CDLOADER.32x" "$HERE/cd32x/subcpu/EDCD_HURRICAN.32X"
  rm -rf "$T"
  echo "==> EDCD_HURRICAN.32X: $HERE/cd32x/subcpu/EDCD_HURRICAN.32X - copy it to the EverDrive's SD card and start it from the menu"
  echo "    with the disc above in the Sega CD (not \"Run Mega-CD\")"
fi
