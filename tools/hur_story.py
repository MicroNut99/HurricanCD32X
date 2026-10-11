#!/usr/bin/env python3
"""hur_story.py - the story (Hurrican's intro) as a pack for OPTIONS - STORY.

    python3 tools/hur_story.py <Hurrican data folder> <out OJ010.PAK>

Hurrican's intro (Intro.cpp): six pictures intro1.png .. intro6.png (640 x 480), the story's
lines (the language file's "Intro Screen 1" block - TEXT::INTRO1 on, one player: the two-player
line is left out), shown one every 5.1 s, typed out from the left, four lines a picture, over a
dark band at the bottom; the song intro.it (CD track 24, rendered by build.sh).

The pack (port/pack.h OJPK v2, loaded like a stage pack to the cart's stage area; the
Sub-CPU opens any OJnnn.PAK by name - no change to it):
  PIC0 .. PIC5   320 x 200 palette indices: the picture halved (320 x 240), lines 10-210,
                 with Hurrican's dark band (Intro.cpp: a gradient from 360, black from 420 -
                 here from line 150) baked in
  PAL0 .. PAL5   256 x u16 32X colours (big-endian): 1-250 the picture's, 251-254 the text's
                 greens and the shadow (game/story.c), 0 / 255 black
  TEXT           the lines, each ending in a 0 byte, upper case (the 32X font has no lower case)
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from hur_convert import write_pack, to32x  # noqa: E402

PICS = 6
TOP, BAND = 10, 150             # the 200 lines shown start at the halved picture's line 10


def story_lines(data):
    lang = os.path.join(os.path.dirname(os.path.abspath(data)), "lang", "english.lng")
    if not os.path.exists(lang):
        lang = os.path.join(data, "lang", "english.lng")
    lines = open(lang, encoding="latin-1").read().splitlines()
    k = next(i for i, l in enumerate(lines) if "Intro Screen 1" in l)
    out = []
    for l in lines[k + 1:]:
        if l.startswith("//"):
            break
        out.append(l)
    # TEXT::INTRO1 + 22 (one trooper) is used, + 23 (two troopers) is not (Intro.cpp)
    if len(out) >= 25:
        del out[23]
    return [l.upper().replace('"', "'") for l in out]


def picture(path):
    im = Image.open(path).convert("RGB").resize((320, 240), Image.LANCZOS)
    a = np.array(im, np.float32)[TOP:TOP + 200]
    for y in range(BAND, 200):                     # the dark band: Hurrican's gradient, then black
        a[y] *= max(0.0, 1.0 - (y - BAND) / 30.0) * 0.85 if y < BAND + 30 else 0.0
    im = Image.fromarray(a.clip(0, 255).astype(np.uint8))
    q = im.quantize(colors=250, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:750], np.uint8).reshape(-1, 3)
    idx = np.array(q, np.uint8).astype(np.int32) + 1          # 1 .. 250
    words = [0x0400] * 256                                     # black (0x0000 is see-through)
    for i, c in enumerate(pal):
        words[i + 1] = to32x(c) or 0x0400
    words[251] = to32x((40, 255, 40))                          # the newest line
    words[252] = to32x((20, 170, 20))                          # older lines
    words[253] = to32x((10, 100, 10))
    words[254] = 0x0400                                        # the shadow
    return idx.astype(np.uint8).tobytes(), b"".join(struct.pack(">H", w) for w in words)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data, out = sys.argv[1], sys.argv[2]
    files = []
    for k in range(PICS):
        pix, pal = picture(os.path.join(data, "textures", "intro%d.png" % (k + 1)))
        files += [("PIC%d" % k, pix), ("PAL%d" % k, pal)]
    text = story_lines(data)
    files.append(("TEXT", b"".join(l.encode("latin-1", "replace") + b"\0" for l in text)))
    total = write_pack(out, files, 0x170000)
    print("  story pack: %d KB - %d pictures, %d lines" % (total // 1024, PICS, len(text)))


if __name__ == "__main__":
    main()
