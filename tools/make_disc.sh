#!/bin/bash
# make_disc.sh - builds the Hurrican CD32X disc of the folder this script sits
# in (tools/..), checks it, and copies the .bin / .cue to sega/HurricanP1q/.
#   bash /mnt/s/KoboPort/sega-toolchain-12.1/sega/HurricanCD32X/tools/make_disc.sh
# Hurrican's data: HURRICAN_DATA=<folder>, or HurricanData/ in the project folder, or taken
# from Hurrican-master.zip (next to the project folder, in it, or in the Windows Downloads
# folder; unpacked into HurricanData/ once).
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
NAME=HurricanP1q
DISCS="$(cd "$HERE/.." && pwd)/$NAME"

say()  { echo; echo "==> $*"; }
fail() { echo; echo "*** STOPPED: $*"; echo "*** Paste the lines above and this message."; exit 1; }

cd "$HERE" || fail "cannot enter $HERE"
say "Project folder: $HERE"

DATA="${HURRICAN_DATA:-$HERE/HurricanData}"
# the story's text (OPTIONS - STORY) is Hurrican/lang/english.lng, beside data/: an older
# HurricanData/ without lang/ is unpacked again
[ -d "$DATA/textures" ] && [ ! -f "$DATA/lang/english.lng" ] && [ ! -f "$DATA/../lang/english.lng" ] && [ -z "${HURRICAN_DATA:-}" ] && rm -rf "$HERE/HurricanData"
if [ ! -d "$DATA/textures" ]; then
  ZIP=""
  for z in "$(cd "$HERE/.." && pwd)/Hurrican-master.zip" "$HERE/Hurrican-master.zip" /mnt/c/Users/*/Downloads/Hurrican-master.zip; do
    [ -f "$z" ] && { ZIP="$z"; break; }
  done
  [ -n "$ZIP" ] || fail "no Hurrican data: put Hurrican-master.zip next to the project folder or in your Windows Downloads folder, or set HURRICAN_DATA"
  say "Unpacking Hurrican's data from $ZIP"
  T="$(mktemp -d)"
  unzip -q "$ZIP" 'Hurrican-master/Hurrican/data/*' 'Hurrican-master/Hurrican/lang/*' -d "$T" || fail "unzip failed"
  mv "$T/Hurrican-master/Hurrican/data" "$HERE/HurricanData" || fail "could not move the data folder"
  mv "$T/Hurrican-master/Hurrican/lang" "$HERE/HurricanData/lang" || fail "could not move the lang folder"
  DATA="$HERE/HurricanData"
fi
for p in numpy PIL; do
  python3 -c "import $p" 2>/dev/null || {
    say "Python's $p is missing - installing it (asks for your password)"
    sudo apt install -y python3-numpy python3-pil || fail "numpy / Pillow could not be installed"
  }
done

say "Building (the full log goes to build_log.txt)"
HURRICAN_DATA="$DATA" ./build.sh > build_log.txt 2>&1
STATUS=$?
tail -25 build_log.txt
[ $STATUS -eq 0 ] || fail "the build ended with an error (see build_log.txt)"

BIN=cd32x/subcpu/$NAME.bin
CUE=cd32x/subcpu/$NAME.cue
[ -f "$BIN" ] && [ -f "$CUE" ] || fail "no .bin / .cue was made"
[ "$(find "$CUE" -mmin -240 | wc -l)" -eq 1 ] || fail "the .cue is old - the build did not make a new disc"

mkdir -p "$DISCS" || fail "cannot make $DISCS"
cp "$BIN" "$CUE" "$DISCS/" || fail "copy to $DISCS failed"
say "DONE. Burn this file:"
WIN=$(echo "$DISCS" | sed 's#^/mnt/\([a-z]\)/#\U\1:\\#; s#/#\\#g')
echo "    $WIN\\$NAME.cue"
ls -la "$DISCS"
