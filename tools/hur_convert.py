#!/usr/bin/env python3
"""hur_convert.py - Hurrican's pictures and maps for the CD32X: the resident pack and stage packs.

    python3 tools/hur_convert.py <Hurrican data folder> <out folder> [stage ...]

<Hurrican data folder> is the folder that holds textures/ and levels/ (Hurrican/data in the
source tree). Writes into <out folder>:
    IMAGE8.RAW   the resident pack (cart 0x100000): every picture the game loads at start-up
                 (player, HUD, fonts, menus, shots, explosions, common enemies) - loaded ONCE
    OJ00n.PAK    stage n's pack (cart 0x280000): only what stage n adds - its tiles, map,
                 enemies, palette and the map's objects
Default stage: 1 (jungle.map).

Keeping loads short (the lesson from the Jazz port): what every stage needs is in the resident
pack once and never in a stage pack; nothing raw goes in (no PNG or map file, only what the 32X
draws from: indexed pixels, packed rows, a block-compressed map).

Pictures: halved (640x480 -> 320x240), 256 colours:
    0        transparent in pictures; the backdrop colour on screen
    1-96     the resident pictures' colours (the same in every stage)
    97-255   the stage's own colours
Sprite frames are stored packed: only visible pixels, row by row as runs (skip, length).

File formats (big-endian; all offsets from the start of the file):
  RSPRITES.BIN (resident) and SSPRITES.BIN (stage):
               "SPRB" ntex nframes; ntex x {name[16], first u32, count u16, fw u16, fh u16, flags u16}
               flags bit 0: an additive picture (a glow: Hurrican adds it to the screen)
               nframes x {data u32, x0 s16, y0 s16, w u16, h u16, pixels u32}
               frame data: h x u32 row offsets (from the frame data), then per row:
               runs u16, runs x {skip u16, len u16, len pixels, a 0 byte if len is odd}
  TILES.BIN    "TILE" n u32, 100 u32; n flag bytes (bit 0: no transparent pixel), padded to 4;
               n x 10x10 pixels (tile 0 = none)
  MAP.BIN      "MAPB" w u16 h u16 (tiles), bw u16 bh u16 (8x8 blocks), ncells u32, nblocks u32;
               ncells x {under-back, under-front, over-back, over-front u16 tile numbers,
                         block flags u16};
               bw*bh u16 block numbers; nblocks x 64 u16 cell numbers (rows of 8)
               block flags (from Hurrican's 32-bit block value, Tileengine.hpp):
                 bit 0 wall, 1 wall for enemies only, 2 platform (stand on, jump through),
                 3 damage, 4 slope rising to the left, 5 slope rising to the right,
                 6 liquid (water), 7 destructible wall, 8 waterfall, 9 swamp,
                 10 turning point (moving platforms turn there), 11 front tiles take the light
  LIGHT.BIN    "LITE" w u16 h u16 bw u16 bh u16 nblocks u32; bw*bh u16 block numbers;
               nblocks x 32 bytes: 8 x 8 cells, 4 bits each (the even x in the high bits):
               the cell's light, 0 = none, n = SHADE.LUT row n. Hurrican lights a tile by its
               corners (TileEngine::ComputeCoolLight); here a cell gets the mean of its corners
  TILEXT.BIN   "TEXT" n u32, then 10 bytes a tile: a row's drawn part, first << 4 | last (0xFF:
               none) - the wall pass over the sprites (P1q)
  SHADE.LUT    16 x 256 bytes: row n = the palette entry nearest to colour p lit by light n
               (row 0 = no change; entry 0, transparent, stays 0)
  OBJECTS.BIN  "OBJS" n u32; n x {id u16, x s16, y s16, value1 s16, value2 s16, skill u16}:
               the map's objects (player start, enemies, pick-ups, triggers) in Hurrican's
               coordinates (640x480 screen pixels; the game runs its logic in those and draws
               at half size), sorted by x
  PALETTE.BIN  256 x u16 32X colours (0BBBBBGGGGGRRRRR)
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
RES_COLOURS = 96
FIRST_STAGE_COLOUR = 1 + RES_COLOURS          # 97
STAGE_COLOURS = 256 - FIRST_STAGE_COLOUR      # 159
STAGE_MAPS = {1: "jungle.map", 2: "temple.map", 3: "elevator.map", 4: "mpde.map",
              5: "spinnenfabrik.map", 6: "tower.map", 7: "eis.map", 8: "cave.map", 9: "endboss.map"}
BACKGROUND = ("static_", "back_", "front_")
RESIDENT_MAX = 0x180000   # 0x100000 .. 0x280000
STAGE_MAX = 0x170000      # 0x280000 .. 0x3F0000 (saves from there)

# Hurrican's block flags (Tileengine.hpp)
WAND, VERDECKEN = 0x000001, 0x000010
# -> the cell's 16-bit flags (MAP.BIN): Hurrican's bit, our bit
BLOCK_FLAGS = [(0x000001, 0x001), (0x000002, 0x002), (0x000004, 0x004), (0x000100, 0x008),
               (0x200000, 0x010), (0x400000, 0x020), (0x800000 | 0x000080, 0x040),
               (0x001000, 0x080), (0x040000, 0x100), (0x008000, 0x200), (0x000800, 0x400),
               (0x004000, 0x800)]


def fail(msg):
    sys.exit("*** hur_convert: " + msg)


# ------------------------------------------------------------------ pictures
# pictures without any transparent pixel are glows and smoke that Hurrican adds to the screen
# (black = nothing) - except these, which are drawn as they are
SOLID = ("static_", "s_", "hud", "loading", "console", "boss", "fallingstone", "pharaokopf",
         "spinnenmaschine", "lift", "wasserfall", "water", "walker-laser", "skeletor_shot")


def load_rgba(path):
    """RGBA pixels and whether the picture is an additive one (its alpha made from brightness)"""
    im = Image.open(path)
    if im.mode == "P" and "transparency" in im.info:
        im = im.convert("RGBA")
    a = np.array(im.convert("RGBA"), dtype=np.uint8)
    # magenta is a colour key in some of Hurrican's pictures
    key = (a[:, :, 0] == 255) & (a[:, :, 1] == 0) & (a[:, :, 2] == 255)
    a[key, 3] = 0
    additive = False
    name = os.path.basename(path)
    if (a[:, :, 3] < 255).mean() < 0.01 and not name.startswith(SOLID):
        additive = True
        a[:, :, 3] = np.where(a[:, :, :3].max(2) >= 32, 255, 0)
    return a, additive


def halve(a):
    """640x480 -> 320x240: 2x2 averages of premultiplied colour (no dark edges); a pixel is
    visible when half or more of its area was"""
    h, w = a.shape[0] // 2 * 2, a.shape[1] // 2 * 2
    a = a[:h, :w].astype(np.float32)
    al = a[:, :, 3:4] / 255.0
    pm = a[:, :, :3] * al
    s = lambda x: x[0::2, 0::2] + x[1::2, 0::2] + x[0::2, 1::2] + x[1::2, 1::2]
    alpha = s(al) / 4.0
    rgb = np.where(alpha > 0, s(pm) / 4.0 / np.maximum(alpha, 1e-6), 0)
    out = np.zeros((h // 2, w // 2, 4), np.uint8)
    out[:, :, :3] = np.clip(rgb + 0.5, 0, 255).astype(np.uint8)
    out[:, :, 3] = (alpha[:, :, 0] >= 0.5) * 255
    return out


def quantize(pixels, n):
    """n colours for an (N, 3) array of visible pixels (median cut + k-means refinement)"""
    if len(pixels) == 0:
        return np.zeros((0, 3), np.uint8)
    rng = np.random.default_rng(1)
    if len(pixels) > 600000:
        pixels = pixels[rng.choice(len(pixels), 600000, replace=False)]
    # 15-bit colours are all the 32X shows: quantize those
    pixels = (pixels >> 3 << 3) | 4
    img = Image.fromarray(pixels.reshape(-1, 1, 3))
    # Image.Quantize came with Pillow 9.1; Ubuntu 22.04 has 9.0 (the plain constant)
    mediancut = Image.Quantize.MEDIANCUT if hasattr(Image, "Quantize") else Image.MEDIANCUT
    q = img.quantize(colors=n, method=mediancut, kmeans=3)
    pal = np.array(q.getpalette()[:3 * n], np.uint8).reshape(-1, 3)
    used = np.unique(np.array(q))
    return pal[used]


def to32x(c):
    r, g, b = (int(v) >> 3 for v in c)
    return r | g << 5 | b << 10


class Mapper:
    """nearest palette entry for every 15-bit colour, among the entries given"""
    def __init__(self, palette, entries):
        self.cube = np.zeros(32768, np.uint8)
        cols = np.arange(32768)
        rgb = np.stack([(cols & 31) << 3 | 4, (cols >> 5 & 31) << 3 | 4, (cols >> 10 & 31) << 3 | 4], 1).astype(np.int32)
        pal = palette[entries].astype(np.int32)
        best = np.full(32768, 1 << 30, np.int64)
        for k, e in enumerate(entries):
            d = rgb - pal[k]
            # weighted distance (green counts most)
            dist = 3 * d[:, 0] ** 2 + 4 * d[:, 1] ** 2 + 2 * d[:, 2] ** 2
            better = dist < best
            best[better] = dist[better]
            self.cube[better] = e

    def map(self, a):
        """RGBA array -> palette indices (0 where transparent)"""
        k = (a[:, :, 0].astype(np.int32) >> 3) | (a[:, :, 1].astype(np.int32) >> 3) << 5 | (a[:, :, 2].astype(np.int32) >> 3) << 10
        out = self.cube[k]
        out[a[:, :, 3] < 128] = 0
        return out


# ------------------------------------------------------------------ textures
def read_texture_table():
    tex = {}
    for line in open(os.path.join(HERE, "data", "hurrican_textures.txt")):
        if line.startswith("#") or not line.strip():
            continue
        p = line.split()
        tex[p[0]] = dict(xs=int(p[1]), ys=int(p[2]), xfc=int(p[3]), yfc=int(p[4]),
                         xfs=int(p[5]), yfs=int(p[6]), tags=set(p[7].split(",")))
    return tex


def frames_of(rgba, t):
    """the frames of a sprite sheet as Hurrican cuts it (LoadImage's sizes; pictures whose file
    is larger or smaller than the sizes given are scaled to them)"""
    H, W = rgba.shape[:2]
    sx, sy = W / max(1, t["xs"]), H / max(1, t["ys"])
    out = []
    for fy in range(max(1, t["yfc"])):
        for fx in range(max(1, t["xfc"])):
            x0, y0 = round(fx * t["xfs"] * sx), round(fy * t["yfs"] * sy)
            x1, y1 = round((fx + 1) * t["xfs"] * sx), round((fy + 1) * t["yfs"] * sy)
            f = rgba[y0:y1, x0:x1]
            if f.shape[0] < 2 or f.shape[1] < 2:
                f = np.zeros((2, 2, 4), np.uint8)
            out.append(f)
    if "ROT16" in t["tags"]:
        # a picture Hurrican draws turned (RenderSpriteRotated, e.g. the spread shot, which
        # points up at angle 0): 16 frames, frame k turned clockwise by k x 22.5 degrees
        im = Image.fromarray(out[0], "RGBA")
        side = max(im.width, im.height)               # a long picture (the laser) turns in a square
        if im.width != im.height:
            sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
            sq.paste(im, ((side - im.width) // 2, (side - im.height) // 2))
            im = sq
        out = [np.array(im.rotate(-22.5 * k, resample=Image.BICUBIC)) for k in range(16)]
    return out


def pack_rows(idx):
    """frame (palette indices) -> trimmed box and packed rows"""
    ys, xs = np.nonzero(idx)
    if len(ys) == 0:
        return (0, 0, 0, 0, 0, b"")
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    box = idx[y0:y1, x0:x1]
    h = box.shape[0]
    rows = []
    for r in box:
        runs = []
        x, n = 0, len(r)
        while x < n:
            s = x
            while x < n and r[x] == 0:
                x += 1
            if x >= n:
                break
            e = x
            while e < n and r[e] != 0:
                e += 1
            runs.append((x - s, bytes(r[x:e])))
            x = e
        b = bytearray(struct.pack(">H", len(runs)))
        for skip, px in runs:
            b += struct.pack(">HH", skip, len(px)) + px
            if len(px) & 1:
                b += b"\0"
        rows.append(bytes(b))
    head = 4 * h
    offs, pos = [], head
    for r in rows:
        offs.append(pos)
        pos += len(r)
    data = b"".join(struct.pack(">I", o) for o in offs) + b"".join(rows)
    return (int(x0), int(y0), int(x1 - x0), int(y1 - y0), int(np.count_nonzero(box)), data)


def sprite_file(entries):
    """entries: [(name, fw, fh, [packed frames], additive)] -> SPRITES.BIN"""
    ntex = len(entries)
    nfr = sum(len(e[3]) for e in entries)
    head = 12 + 28 * ntex + 16 * nfr
    tex_tab, fr_tab, data = bytearray(), bytearray(), bytearray()
    first = 0
    pos = head
    for name, fw, fh, frames, additive in entries:
        nm = name.encode("ascii")[:16].ljust(16, b"\0")
        tex_tab += nm + struct.pack(">IHHHH", first, len(frames), fw, fh, 1 if additive else 0)
        for (x0, y0, w, h, px, d) in frames:
            if pos & 3:
                pad = 4 - (pos & 3)
                data += b"\0" * pad
                pos += pad
            fr_tab += struct.pack(">IhhHHI", pos if d else 0, x0, y0, w, h, px)
            data += d
            pos += len(d)
        first += len(frames)
    return b"SPRB" + struct.pack(">II", ntex, nfr) + bytes(tex_tab) + bytes(fr_tab) + bytes(data)


# ------------------------------------------------------------------ packs
# ---------------------------------------------------------------- the Genesis background (genbg)
GBG_W, GBG_H = 256, 224          # the picture on the plane: 32 x 28 tiles, repeated across 64
GBG_TILE0 = 128                  # VRAM tile 128 (0x1000) on: the font stays below
GBG_DIM = 0.85                   # the picture's brightness under the fade (P1u 0.7, P1v 0.85)


GBG_LINES = 1                    # palette lines (bands of rows); 1: line 3 only, "GBG1" (P1v: three
                                 # bands showed seams between them)
GBG_FADE = 0.35                  # P1v: how much of the colour fade lies over the picture


def _gbg_snap(pal):
    """colours (0..255) on the Genesis' levels (0..7 a channel), each once"""
    uniq = []
    for c in map(tuple, np.clip(np.rint(np.asarray(pal, np.float64) / 36.43), 0, 7).astype(np.int32).tolist()):
        if c not in uniq:
            uniq.append(c)
    return np.array(uniq, np.int32)


# P1w: a stage's own fade - (bottom hue against the top's in turns, top brightness, bottom
# brightness, how much lies over the picture); the default: a neighbouring hue. The jungle stays
# blue and brighter (P1v's shift turned it purple-pink there, and it came out nearly black)
GBG_FADE_DEFAULT = (0.08, 0.62, 0.08, GBG_FADE)
GBG_FADE_STAGE = {"static_jungle": (-0.03, 0.95, 0.22, 0.55), "static_ice_blue": (0.0, 0.75, 0.10, GBG_FADE)}
# P1x: a stage's backdrop from another far picture, recoloured - stage 1: stage 7's ice picture
# in shades of blue (the jungle's own far picture is nearly black)
GBG_STAGE_PICTURE = {1: ("static_ice.png", "blue")}
# P1z: only these stages get a Genesis backdrop (the others' walls cover the sky anyway)
GBG_STAGES = (1, 7)


def _gbg_recolour(rgb, how):
    """the picture's brightness on a ramp of one colour ("blue": navy to light blue)"""
    a = rgb[:, :, :3].astype(np.float32) / 255.0
    lum = (a @ np.array([0.3, 0.59, 0.11], np.float32))[:, :, None]
    lo, hi = {"blue": ((0.02, 0.05, 0.20), (0.55, 0.78, 1.0))}[how]
    out = np.array(lo, np.float32) * (1 - lum) + np.array(hi, np.float32) * lum
    return np.concatenate([np.clip(out * 255 + 0.5, 0, 255).astype(np.uint8), rgb[:, :, 3:4]], 2)


def _gbg_fade(im, name=""):
    """P1v: the stage's own colour fading down the picture - its hue bright at the top, a
    neighbouring hue dark at the bottom - laid over the (dimmed) picture"""
    import colorsys
    a = np.asarray(im, np.float32) / 255.0
    m = a.reshape(-1, 3).mean(0)
    h, sat, v = colorsys.rgb_to_hsv(*m)
    if sat < 0.15:                                # a grey picture: the warmest of its colours
        flat = a.reshape(-1, 3)
        h = colorsys.rgb_to_hsv(*flat[np.argmax(flat[:, 0] - flat[:, 2])])[0]
    shift, vtop, vbot, k = GBG_FADE_STAGE.get(name, GBG_FADE_DEFAULT)
    top = np.array(colorsys.hsv_to_rgb(h, 0.75, vtop), np.float32)
    bot = np.array(colorsys.hsv_to_rgb((h + shift) % 1.0, 0.85, vbot), np.float32)
    t = np.linspace(0, 1, a.shape[0], dtype=np.float32)[:, None, None]
    tint = top[None, None, :] * (1 - t) + bot[None, None, :] * t
    out = a * (1 - k) + tint * k
    return Image.fromarray(np.clip(out * 255 + 0.5, 0, 255).astype(np.uint8))


def genesis_bg(rgb, name=""):
    """GENBG.BIN from the stage's far background (an RGB array): the picture (a step darker, P1u)
    under the stage's colour fade (P1v), in three bands of rows with 15 colours each (palette
    lines 1-3, Genesis 3-bit levels), spread over them with error diffusion; 8x8 tiles
    deduplicated (also mirrored), a 64x32 plane map.
      "GBG2", u16 tiles, u16 lines (3), 48 x u16 CRAM words (lines 1-3, colour 0 unused),
      64 x 32 u16 map, tiles x 32 bytes"""
    im = Image.fromarray((rgb[:, :, :3].astype(np.float32) * GBG_DIM).astype(np.uint8)).resize((GBG_W, GBG_H), Image.LANCZOS)
    im = _gbg_fade(im, name)
    TY, TX = GBG_H // 8, GBG_W // 8
    line_of = np.array([[min(ty * GBG_LINES // TY, GBG_LINES - 1)] * TX for ty in range(TY)], np.int32)
    idx = np.zeros((GBG_H, GBG_W), np.uint8)
    cram = []
    for g in range(GBG_LINES):
        rows = np.repeat(line_of[:, 0] == g, 8)
        r0, r1 = np.nonzero(rows)[0][[0, -1]]
        ext = np.asarray(im)[max(r0 - 12, 0):r1 + 13]     # the neighbours' edges too: no seam
        q = Image.fromarray(ext).quantize(colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pl = _gbg_snap(np.array(q.getpalette()[:45]).reshape(-1, 3))
        pimg = Image.new("P", (1, 1))
        flat = [int(v * 36.43 + 0.5) for c in pl for v in c]
        pimg.putpalette(flat + flat[:3] * (256 - len(pl)))
        qq = np.minimum(np.array(im.quantize(palette=pimg, dither=Image.Dither.FLOYDSTEINBERG), np.uint8), len(pl) - 1) + 1
        idx[rows] = qq[rows]
        words = [0] + [int((b << 9) | (gg << 5) | (r << 1)) for r, gg, b in pl]
        cram += words + [0] * (16 - len(words))
    tiles, lookup, tmap = [], {}, np.zeros((GBG_H // 8, GBG_W // 8), np.int32)
    for ty in range(GBG_H // 8):
        for tx in range(GBG_W // 8):
            t = idx[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8]
            key_line = int(line_of[ty, tx])
            word = None
            for hf in (0, 1):
                for vf in (0, 1):
                    v = t[:, ::-1] if hf else t
                    v = v[::-1, :] if vf else v
                    k = bytes([key_line]) + v.tobytes()
                    if k in lookup:
                        word = lookup[k] | (hf << 11) | (vf << 12)
                        break
                if word is not None:
                    break
            if word is None:
                n = GBG_TILE0 + len(tiles)
                tiles.append(t.copy())
                lookup[bytes([key_line]) + t.tobytes()] = n
                word = n
            tmap[ty, tx] = word | ((4 - GBG_LINES + int(line_of[ty, tx])) << 13)
    plane = np.zeros((32, 64), np.int32)
    for r in range(32):
        for c in range(64):
            plane[r, c] = tmap[r % (GBG_H // 8), c % (GBG_W // 8)]
    tbytes = bytearray()
    for t in tiles:
        for row in t:
            for i in range(0, 8, 2):
                tbytes.append(int(row[i]) << 4 | int(row[i + 1]))
    out = (b"GBG1" + struct.pack(">HH", len(tiles), 0) if GBG_LINES == 1 else
           b"GBG2" + struct.pack(">HH", len(tiles), GBG_LINES)) + b"".join(struct.pack(">H", w) for w in cram)
    out += b"".join(struct.pack(">H", int(w)) for w in plane.flatten()) + bytes(tbytes)
    return out, len(tiles)


def write_pack(path, files, limit):
    """OJPK version 2 with files only (port/pack.h)"""
    n = len(files)
    pos = 24 + 24 * n
    offs = []
    for name, data in files:
        pos = (pos + 3) & ~3
        offs.append(pos)
        pos += len(data)
    total = (pos + 3) & ~3
    out = bytearray(b"OJPK" + struct.pack(">IIIII", 2, n, 0, total, 0))
    for (name, data), o in zip(files, offs):
        out += name.encode("ascii").ljust(16, b"\0") + struct.pack(">II", o, len(data))
    for (name, data), o in zip(files, offs):
        out += b"\0" * (o - len(out))
        out += data
    out += b"\0" * (total - len(out))
    if total > limit:
        fail("%s is %d bytes, more than the %d its cart area holds" % (path, total, limit))
    open(path, "wb").write(out)
    return total


# ------------------------------------------------------------------ map
def read_map(path):
    b = open(path, "rb").read()
    hdr = b[:1292]
    sets = [hdr[249 + 16 * i:265 + 16 * i].split(b"\0")[0].decode() for i in range(hdr[248])]
    sx, sy, nobj = struct.unpack_from("<III", hdr, 1276)
    a = np.frombuffer(b, np.uint8, sx * sy * 12, 1292).reshape(sx, sy, 12)
    blk = a[:, :, 8].astype(np.uint32) | a[:, :, 9].astype(np.uint32) << 8 | a[:, :, 10].astype(np.uint32) << 16 | a[:, :, 11].astype(np.uint32) << 24
    bgs = [hdr[146 + 24 * i:170 + 24 * i].split(b"\0")[0].decode() for i in range(4)]
    # the objects: LevelObjectStruct (24 bytes) after the tiles
    objs = []
    o = 1292 + sx * sy * 12
    for k in range(nobj):
        if o + 24 > len(b):
            break
        oid, ox, oy, _light, skill, _p1, _p2, v1, v2 = struct.unpack_from("<IiiBBBBii", b, o)
        objs.append((oid, ox, oy, v1, v2, skill))
        o += 24
    return dict(w=sx, h=sy, sets=sets, setb=a[:, :, 0], setf=a[:, :, 1], back=a[:, :, 2], front=a[:, :, 3], block=blk,
                light=a[:, :, 4:7], bgs=bgs, objs=objs)


def corner_lights(m, wall):
    """Hurrican's tile lighting (TileEngine::ComputeCoolLight): each corner of a tile gets the
    mean of the tile's colour and its three neighbours' at that corner (a neighbour on the other
    side of a wall edge counts with the tile's own colour). Returns TL, TR, BL, BR: [x][y][3]"""
    L = m["light"].astype(np.int32)

    def nb(dx, dy):
        sh = np.roll(np.roll(L, -dx, 0), -dy, 1)
        sw = np.roll(np.roll(wall, -dx, 0), -dy, 1)
        return np.where((sw == wall)[..., None], sh, L)
    tl = (L + nb(-1, -1) + nb(0, -1) + nb(-1, 0)) // 4
    tr = (L + nb(0, -1) + nb(1, -1) + nb(1, 0)) // 4
    bl = (L + nb(-1, 0) + nb(-1, 1) + nb(0, 1)) // 4
    br = (L + nb(1, 0) + nb(0, 1) + nb(1, 1)) // 4
    return tl, tr, bl, br


def light_clusters(colours, weights, n):
    """n light colours for the map's corner colours (k-means, weighted); white is not one of
    them (light 0 = the tile as it is)"""
    rng = np.random.default_rng(2)
    c = colours.astype(np.float64)
    w = weights.astype(np.float64)
    if len(c) <= n:
        return colours.astype(np.int32)
    centres = c[rng.choice(len(c), n, replace=False, p=w / w.sum())]
    for _ in range(30):
        d = ((c[:, None, :] - centres[None, :, :]) ** 2).sum(2)
        lab = d.argmin(1)
        for k in range(n):
            sel = lab == k
            if sel.any():
                centres[k] = (c[sel] * w[sel, None]).sum(0) / w[sel].sum()
    return np.clip(centres + 0.5, 0, 255).astype(np.int32)


def stage_pack(data_dir, out_dir, stage, tex, res_palette, res_mapper_unused):
    m = read_map(os.path.join(data_dir, "levels", STAGE_MAPS[stage]))
    tag = "S%d" % stage
    names = sorted(n for n, t in tex.items() if tag in t["tags"] and "R" not in t["tags"])
    tilesets = [n for n in names if n.startswith("s_")]
    # the far background layers are not in the pack (the 32X does not draw them: the screen
    # shows the backdrop colour there - planned for the Genesis planes); the far picture gives
    # the backdrop colour
    backgrounds = [n for n in names if n.startswith(BACKGROUND)]
    pictures = [n for n in names if not n.startswith("s_") and not n.startswith(BACKGROUND)]
    tdir = os.path.join(data_dir, "textures")

    # which tiles the map draws, and where (Hurrican's TileEngine: DrawBackLevel / DrawFrontLevel
    # under the sprites, DrawBackLevelOverlay / DrawOverlayLevel over them)
    back, front, blk = m["back"].astype(np.int32), m["front"].astype(np.int32), m["block"]
    wall = (blk & WAND) != 0
    cover = (blk & VERDECKEN) != 0
    hasf = front > 0
    under_b = (back > 0) & (~wall | (hasf & cover))
    under_f = hasf & ~cover & ~wall
    over_b = (back > 0) & wall & ~(hasf & cover)
    over_f = hasf & (cover | wall)
    key_b = m["setb"].astype(np.int32) * 256 + back
    key_f = m["setf"].astype(np.int32) * 256 + front
    used = set(np.unique(key_b[under_b | over_b]).tolist()) | set(np.unique(key_f[under_f | over_f]).tolist())
    used = sorted(used)

    # the light: 15 colours for the map's tile corners, white = none
    corners = corner_lights(m, wall)
    lit_cells = (back > 0) | (hasf & ((blk & 0x004000) != 0))
    allc = np.concatenate([c[lit_cells] for c in corners])
    whiteish = (allc >= 236).all(1)
    uc, cnt = np.unique(allc[~whiteish], axis=0, return_counts=True)
    lights = light_clusters(uc, cnt, 15) if len(uc) else np.zeros((0, 3), np.int32)
    nlights = len(lights)

    def light_index(c):
        """[x][y][3] corner colours -> 0 (white) or 1..15"""
        flat = c.reshape(-1, 3).astype(np.int32)
        if nlights == 0:
            return np.zeros(c.shape[:2], np.int32)
        d = ((flat[:, None, :] - lights[None, :, :]) ** 2).sum(2)
        idx = d.argmin(1) + 1
        idx[(flat >= 236).all(1)] = 0
        return idx.reshape(c.shape[:2])
    lidx = [light_index((corners[0] + corners[1] + corners[2] + corners[3]) // 4)]
    cell_light = np.where(lit_cells, lidx[0], 0)

    # the stage's pictures (halved), the tile sets cut into 10x10 tiles
    sheets, additive_pic = {}, {}
    for n in tilesets + pictures + backgrounds:
        p = os.path.join(tdir, n)
        if not os.path.exists(p):
            print("    (missing picture %s)" % n)
            continue
        sheets[n], additive_pic[n] = load_rgba(p)
    tiles_rgba = []
    for k in used:
        s, art = k >> 8, k & 255
        setname = m["sets"][s] if s < len(m["sets"]) else None
        if setname not in sheets:
            tiles_rgba.append(np.zeros((10, 10, 4), np.uint8))
            continue
        i = art - 1
        x0, y0 = (i % 12) * 20, (i // 12) * 20
        t = sheets[setname][y0:y0 + 20, x0:x0 + 20]
        if t.shape[:2] != (20, 20):
            t = np.zeros((20, 20, 4), np.uint8)
        tiles_rgba.append(halve(t))
    pic_frames = {}
    for n in pictures:
        if n in sheets:
            pic_frames[n] = [halve(f) for f in frames_of(sheets[n], tex[n])]

    # the stage's colours
    vis = [t[t[:, :, 3] >= 128][:, :3] for t in tiles_rgba]
    # the tiles as they are lit on screen: their colours count too
    tile_ix = {k: i for i, k in enumerate(used)}
    combos = set()
    for q in range(1):
        li = lidx[q]
        for keys, sel in ((key_b, under_b | over_b), (key_f, (under_f | over_f) & ((blk & 0x004000) != 0))):
            pairs = np.unique(np.stack([keys[sel], li[sel]], 1), axis=0)
            combos |= set(map(tuple, pairs.tolist()))
    for key, li in combos:
        if li == 0 or key not in tile_ix:
            continue
        t = tiles_rgba[tile_ix[key]]
        px = t[t[:, :, 3] >= 128][:, :3].astype(np.int32)
        vis.append((px * lights[li - 1][None, :] // 255).astype(np.uint8))
    for fr in pic_frames.values():
        vis += [f[f[:, :, 3] >= 128][:, :3] for f in fr]
    allpx = np.concatenate([v for v in vis if len(v)]) if vis else np.zeros((0, 3), np.uint8)
    stage_cols = quantize(allpx, STAGE_COLOURS)
    palette = np.zeros((256, 3), np.uint8)
    palette[1:1 + len(res_palette)] = res_palette
    palette[FIRST_STAGE_COLOUR:FIRST_STAGE_COLOUR + len(stage_cols)] = stage_cols
    # backdrop (index 0): the sky - the mean colour of the top of the far background
    static = [n for n in backgrounds if n.startswith("static_") and n in sheets]
    if static:
        top = halve(sheets[static[0]])[:40]
        palette[0] = top[:, :, :3].reshape(-1, 3).mean(0).astype(np.uint8)
    nstage = FIRST_STAGE_COLOUR + len(stage_cols)
    mapper = Mapper(palette, list(range(1, nstage)))

    # tiles
    tiles = [np.zeros((10, 10), np.uint8)] + [mapper.map(t) for t in tiles_rgba]
    flags = bytes([1 if np.all(t != 0) else 0 for t in tiles])
    tile_no = {k: i + 1 for i, k in enumerate(used)}
    tfile = b"TILE" + struct.pack(">II", len(tiles), 100) + flags + b"\0" * (-len(flags) % 4) + b"".join(t.tobytes() for t in tiles)
    # P1q: each tile row's drawn part (first << 4 | last, 0xFF empty) - the wall pass over the
    # sprites copies only that part
    ext = bytearray()
    for t in tiles:
        for r in t:
            nz = np.nonzero(r)[0]
            ext.append(0xFF if len(nz) == 0 else (int(nz[0]) << 4) | int(nz[-1]))
    xfile = b"TEXT" + struct.pack(">I", len(tiles)) + bytes(ext)

    # map: per cell the four tile numbers, cells deduplicated, 8x8 blocks of cells deduplicated
    lut = np.zeros(65536, np.int32)
    for k, i in tile_no.items():
        lut[k] = i
    bflags = np.zeros(blk.shape, np.uint32)
    for hur, ours in BLOCK_FLAGS:
        bflags |= np.where((blk & hur) != 0, ours, 0).astype(np.uint32)
    cells = np.stack([np.where(under_b, lut[key_b], 0), np.where(under_f, lut[key_f], 0),
                      np.where(over_b, lut[key_b], 0), np.where(over_f, lut[key_f], 0), bflags], -1).astype(np.uint16)
    cells = cells.transpose(1, 0, 2)                       # [y][x]
    W, H = m["w"], m["h"]
    flat = cells.reshape(-1, 5)
    uniq, inv = np.unique(flat, axis=0, return_inverse=True)
    # cell 0 must be the empty cell
    empty = np.where((uniq == 0).all(1))[0]
    order = np.arange(len(uniq))
    if len(empty):
        e = empty[0]
        order = np.concatenate([[e], np.delete(order, e)])
    else:
        uniq = np.concatenate([np.zeros((1, 5), np.uint16), uniq])
        inv = inv + 1
        order = np.arange(len(uniq))
    remap = np.empty(len(order), np.int64)
    remap[order] = np.arange(len(order))
    uniq = uniq[order]
    cellmap = remap[inv.reshape(-1)].reshape(H, W)
    bw, bh = (W + 7) // 8, (H + 7) // 8
    pad = np.zeros((bh * 8, bw * 8), np.int64)
    pad[:H, :W] = cellmap
    blocks = pad.reshape(bh, 8, bw, 8).transpose(0, 2, 1, 3).reshape(bh * bw, 64)
    ub, binv = np.unique(blocks, axis=0, return_inverse=True)
    if len(uniq) > 65535 or len(ub) > 65535:
        fail("stage %d: too many different cells or blocks for 16-bit numbers" % stage)
    mfile = (b"MAPB" + struct.pack(">HHHHII", W, H, bw, bh, len(uniq), len(ub)) + uniq.astype(">u2").tobytes()
             + binv.reshape(-1).astype(">u2").tobytes() + ub.astype(">u2").tobytes())

    # the light map: 4 bits a cell, 8x8 blocks deduplicated
    lpad = np.zeros((bh * 8, bw * 8), np.int64)
    lpad[:H, :W] = cell_light.T
    lblocks = lpad.reshape(bh, 8, bw, 8).transpose(0, 2, 1, 3).reshape(bh * bw, 64)
    lub, lbinv = np.unique(lblocks, axis=0, return_inverse=True)
    packed = (lub[:, 0::2] << 4 | lub[:, 1::2]).astype(np.uint8)
    lfile = (b"LITE" + struct.pack(">HHHHI", W, H, bw, bh, len(lub)) + lbinv.reshape(-1).astype(">u2").tobytes()
             + packed.tobytes())

    # sprites, backgrounds
    entries = []
    for n in pictures:
        if n not in pic_frames:
            continue
        fr = [pack_rows(mapper.map(f)) for f in pic_frames[n]]
        fw, fh = pic_frames[n][0].shape[1], pic_frames[n][0].shape[0]
        entries.append((n, fw, fh, fr, additive_pic.get(n, False)))
    sfile = sprite_file(entries)

    pal = b"".join(struct.pack(">H", to32x(c)) for c in palette)

    # the shade tables: palette colours lit by each light
    shade = np.zeros((16, 256), np.uint8)
    shade[0] = np.arange(256)
    for k in range(nlights):
        lit = (palette.astype(np.int32) * lights[k][None, :] // 255).astype(np.uint8)
        shade[k + 1] = mapper.map(np.concatenate([lit.reshape(256, 1, 3), np.full((256, 1, 1), 255, np.uint8)], 2))[:, 0]
        shade[k + 1, 0] = 0
    for k in range(nlights + 1, 16):
        shade[k] = shade[0]

    # the map's objects, sorted by x (the game wakes them as the camera comes near)
    objs = sorted(m["objs"], key=lambda t: (t[1], t[2]))
    clamp = lambda v: max(-32768, min(32767, int(v)))
    ofile = b"OBJS" + struct.pack(">I", len(objs)) + b"".join(
        struct.pack(">HhhhhH", oid & 0xFFFF, clamp(x), clamp(y), clamp(v1), clamp(v2), sk & 0xFFFF)
        for oid, x, y, v1, v2, sk in objs)

    gbg = None
    if stage not in GBG_STAGES:
        pass
    elif stage in GBG_STAGE_PICTURE and os.path.exists(os.path.join(tdir, GBG_STAGE_PICTURE[stage][0])):
        pname, how = GBG_STAGE_PICTURE[stage]
        rgb = _gbg_recolour(load_rgba(os.path.join(tdir, pname))[0], how)
        gbg, gtiles = genesis_bg(rgb, pname.split(".")[0] + "_" + how)
        print("  Genesis background: %s in %s, %d tiles (%d KB)" % (pname, how, gtiles, len(gbg) // 1024))
    elif static:
        gbg, gtiles = genesis_bg(sheets[static[0]], static[0].split(".")[0])
        print("  Genesis background: %s, %d tiles (%d KB)" % (static[0], gtiles, len(gbg) // 1024))
    files = [("PALETTE.BIN", pal), ("TILES.BIN", tfile), ("TILEXT.BIN", xfile), ("MAP.BIN", mfile), ("SSPRITES.BIN", sfile),
             ("OBJECTS.BIN", ofile), ("SHADE.LUT", shade.tobytes()), ("LIGHT.BIN", lfile)] + ([("GENBG.BIN", gbg)] if gbg else [])
    out = os.path.join(out_dir, "OJ%03d.PAK" % stage)
    total = write_pack(out, files, STAGE_MAX)
    print("  stage %d (%s): %d KB - tiles %d (%d KB), map %dx%d: %d cells, %d blocks (%d KB), "
          "light %d KB, %d pictures (%d KB), %d stage colours, %d objects"
          % (stage, STAGE_MAPS[stage], total // 1024, len(tiles), len(tfile) // 1024, W, H, len(uniq), len(ub),
             len(mfile) // 1024, len(lfile) // 1024, len(entries), len(sfile) // 1024, len(stage_cols), len(objs)))
    return total


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    data_dir, out_dir = sys.argv[1], sys.argv[2]
    stages = [int(s) for s in sys.argv[3:]] or [1]
    if not os.path.isdir(os.path.join(data_dir, "textures")) or not os.path.isdir(os.path.join(data_dir, "levels")):
        fail("%s has no textures/ and levels/ folders (give Hurrican's data folder)" % data_dir)
    os.makedirs(out_dir, exist_ok=True)
    tex = read_texture_table()
    tdir = os.path.join(data_dir, "textures")

    # resident pictures: halved, their colours
    res = sorted(n for n, t in tex.items() if "R" in t["tags"])
    res_frames, res_add = {}, {}
    for n in res:
        p = os.path.join(tdir, n)
        if not os.path.exists(p):
            print("    (missing picture %s)" % n)
            continue
        rgba, res_add[n] = load_rgba(p)
        res_frames[n] = [halve(f) for f in frames_of(rgba, tex[n])]
    vis = [f[f[:, :, 3] >= 128][:, :3] for fr in res_frames.values() for f in fr]
    res_palette = quantize(np.concatenate([v for v in vis if len(v)]), RES_COLOURS)
    palette = np.zeros((256, 3), np.uint8)
    palette[1:1 + len(res_palette)] = res_palette
    res_mapper = Mapper(palette, list(range(1, 1 + len(res_palette))))
    entries = []
    for n in res:
        if n not in res_frames:
            continue
        fr = [pack_rows(res_mapper.map(f)) for f in res_frames[n]]
        entries.append((n, res_frames[n][0].shape[1], res_frames[n][0].shape[0], fr, res_add[n]))
    # the credits' font (game/credits.c): menufont.png's letters at 11 lines (the menu font
    # halved is 14), with hard edges - each pixel white or see-through, no blended edge colours
    mf = os.path.join(tdir, "menufont.png")
    if os.path.exists(mf):
        font, _ = load_rgba(mf)
        frames = []
        for k in range(16 * 12):
            cell = Image.fromarray(font[(k // 16) * 28:(k // 16) * 28 + 28, (k % 16) * 28:(k % 16) * 28 + 28], "RGBA")
            a = np.array(cell.getchannel("A").resize((11, 11), Image.LANCZOS))
            solid = np.zeros((11, 11, 4), np.uint8)
            solid[a >= 100] = (255, 255, 255, 255)
            frames.append(pack_rows(res_mapper.map(solid)))
        entries.append(("creditfont.png", 11, 11, frames, False))
    sfile = sprite_file(entries)
    rpal = b"".join(struct.pack(">H", to32x(c)) for c in palette[:1 + len(res_palette)])
    total = write_pack(os.path.join(out_dir, "IMAGE8.RAW"), [("RSPRITES.BIN", sfile), ("PALRES.BIN", rpal)], RESIDENT_MAX)
    print("  resident pack: %d KB - %d pictures, %d frames, %d colours" % (total // 1024, len(entries), sum(len(e[3]) for e in entries), len(res_palette)))

    for s in stages:
        stage_pack(data_dir, out_dir, s, tex, res_palette, res_mapper)


if __name__ == "__main__":
    main()
