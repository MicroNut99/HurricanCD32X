#!/usr/bin/env python3
"""make_splash.py - the boot / loading picture for the Hurrican CD32X disc, made from the game's
own pictures (so the repository carries none of Hurrican's pictures).

    python3 tools/make_splash.py <Hurrican data folder> <out.png>

320x200, after Hurrican's own loading screen (Gameplay.cpp DisplayLoadInfo: ShowMenuBack's
star field with the turning spiral hurrican_rund.png, "LOADING" in the menu font, loading.png's
glowing frame around the bar), with Hurrican's logo on top - the same picture shows at power-on
and while a stage loads (the loader and the platform layer both show IMAGE.RAW).

The progress bar itself is drawn by the boot path (cd32x/sh2/cdloader_sh2.c splash_bar, port/
plat_mars.c - not moved): x 56-236 of the 320 (centre 146), lines 186-198 of the 224 (174-186 of
this 200-line picture); everything is centred on it. tools/make_boot_image.py turns the picture
into IMAGE.RAW; tools/splash_bar_colours.py then gives the bar the original's colours."""
import os
import sys
from PIL import Image

data, out = sys.argv[1], sys.argv[2]
tex = os.path.join(data, "textures")
CX, BAR_Y0, BAR_Y1 = 146, 174, 186

img = Image.new("RGBA", (320, 200), (0, 0, 0, 255))
# the star field with the spiral, its middle in the middle of the screen (it covers it all)
rund = Image.open(os.path.join(tex, "hurrican_rund.png")).convert("RGBA").resize((360, 360), Image.LANCZOS)
img.alpha_composite(rund, (CX - 180, 100 - 180))

# the logo on top, its visible part centred
logo = Image.open(os.path.join(tex, "hurrican-logo.png")).convert("RGBA")
logo = logo.resize((logo.width * 7 // 10, logo.height * 7 // 10), Image.LANCZOS)
bx0, _, bx1, _ = logo.getchannel("A").point(lambda v: 255 if v >= 128 else 0).getbbox()
img.alpha_composite(logo, (CX - (bx0 + bx1) // 2, 16))


def add_white(pic, x, y):
    """a grey-scale picture of Hurrican's (white on black, drawn additively) over the image"""
    g = pic.convert("L")
    layer = Image.new("RGBA", g.size, (255, 255, 255, 0))
    layer.putalpha(g)
    img.alpha_composite(layer, (x, y))


# loading.png's glowing frame (360 x 60, halved: 180 x 30 - the bar's width) around the bar
frame = Image.open(os.path.join(tex, "loading.png")).resize((180, 30), Image.LANCZOS)
add_white(frame, CX - 90, (BAR_Y0 + BAR_Y1) // 2 - 15)

# "LOADING" in the menu font (menufont.png: 28 x 28 cells, '!' first), halved, above the bar
font = Image.open(os.path.join(tex, "menufont.png")).convert("RGBA")
glyphs = []
for ch in "LOADING":
    k = ord(ch) - 33
    cell = font.crop(((k % 16) * 28, (k // 16) * 28, (k % 16) * 28 + 28, (k // 16) * 28 + 28))
    box = cell.getchannel("A").point(lambda v: 255 if v >= 128 else 0).getbbox() or (0, 0, 8, 28)
    cell = cell.crop((box[0], 0, box[2], 28)).resize(((box[2] - box[0] + 1) // 2, 14), Image.LANCZOS)
    glyphs.append(cell)
w = sum(g.width + 2 for g in glyphs) - 2
x = CX - w // 2
for g in glyphs:
    img.alpha_composite(g, (x, BAR_Y0 - 22))
    x += g.width + 2

# make_boot_image.py cuts away black borders (values <= 8): a frame just above that (9, 9, 9 -
# black on the screen) keeps the whole 320x200 in place without a visible line
out_img = img.convert("RGB")
for i in range(320):
    out_img.putpixel((i, 0), (9, 9, 9)); out_img.putpixel((i, 199), (9, 9, 9))
for j in range(200):
    out_img.putpixel((0, j), (9, 9, 9)); out_img.putpixel((319, j), (9, 9, 9))
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
out_img.save(out)
