/*
 * Hurrican CD32X - P1: stage 1 (the jungle), playable.
 *
 * Runs from main(), after the boot path (the OpenJazz port's J19b files, byte for byte) has
 * finished, and uses only the platform layer's existing calls (port/plat.h).
 *
 * The screen: the J19b scrolling screen (plat_present_scroll) used as one LINEAR RING.
 * The frame buffer area at 0x200 holds 76,800 bytes (384 x 200); world pixel (x, y) - in
 * half-size pixels - is kept at
 *     lin(x, y) = (y * 384 + x) mod 76800
 * The view (320 x 200 shown, 322 kept for the 32X's 1-pixel shift) maps to 200 rows of 384,
 * so its pixels never share a place - the camera can go anywhere without re-anchoring.
 * The 32X line table reads each line straight from lin(cam_x, cam_y + line): with
 * row0 = (cam_y + cam_x / 384) % 200 and xoff = cam_x % 384, plat_present_scroll's
 * ((row0 + line) % 200) * 384 + xoff is exactly that. A line that runs past byte 76800 goes
 * on into the bytes after the ring (0x12E00, where the Jazz port kept its panel): they are a
 * copy of the ring's first 336 bytes, made at the end of every frame. The whole 200 lines
 * are the view (viewH = 200, no panel); the HUD is drawn into the view, as Hurrican does.
 *
 * What a frame draws (each of the two frame buffers keeps its own picture):
 *   - a clean copy of the tile layers (CLEAN, 76,800 bytes in SDRAM, the same linear ring):
 *     only the strips that came into view since the last frame are made from the map
 *   - in the frame buffer: the strips that came into view since this buffer's last frame,
 *     and exactly the rectangles this buffer's last frame drew sprites into, copied from
 *     CLEAN (no whole tiles, nothing outside the view)
 *   - the sprites (player, enemies, shots, pick-ups, explosions), clipped to the view
 *   - the wall tiles over the sprites (only cells under a sprite that have wall tiles)
 *   - the HUD
 *
 * The game: Hurrican's rules, in Hurrican's own coordinates (the PC game's 640 x 480 screen
 * pixels; 20-pixel tiles) and speeds (units of 1/10 s, at 60 steps a second), in fixed point
 * (8 bits of fraction); drawn at half size. The player, the map's blocks (walls, platforms,
 * slopes, damage, water), the spread shot, the stage's enemies, pick-ups, power blocks,
 * bridges, the moving platform and the level end. Easy, normal or hard (options menu).
 *
 * Buttons: left / right walk, down ducks, up aims up (with left / right: diagonally),
 * B shoots (hold), C or A jumps (hold for a higher jump), START pauses (CONTINUE / MAIN MENU / TIMING).
 *
 * Sound: plat_sfx(n) plays effect n of SFX00.BIN (tools/make_hur_sfx.py); plat_music(n) CD
 * audio track n (2 = the jungle, 3 = stage clear, 4 = game over; tools/it2wav.c).
 */
#include "plat.h"
#include "pack.h"

/* the added enemy kinds: a list of K2(...) lines and their code (game/k2/README.md) */
#ifndef K2_LIST
#define K2_LIST "k2/all_list.h"
#endif
#ifndef K2_CODE
#define K2_CODE "k2/all_code.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ screen layout */
#define RING_LIN   76800u        /* 384 x 200 bytes from the ring's start (frame buffer 0x200) */
#define RING_W     384
#define MIRROR_N   336           /* bytes after the ring that copy its start (0x12E00..) */
#define VIEW_W     322           /* pixels per line kept in view (320 shown + shift + 1) */
#define VIEW_H     200
#define HUD_H      24            /* P1m: the HUD's band at the top (drawn once, copied each frame) */
#define PLAY_H     (VIEW_H - HUD_H)   /* the level's part of the screen, below the band */
#define SCREEN_W   320

#define NSTAGES    9             /* OJ001.PAK .. OJ009.PAK */
#define STAGE_CART 0x280000u
#define STAGE_MAX  0x170000u

/* 16-bit frame buffer access (Kobo rule 4: no byte writes); b0 = byte at the lower address */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PUT2(p, off, b0, b1) (*(volatile uint16_t *)((p) + (off)) = (uint16_t)((b0) << 8 | (b1)))
#define GET2(p, off) (*(volatile uint16_t *)((p) + (off)))
static inline uint16_t rd16(const uint8_t *p) { return *(const uint16_t *)p; }
static inline uint32_t rd32(const uint8_t *p) { return *(const uint32_t *)p; }
#else
#define PUT2(p, off, b0, b1) ((p)[(off)] = (uint8_t)(b0), (p)[(off) + 1] = (uint8_t)(b1))
#define GET2(p, off) ((uint16_t)((p)[(off)] << 8 | (p)[(off) + 1]))
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
#endif
#define HI(w) ((w) >> 8)
#define LO(w) ((w) & 0xFF)

/* drawing code in SDRAM on the 32X (cart code goes through the slow bus and a 4 KB cache) */
#ifdef OJ32X_MARS
#define FAST __attribute__((section(".sdcode.fast"), aligned(16), noinline))
#else
#define FAST
#endif

/* ------------------------------------------------------------------ data (in the cart) */
static uint16_t palette[256];
static int col_dark, col_light, col_red, col_yellow, col_green, col_grey;
static const uint8_t *tile_px;
static int ntiles;
static const uint8_t *map_cells, *map_blockmap, *map_blocks;
static int map_w, map_h, map_bw;                 /* tiles */
static const uint8_t *light_blockmap, *light_blocks, *shade;
#define B_LITFRONT 0x800

typedef struct { const uint8_t *file, *tex, *frames; int ntex; } Bank;
static Bank bank_res, bank_stage;
typedef struct { const uint8_t *data; int x0, y0, w, h; } Frame;
/* a picture: its first frame, frame count, full frame size (half-size pixels) */
typedef struct { const Bank *bank; int first, count, fw, fh; } Pic;

static int bank_open(Bank *b, const char *name)
{
	const uint8_t *d;
	uint32_t size;
	if (!pack_file(name, &d, &size) || memcmp(d, "SPRB", 4) != 0) return 0;
	b->file = d;
	b->ntex = (int)rd32(d + 4);
	b->tex = d + 12;
	b->frames = b->tex + 28 * b->ntex;
	return 1;
}

/* a picture by name, from the stage pack or the resident pack; count 0 if missing */
static Pic pic(const char *name)
{
	Pic p = { 0, 0, 0, 0, 0 };
	const Bank *banks[2] = { &bank_stage, &bank_res };
	for (int k = 0; k < 2; k++)
		for (int i = 0; i < banks[k]->ntex; i++)
		{
			const uint8_t *t = banks[k]->tex + 28 * i;
			if (strncmp((const char *)t, name, 16) == 0)
			{
				p.bank = banks[k];
				p.first = (int)rd32(t + 16);
				p.count = rd16(t + 20);
				p.fw = rd16(t + 22);
				p.fh = rd16(t + 24);
				return p;
			}
		}
	return p;
}

static void frame_of(const Pic *p, int i, Frame *f)
{
	if (i < 0) i = 0;
	if (i >= p->count) i = p->count - 1;
	const uint8_t *e = p->bank->frames + 16 * (p->first + i);
	const uint32_t off = rd32(e);
	f->data = off ? p->bank->file + off : 0;
	f->x0 = (int16_t)rd16(e + 4);
	f->y0 = (int16_t)rd16(e + 6);
	f->w = rd16(e + 8);
	f->h = rd16(e + 10);
}

/* walls shot away (Hurrican's destructible walls): a bit per map cell, and the cells as they
 * are now - the map in the cart cannot change */
#define CELL_BYTES 10
#define MAX_BROKEN 300
static uint8_t broken_bits[36 * 1024];          /* maps up to 294,912 cells (the largest: 700 x 400) */
static int nbroken;
static struct { int16_t tx, ty; uint8_t cell[CELL_BYTES]; } broken[MAX_BROKEN];

static const uint8_t *broken_cell(int tx, int ty)
{
	for (int i = 0; i < nbroken; i++)
		if (broken[i].tx == tx && broken[i].ty == ty) return broken[i].cell;
	return 0;
}

/* map cell -> {under-back, under-front, over-back, over-front, block flags} */
FAST static const uint8_t *cell_at(int tx, int ty)
{
	if (tx < 0 || ty < 0 || tx >= map_w || ty >= map_h) return map_cells;   /* cell 0: empty */
	const uint32_t bit = (uint32_t)ty * (uint32_t)map_w + (uint32_t)tx;
	if (broken_bits[bit >> 3] & (1u << (bit & 7)))
	{
		const uint8_t *c = broken_cell(tx, ty);
		if (c) return c;
	}
	const int b = rd16(map_blockmap + 2 * ((ty >> 3) * map_bw + (tx >> 3)));
	const int c = rd16(map_blocks + 2 * (b * 64 + (ty & 7) * 8 + (tx & 7)));
	return map_cells + CELL_BYTES * c;
}

/* a cell's light (LIGHT.BIN): 0 = none, n = row n of SHADE.LUT */
FAST static int light_at(int tx, int ty)
{
	const int b = rd16(light_blockmap + 2 * ((ty >> 3) * map_bw + (tx >> 3)));
	const uint8_t v = light_blocks[b * 32 + (ty & 7) * 4 + ((tx & 7) >> 1)];
	return tx & 1 ? v & 15 : v >> 4;
}

/* block flags (tools/hur_convert.py, MAP.BIN) */
#define B_WALL     0x001
#define B_EWALL    0x002
#define B_PLATFORM 0x004
#define B_DAMAGE   0x008
#define B_SLOPE_L  0x010
#define B_SLOPE_R  0x020
#define B_LIQUID   0x040
#define B_DESTROY  0x080
#define B_TURN     0x400

static inline int blk(int tx, int ty)
{
	if (tx < 0 || tx >= map_w) return B_WALL;              /* the map's sides are walls */
	if (ty < 0 || ty >= map_h) return 0;
	return rd16(cell_at(tx, ty) + 8);
}

/* ------------------------------------------------------------------ the linear ring */
static uint8_t CLEAN[RING_LIN];          /* the tile layers as the view shows them (SDRAM) */
static volatile uint8_t *ring;           /* the back buffer's ring (lin 0) */
static int view_x, view_y;               /* this frame's level view: world pixels [x, x + 322) x [y, y + PLAY_H), below the HUD band */
static int cam_x, cam_y;                 /* the camera: world pixel shown at the screen's top-left */

static inline uint32_t lin_of(int x, int y)
{
	uint32_t l = (uint32_t)(y % VIEW_H) * RING_W + (uint32_t)x;
	while (l >= RING_LIN) l -= RING_LIN;
	return l;
}

/* n pixels to ring[l..l+n) (no wrap), any alignment */
FAST static void put_run(uint32_t l, const uint8_t *s, int n)
{
	if (n <= 0) return;
	if (l & 1)
	{
		const uint16_t w = GET2(ring, l - 1);
		PUT2(ring, l - 1, HI(w), s[0]);
		l++; s++; n--;
	}
	for (; n >= 2; n -= 2, l += 2, s += 2) PUT2(ring, l, s[0], s[1]);
	if (n)
	{
		const uint16_t w = GET2(ring, l);
		PUT2(ring, l, s[0], LO(w));
	}
}

/* the same, pixels of value 0 left as they are */
FAST static void put_keyed(uint32_t l, const uint8_t *s, int n)
{
	if (n <= 0) return;
	if (l & 1)
	{
		if (s[0]) { const uint16_t w = GET2(ring, l - 1); PUT2(ring, l - 1, HI(w), s[0]); }
		l++; s++; n--;
	}
	for (; n >= 2; n -= 2, l += 2, s += 2)
	{
		const uint8_t a = s[0], b = s[1];
		if (a && b) PUT2(ring, l, a, b);
		else if (a) { const uint16_t w = GET2(ring, l); PUT2(ring, l, a, LO(w)); }
		else if (b) { const uint16_t w = GET2(ring, l); PUT2(ring, l, HI(w), b); }
	}
	if (n && s[0])
	{
		const uint16_t w = GET2(ring, l);
		PUT2(ring, l, s[0], LO(w));
	}
}

/* n pixels from s to the ring at l, wrapping at the ring's end */
FAST static void fb_put(uint32_t l, const uint8_t *s, int n, int keyed)
{
	int a = n;
	if (l + (uint32_t)n > RING_LIN) a = (int)(RING_LIN - l);
	if (keyed) { put_keyed(l, s, a); put_keyed(0, s + a, n - a); }
	else { put_run(l, s, a); put_run(0, s + a, n - a); }
}

/* n pixels from CLEAN to the frame buffer, both at l */
FAST static void fb_from_clean(uint32_t l, int n)
{
	int a = n;
	if (l + (uint32_t)n > RING_LIN) a = (int)(RING_LIN - l);
	put_run(l, CLEAN + l, a);
	if (n > a) put_run(0, CLEAN, n - a);
}

/* a rectangle (world pixels) from CLEAN to the frame buffer, clipped to the view */
static uint32_t px_copied;
FAST static void rect_from_clean(int x0, int y0, int x1, int y1)
{
	if (x0 < view_x) x0 = view_x;
	if (y0 < view_y) y0 = view_y;
	if (x1 > view_x + VIEW_W) x1 = view_x + VIEW_W;
	if (y1 > view_y + PLAY_H) y1 = view_y + PLAY_H;
	if (x0 >= x1 || y0 >= y1) return;
	for (int y = y0; y < y1; y++) fb_from_clean(lin_of(x0, y), x1 - x0);
	px_copied += (uint32_t)((x1 - x0) * (y1 - y0));
}

/* the tile layers of a rectangle (world pixels) into CLEAN, made from the map */
static uint32_t px_composed;
FAST static void compose_clean(int x0, int y0, int x1, int y1)
{
	if (x0 >= x1 || y0 >= y1) return;
	px_composed += (uint32_t)((x1 - x0) * (y1 - y0));
	for (int y = y0; y < y1; y++)
	{
		const int ty = y / 10, ry = (y - ty * 10) * 10;
		uint32_t l = lin_of(x0, y);
		int x = x0;
		while (x < x1)
		{
			const int tx = x / 10;
			int e = tx * 10 + 10;
			if (e > x1) e = x1;
			const uint8_t *c = cell_at(tx, ty);
			const int ub = rd16(c), uf = rd16(c + 2), ob = rd16(c + 4), of = rd16(c + 6);
			const int base = ry - tx * 10;
			const uint8_t *pb = ub ? tile_px + 100 * ub + base : 0;
			const uint8_t *pf = uf ? tile_px + 100 * uf + base : 0;
			const uint8_t *qb = ob ? tile_px + 100 * ob + base : 0;
			const uint8_t *qf = of ? tile_px + 100 * of + base : 0;
			/* the light: back tiles always, front tiles when the cell says so */
			const int li = (ub | ob | uf | of) ? light_at(tx, ty) : 0;
			const uint8_t *sb = li ? shade + 256 * li : 0;
			const uint8_t *sf = (rd16(c + 8) & B_LITFRONT) ? sb : 0;
			for (; x < e; x++)
			{
				uint8_t v = 0, t;
				if (pb && (t = pb[x])) v = sb ? sb[t] : t;
				if (pf && (t = pf[x])) v = sf ? sf[t] : t;
				if (qb && (t = qb[x])) v = sb ? sb[t] : t;
				if (qf && (t = qf[x])) v = sf ? sf[t] : t;
				CLEAN[l] = v;
				if (++l == RING_LIN) l = 0;
			}
		}
	}
}

/* the wall tiles (over the sprites) of a rectangle, into the frame buffer, keyed */
FAST static void over_rect(int x0, int y0, int x1, int y1)
{
	uint8_t row[10];
	if (x0 < view_x) x0 = view_x;
	if (y0 < view_y) y0 = view_y;
	if (x1 > view_x + VIEW_W) x1 = view_x + VIEW_W;
	if (y1 > view_y + PLAY_H) y1 = view_y + PLAY_H;
	if (x0 >= x1 || y0 >= y1) return;
	for (int ty = y0 / 10; ty <= (y1 - 1) / 10; ty++)
		for (int tx = x0 / 10; tx <= (x1 - 1) / 10; tx++)
		{
			const uint8_t *c = cell_at(tx, ty);
			const int ob = rd16(c + 4), of = rd16(c + 6);
			if (!ob && !of) continue;
			const int li = light_at(tx, ty);
			const uint8_t *sb = li ? shade + 256 * li : 0;
			const uint8_t *sf = (rd16(c + 8) & B_LITFRONT) ? sb : 0;
			int cx0 = tx * 10, cx1 = cx0 + 10, cy0 = ty * 10, cy1 = cy0 + 10;
			if (cx0 < x0) cx0 = x0;
			if (cx1 > x1) cx1 = x1;
			if (cy0 < y0) cy0 = y0;
			if (cy1 > y1) cy1 = y1;
			for (int y = cy0; y < cy1; y++)
			{
				const int base = (y - ty * 10) * 10 - tx * 10;
				const uint8_t *qb = ob ? tile_px + 100 * ob + base : 0;
				const uint8_t *qf = of ? tile_px + 100 * of + base : 0;
				for (int x = cx0; x < cx1; x++)
				{
					uint8_t v = qf ? qf[x] : 0;
					if (v) { if (sf) v = sf[v]; }
					else if (qb && (v = qb[x]) && sb) v = sb[v];
					row[x - cx0] = v;
				}
				fb_put(lin_of(cx0, y), row, cx1 - cx0, 1);
			}
		}
}

/* the part of view (nx, ny) that view (ox, oy) did not have: up to two rectangles */
typedef struct { int16_t x0, y0, x1, y1; } Rect;
static int view_new_parts(int nx, int ny, int ox, int oy, Rect *out)
{
	if (nx - ox >= VIEW_W || ox - nx >= VIEW_W || ny - oy >= PLAY_H || oy - ny >= PLAY_H)
	{
		out[0] = (Rect){ (int16_t)nx, (int16_t)ny, (int16_t)(nx + VIEW_W), (int16_t)(ny + PLAY_H) };
		return 1;
	}
	int n = 0;
	/* rows the old view did not have, the full width */
	if (ny < oy) out[n++] = (Rect){ (int16_t)nx, (int16_t)ny, (int16_t)(nx + VIEW_W), (int16_t)oy };
	else if (ny > oy) out[n++] = (Rect){ (int16_t)nx, (int16_t)(oy + PLAY_H), (int16_t)(nx + VIEW_W), (int16_t)(ny + PLAY_H) };
	/* in the rows both have: the columns the old view did not have */
	const int ry0 = ny > oy ? ny : oy, ry1 = (ny < oy ? ny : oy) + PLAY_H;
	if (nx < ox) out[n++] = (Rect){ (int16_t)nx, (int16_t)ry0, (int16_t)ox, (int16_t)ry1 };
	else if (nx > ox) out[n++] = (Rect){ (int16_t)(ox + VIEW_W), (int16_t)ry0, (int16_t)(nx + VIEW_W), (int16_t)ry1 };
	return n;
}

/* ------------------------------------------------------------------ per frame buffer */
#define MAX_RECTS 120
typedef struct {
	int valid;
	int vx, vy;              /* the view of this buffer's last frame */
	int nrects;
	int overflow;            /* more rectangles than the list holds: copy the whole view */
	Rect rects[MAX_RECTS];   /* what that frame drew over the tiles */
} Buf;
static Buf bufs[2];
static Buf *cur;
static int clean_valid, clean_vx, clean_vy;

static void rect_add(int x0, int y0, int x1, int y1)
{
	if (x0 < view_x) x0 = view_x;
	if (y0 < view_y) y0 = view_y;
	if (x1 > view_x + VIEW_W) x1 = view_x + VIEW_W;
	if (y1 > view_y + PLAY_H) y1 = view_y + PLAY_H;
	if (x0 >= x1 || y0 >= y1) return;
	if (cur->nrects >= MAX_RECTS) { cur->overflow = 1; return; }
	cur->rects[cur->nrects++] = (Rect){ (int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1 };
}

/* a wall shot away (TileEngine::ExplodeWall): the cell keeps its back tile, loses its front
 * tile and its block flags; CLEAN gets the cell again now, both frame buffers copy it from
 * CLEAN when they are drawn next */
static void put16be(uint8_t *p, int v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static int wall_defer, nwall_pending;     /* P1m: walls shot away while the drawing runs alongside */
static int16_t wall_pending[32];
static void wall_redraw(int tx, int ty);
static int wall_break(int tx, int ty)
{
	if (tx < 1 || ty < 1 || tx >= map_w - 1 || ty >= map_h - 1 || nbroken >= MAX_BROKEN) return 0;
	const uint8_t *c = cell_at(tx, ty);
	if (!(rd16(c + 8) & B_DESTROY)) return 0;
	const int ub = rd16(c), ob = rd16(c + 4);
	memset(broken[nbroken].cell, 0, CELL_BYTES);
	put16be(broken[nbroken].cell, ub ? ub : ob);
	broken[nbroken].tx = (int16_t)tx;
	broken[nbroken].ty = (int16_t)ty;
	nbroken++;
	const uint32_t bit = (uint32_t)ty * (uint32_t)map_w + (uint32_t)tx;
	broken_bits[bit >> 3] |= (uint8_t)(1u << (bit & 7));
	if (wall_defer) { if (nwall_pending < 32) wall_pending[nwall_pending++] = (int16_t)(nbroken - 1); return 1; }
	wall_redraw(tx, ty);
	return 1;
}
/* the drawing part of a wall shot away: CLEAN and both frame buffers' lists */
static void wall_redraw(int tx, int ty)
{
	const int x0 = tx * 10, y0 = ty * 10;
	if (clean_valid)
	{
		int a = x0, b = y0, e = x0 + 10, f = y0 + 10;
		if (a < clean_vx) a = clean_vx;
		if (b < clean_vy) b = clean_vy;
		if (e > clean_vx + VIEW_W) e = clean_vx + VIEW_W;
		if (f > clean_vy + PLAY_H) f = clean_vy + PLAY_H;
		compose_clean(a, b, e, f);
	}
	for (int k = 0; k < 2; k++)
	{
		if (bufs[k].nrects >= MAX_RECTS) { bufs[k].overflow = 1; continue; }
		bufs[k].rects[bufs[k].nrects++] = (Rect){ (int16_t)x0, (int16_t)y0, (int16_t)(x0 + 10), (int16_t)(y0 + 10) };
	}
}

/* ------------------------------------------------------------------ sprites */
static uint8_t rowbuf[512];
static uint32_t px_sprites;

/* frame f of a picture with its full frame's top-left at world (x, y), mirrored or not;
 * clipped to the view; the rectangle goes into this frame's list. over: redraw wall tiles
 * over it afterwards (sprites in the level) - the HUD is not */
static int over_list_n;
static Rect over_list[MAX_RECTS];
FAST static void draw_frame(const Frame *f, int fw, int x, int y, int mirror, int over)
{
	if (!f->data) return;
	const int bx = mirror ? x + fw - (f->x0 + f->w) : x + f->x0;
	const int by = y + f->y0;
	const int vx0 = view_x, vx1 = view_x + VIEW_W, vy0 = view_y, vy1 = view_y + PLAY_H;
	if (bx >= vx1 || by >= vy1 || bx + f->w <= vx0 || by + f->h <= vy0) return;
	const int r0 = by < vy0 ? vy0 - by : 0;
	const int r1 = by + f->h > vy1 ? vy1 - by : f->h;
	for (int r = r0; r < r1; r++)
	{
		const uint8_t *p = f->data + rd32(f->data + 4 * r);
		int runs = rd16(p);
		p += 2;
		int col = 0;
		const int wy = by + r;
		while (runs--)
		{
			const int skip = rd16(p), len = rd16(p + 2);
			const uint8_t *s = p + 4;
			p = s + len + (len & 1);
			col += skip;
			int a, e;
			if (!mirror) { a = bx + col; e = a + len; }
			else { e = bx + f->w - col; a = e - len; }
			col += len;
			if (e <= vx0 || a >= vx1) continue;
			const uint8_t *src = s;
			if (mirror)
			{
				for (int i = 0; i < len; i++) rowbuf[i] = s[len - 1 - i];
				src = rowbuf;
			}
			if (a < vx0) { src += vx0 - a; a = vx0; }
			if (e > vx1) e = vx1;
			fb_put(lin_of(a, wy), src, e - a, 0);
			px_sprites += (uint32_t)(e - a);
		}
	}
	rect_add(bx, by, bx + f->w, by + f->h);
	if (over && over_list_n < MAX_RECTS)
		over_list[over_list_n++] = (Rect){ (int16_t)bx, (int16_t)by, (int16_t)(bx + f->w), (int16_t)(by + f->h) };
}

/* a picture's frame at full-size position (fx, fy) = Hurrican's xPos / yPos (8.8 fixed) */
static void draw_pic(const Pic *p, int frame, int32_t fx, int32_t fy, int mirror)
{
	if (!p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	draw_frame(&f, p->fw, (int)(fx >> 9), (int)(fy >> 9), mirror, 1);
}

/* ------------------------------------------------------------------ HUD text */
#include "mars/font3x5.h"

/* P1m: the HUD is drawn into its band (hud_into_band) only when what it shows changes; each
 * frame the band's rows are copied to the top of the screen */
static uint8_t hud_band[HUD_H][SCREEN_W];
static int hud_into_band;
static void band_put(int sx, int sy, const uint8_t *src, int w, int keyed)
{
	if (sy < 0 || sy >= HUD_H) return;
	for (int x = 0; x < w; x++)
	{
		const int bx = sx + x;
		if (bx < 0 || bx >= SCREEN_W || (keyed && !src[x])) continue;
		hud_band[sy][bx] = src[x];
	}
}

/* a line of text at screen (sx, sy): 3x5 font doubled, with a dark shadow */
static void hud_text(int sx, int sy, const char *t, int colour)
{
	const int n = (int)strlen(t);
	const int w = n * 8 + 2;
	for (int ry = 0; ry < 11; ry++)
	{
		memset(rowbuf, 0, (size_t)w);
		for (int c = 0; c < n; c++)
		{
			int ch = t[c];
			if (ch >= 'a' && ch <= 'z') ch -= 32;
			if (ch < 32 || ch > 95) ch = '?';
			const uint16_t g = font3x5[ch - 32];
			for (int pass = 0; pass < 2; pass++)            /* shadow (1 down, 1 right), then the letter */
			{
				const int gy = (ry - pass) / 2;
				if (ry - pass < 0 || gy >= 5) continue;
				for (int gx = 0; gx < 3; gx++)
					if (g & (1u << (14 - (gy * 3 + gx))))
					{
						const int x = c * 8 + gx * 2 + pass;
						const uint8_t v = pass ? (uint8_t)col_dark : (uint8_t)colour;
						if (pass == 0 || !rowbuf[x]) rowbuf[x] = v;
						if (pass == 0 || !rowbuf[x + 1]) rowbuf[x + 1] = v;
					}
			}
		}
		if (hud_into_band) band_put(sx, sy + ry, rowbuf, w, 1);
		else fb_put(lin_of(cam_x + sx, cam_y + sy + ry), rowbuf, w, 1);
	}
	if (!hud_into_band) rect_add(cam_x + sx, cam_y + sy, cam_x + sx + w, cam_y + sy + 11);
}

/* a filled box on the screen */
static void hud_box(int sx, int sy, int w, int h, int colour)
{
	memset(rowbuf, colour, (size_t)w);
	for (int y = 0; y < h; y++)
		if (hud_into_band) band_put(sx, sy + y, rowbuf, w, 0);
		else fb_put(lin_of(cam_x + sx, cam_y + sy + y), rowbuf, w, 0);
	if (!hud_into_band) rect_add(cam_x + sx, cam_y + sy, cam_x + sx + w, cam_y + sy + h);
}

static char *put_dec(char *p, uint32_t v, int digits)
{
	char t[12];
	int n = 0;
	do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
	while (n < digits) t[n++] = '0';
	while (n) *p++ = t[--n];
	*p = 0;
	return p;
}
static char *put_str(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

/* ------------------------------------------------------------------ game: fixed point */
typedef int32_t fx;
#define FX(v)      ((fx)((v) * 256))
#define SYNC(v)    ((v) / 6)                     /* one 60 Hz step of a speed in Hurrican's units */
/* one difficulty: Hurrican's MEDIUM - the map's objects for it (TileEngine::LoadLevel:
 * LoadObject.Skill <= Skill), the damage the player takes x 0.6 (Player::DamagePlayer), the
 * enemies' own skill-dependent timings (P1g had the easy object set: too empty) */
#define MAP_SKILL 1
static int skill = 1;
static const int16_t skill_mul[3] = { 77, 154, 256 };   /* x 0.3 / 0.6 / 1.0, 8.8 */
#define DMG(v)     ((fx)((v) * skill_mul[skill]))       /* damage at once */
#define DMG_T(v)   ((fx)((v) * skill_mul[skill] / 6))   /* damage per step (Timer.sync(v)) */

typedef struct { int16_t l, t, r, b; } Box;      /* Hurrican's collision rectangles */

static uint32_t rng_state = 0x1234567u;
static int rnd(int n) { rng_state = rng_state * 1103515245u + 12345u; return n > 0 ? (int)((rng_state >> 16) % (uint32_t)n) : 0; }
static int isqrt(int v) { int r = 0, b = 1 << 30; if (v <= 0) return 0; while (b > v) b >>= 2; while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; } return r; }

/* sin / cos of whole degrees, 8.8 */
static int16_t sintab[360];
static int sin_d(int d) { d %= 360; if (d < 0) d += 360; return sintab[d]; }
static int cos_d(int d) { return sin_d(d + 90); }
static void make_sintab(void)
{
	/* a rotation by one degree (cos 1 = 0.9998477, sin 1 = 0.0174524), in 30-bit fixed point */
	int64_t c = 1073741824, s = 0;
	const int64_t k = 1073578304, q = 18739413;
	for (int d = 0; d < 360; d++)
	{
		sintab[d] = (int16_t)((s + (1 << 21)) >> 22);
		const int64_t c2 = (c * k - s * q) >> 30, s2 = (s * k + c * q) >> 30;
		c = c2; s = s2;
	}
}

/* ------------------------------------------------------------------ the tile engine's tests
 * (Tileengine.cpp: BlockLinks, BlockRechts, BlockOben, BlockUnten(Normal), BlockSlopes), in
 * whole pixels; x, y = the object's position, xo / yo its last one */
static int blk_right(int x, int y, int xo, const Box *r, int *rx)
{
	if (xo > x) return 0;
	const int xl = (x + r->r + 1) / 20;
	int b = 0;
	for (int j = r->t; j < r->b; j += 20)
	{
		const int yl = (y + j) / 20;
		if (y + j < 0) continue;
		if (yl >= map_h) break;
		const int nb = blk(xl, yl);
		if (nb) b = nb;
		if (b & B_WALL) { if (rx) *rx = xl * 20 - r->r - 1; return b; }
	}
	return b;
}
static int blk_left(int x, int y, int xo, const Box *r, int *rx)
{
	if (xo < x) return 0;
	const int xs = x + r->l - 1;
	const int xl = xs < 0 ? -1 : xs / 20;
	int b = 0;
	for (int j = r->t; j < r->b; j += 20)
	{
		const int yl = (y + j) / 20;
		if (y + j < 0) continue;
		if (yl >= map_h) break;
		const int nb = blk(xl, yl);
		if (nb) b = nb;
		if (b & B_WALL) { if (rx) *rx = xl * 20 + 20 - r->l; return b; }
	}
	return b;
}
static int blk_top(int x, int y, int yo, const Box *r, int *ry)
{
	if (yo < y) return 0;
	const int ys = y + r->t - 1;
	if (ys < 0) return 0;
	const int yl = ys / 20;
	if (yl >= map_h) return 0;
	int b = 0;
	for (int i = r->l; i < r->r; i += 20)
	{
		const int xl = (x + i) / 20;
		if (x + i < 0) continue;
		if (xl >= map_w) break;
		const int nb = blk(xl, yl);
		if (nb) b = nb;
		if (b & B_WALL) { if (ry) *ry = yl * 20 + 20 - r->t; return b; }
	}
	return b;
}
/* BlockUnten: walls and platforms under the rectangle; with ry: where to stand */
static int blk_below(int x, int y, int yo, const Box *r, int *ry)
{
	if (yo > y) return 0;
	const int ys = y + r->b + 1;
	if (ys < 0) return 0;
	const int yl = ys / 20;
	if (yl >= map_h) return 0;
	int b = 0;
	/* Hurrican tests pixel by pixel; tile by tile gives the same answer */
	int xa = x + r->l, xe = x + r->r - 1;
	if (xa < 0) xa = 0;
	if (xe < xa) return 0;
	for (int xl = xa / 20, x1 = xe / 20; xl <= x1 && xl < map_w; xl++)
	{
		const int nb = blk(xl, yl);
		if (nb) b = nb;
		if (b & (B_WALL | B_PLATFORM)) { if (ry) *ry = yl * 20 - r->b; return b; }
	}
	return b;
}
/* BlockSlopes: standing on a slope; sets *y */
static int blk_slopes(int x, int *y, const Box *r, int yspeed)
{
	int lastyl = -9999;
	for (int j = r->b; j < r->b + 20; j++)
	{
		const int yy = *y + j - 1;
		if (yy < 0) continue;
		const int yl = yy / 20;
		if (yl >= map_h) break;
		if (yl == lastyl) continue;          /* the result depends only on the tile row */
		lastyl = yl;
		/* Hurrican tests pixel by pixel (left to right for slopes rising to the left, right to
		 * left for the others); in each tile only the first pixel tested can give the answer
		 * (the height only grows from there), so one test per tile */
		int xa = x + r->l, xe = x + r->r - 1;
		if (xa < 0) xa = 0;
		for (int xl = xa / 20; xa <= xe && xl < map_w; xl++, xa = xl * 20)
		{
			const int cb = blk(xl, yl);
			if (cb & B_SLOPE_L)
			{
				const int ny = (yl + 1) * 20 - r->b - (20 - xa % 20) - 1;
				if (yspeed == 0 || *y > ny) { *y = ny; return cb; }
			}
		}
		xe = x + r->r;
		xa = x + r->l + 1;
		if (xa < 0) xa = 0;
		for (int xl = xe / 20; xe >= xa && xe >= 0; xl--, xe = xl * 20 + 19)
		{
			if (xl >= map_w) continue;
			const int cb = blk(xl, yl);
			if (cb & B_SLOPE_R)
			{
				const int ny = (yl + 1) * 20 - r->b - (xe % 20) - 1;
				if (yspeed == 0 || *y > ny) { *y = ny; return cb; }
			}
		}
	}
	return 0;
}
/* any of the flags in the rectangle */
static int blk_in(int x, int y, const Box *r, int mask)
{
	int b = 0;
	for (int ty = (y + r->t) / 20; ty <= (y + r->b - 1) / 20; ty++)
		for (int tx = (x + r->l) / 20; tx <= (x + r->r - 1) / 20; tx++)
			b |= blk(tx, ty) & mask;
	return b;
}

static int boxes_meet(int ax, int ay, const Box *a, int bx, int by, const Box *b)
{
	return ax + a->l < bx + b->r && ax + a->r > bx + b->l && ay + a->t < by + b->b && ay + a->b > by + b->t;
}

/* ------------------------------------------------------------------ sound */
enum { SFX_SHOT = 1, SFX_EXPLODE, SFX_COLLECT, SFX_HURT, SFX_LAND, SFX_ESHOT, SFX_BIGEXPLODE, SFX_ONEUP, SFX_HIT, SFX_SECRET };
/* one effect per frame reaches the Sega CD: the most important one asked for */
static const uint8_t sfx_rank[11] = { 0, 1, 7, 5, 6, 2, 4, 10, 9, 3, 8 };
static int sfx_want;
static void sfx(int n) { if (sfx_rank[n] > sfx_rank[sfx_want]) sfx_want = n; }

/* ------------------------------------------------------------------ pictures */
static Pic P_idle, P_run, P_jump, P_jumpup, P_jumpdiag, P_crouch, P_shootup, P_shootdiag;
static Pic P_spread, P_spreadbig, P_flare, P_expl, P_explbig, P_smoke, P_laser, P_pflshot, P_dronebullet, P_canon;
/* the player's added weapons (Hurrican's resident pictures) */
static Pic P_plaser, P_plaserbig, P_bounce[3], P_grenade, P_powerline, P_blitz[4], P_blitzbeam, P_rad;
static Pic P_extras, P_diamant, P_oneup, P_powerblock, P_bruecke, P_plattform;
static Pic P_walker, P_guntower, P_spiderbomb, P_piranha, P_muecke, P_deckenturm, P_kugel[4], P_auge;
static Pic P_swimwalker, P_drone, P_pflanze, P_spitter, P_spitterbombe, P_nest;
#define K2(name, id, picture, ...) static Pic P_##name;
#include K2_LIST
#undef K2

static void k2_load(void);
static void load_pictures(void)
{
	P_idle = pic("p1_hurri_idle.png");     P_run = pic("p1_hurri_laufen.png");
	P_jump = pic("p1_hurri_jump.png");     P_jumpup = pic("p1_hurri_jumpup.png");
	P_jumpdiag = pic("p1_hurri_jumpschra"); P_crouch = pic("p1_hurri_crouch.p");
	P_shootup = pic("p1_hurri_shootup");   P_shootdiag = pic("p1_hurri_shootdia");
	P_spread = pic("spreadshot.png");      P_spreadbig = pic("spreadshot2.png");
	P_flare = pic("shotflare.png");        P_expl = pic("explosion-medium");
	P_explbig = pic("explosion-big.png");  P_smoke = pic("smoke.png");
	P_laser = pic("walker-laser.png");     P_pflshot = pic("pflanzeschuss.png");
	P_dronebullet = pic("dronebullet.png"); P_canon = pic("canonball.png");
	P_extras = pic("extras.png");          P_diamant = pic("diamant.png");
	P_oneup = pic("oneup.png");            P_powerblock = pic("powerblock.png");
	P_bruecke = pic("bruecke.png");        P_plattform = pic("plattform.png");
	P_walker = pic("enemy-walker.png");    P_guntower = pic("enemy-guntower.p");
	P_spiderbomb = pic("spiderbomb.png");  P_piranha = pic("piranha.png");
	P_muecke = pic("stahlmuecke.png");     P_deckenturm = pic("deckenturm.png");
	P_kugel[0] = pic("kugel4.png");        P_kugel[1] = pic("kugel3.png");
	P_kugel[2] = pic("kugel2.png");        P_kugel[3] = pic("kugel1.png");
	P_auge = pic("auge.png");              P_swimwalker = pic("swimwalker.png");
	P_drone = pic("drone.png");            P_pflanze = pic("pflanze.png");
	P_spitter = pic("spitter.png");        P_spitterbombe = pic("spitterbombe.png");
	P_nest = pic("nest.png");
	P_plaser = pic("laser.png");           P_plaserbig = pic("laserbig.png");
	P_bounce[0] = pic("bounce1.png");      P_bounce[1] = pic("bounce2.png");
	P_bounce[2] = pic("bounce3.png");      P_grenade = pic("grenade.png");
	P_powerline = pic("powerline.png");    P_blitzbeam = pic("blitzbeam.png");
	P_blitz[0] = pic("blitzstrahl1.png");  P_blitz[1] = pic("blitzstrahl2.png");
	P_blitz[2] = pic("blitzstrahl3.png");  P_blitz[3] = pic("blitzstrahl4.png");
	P_rad = pic("p1_hurri_rad.png");
#define K2(name, id, picture, ...) P_##name = pic(picture);
#include K2_LIST
#undef K2
	k2_load();
}

/* ------------------------------------------------------------------ effects (explosions) */
#define MAX_FX 6                 /* explosions at once (P1g: was 12) */
typedef struct { const Pic *p; int32_t x, y; int16_t frame, ticks, per; } Fx;
static Fx fxs[MAX_FX];
/* an effect centred at full-size (cx, cy) */
static void fx_add(const Pic *p, int cx, int cy, int per)
{
	if (!p->count) return;
	int k = -1;
	for (int i = 0; i < MAX_FX; i++) if (!fxs[i].p) { k = i; break; }
	if (k < 0) k = rnd(MAX_FX);          /* full: replace one (fewer effects, as on the PC's low setting) */
	fxs[k].p = p;
	fxs[k].x = FX(cx - p->fw);           /* fw is half size: the full size is 2 * fw */
	fxs[k].y = FX(cy - p->fh);
	fxs[k].frame = 0;
	fxs[k].ticks = 0;
	fxs[k].per = (int16_t)per;
}
static void fx_tick(void)
{
	for (int i = 0; i < MAX_FX; i++)
	{
		Fx *f = &fxs[i];
		if (!f->p) continue;
		if (++f->ticks >= f->per)
		{
			f->ticks = 0;
			if (++f->frame >= f->p->count) f->p = 0;
		}
	}
}

/* ------------------------------------------------------------------ the player */
enum { PS_STAND, PS_RUN, PS_JUMP, PS_DUCK, PS_DEAD };
static const Box player_box = { 20, 12, 50, 79 };
static const Box player_box_duck = { 20, 35, 50, 79 };
typedef struct {
	fx x, y, xo, yo;           /* Hurrican's xpos / ypos (full-size pixels, 8.8) */
	fx xs, ys;                 /* speeds (units) */
	fx jumpadd;
	fx jumpstart;
	int dir;                   /* +1 right, -1 left */
	int state;
	int anim, animcount;       /* animcount in 1/6 units */
	int aim;                   /* 0 straight, 1 diagonal, 2 up */
	int jump_ok;               /* the jump button was let go since landing */
	int on_ground;
	int platform;              /* the entity stood on + 1, 0 = none */
	fx energy;                 /* 0..100 */
	int lives;
	uint32_t score;
	int gems;
	int sel;                   /* selected weapon 0 spread, 1 laser, 2 bounce */
	int wlevel[3];             /* each weapon's level 1..8 */
	int blitzlen;             /* the lightning's length (segments) = blitz level + 1 */
	int grenades, powerlines, smartbombs;
	int wpn_ok, grn_ok, spc_ok, bomb_ok;   /* X / Y / Z / (wheel B) pressed since let go (edge) */
	int blitz;                 /* 0 none, 1 lightning (A), 2 loading the beam (A+B) */
	int blitz_t;               /* steps held (ramp / charge) */
	int blitz_ang;             /* the lightning's angle, whole degrees (0 right, 180 left, 270 up) */
	int blitz_anim;            /* which blitzstrahl frame */
	int blitz_reach;           /* segments actually drawn this step (stopped by a wall) */
	int wheel;                 /* 0 none, 1 rolling, 2 rolling and falling */
	int wheel_t;               /* the wheel's "armour": steps left before it ends */
	int shotdelay;             /* steps until the next shot */
	int hurt;                  /* steps the hurt flash is shown */
	int safe_t;                /* steps of safety after coming back (no damage, blinks) */
	int dead_t;                /* steps since death */
	int idle_t;
	fx safe_x, safe_y;         /* the last place stood on safely (to come back to) */
	fx ground_y;               /* the last height stood at (JumpySave: how high a jump went) */
	int in_liquid;
} Player;
static Player pl;

/* ------------------------------------------------------------------ shots */
#define MAX_SHOTS 40
enum { SH_SPREAD, SH_LASER, SH_PFLANZE, SH_BULLET, SH_CANON, SH_PIC,
       SH_PLASER, SH_BOUNCE, SH_GRENADE, SH_POWERLINE, SH_BEAM };
/* gen: bounce generation 0/1/2 (bounce1/2/3); bounce_walls: reflects off walls instead of
 * splitting; big: the giant variant (RiesenShot - unused here, kept for shape) */
typedef struct { uint8_t on, kind, enemy, anim, rot, gen, bounce_walls, big; fx x, y, xs, ys, ya; int16_t dmg, life; const Pic *pic; Box box; } Shot;
static Shot shots[MAX_SHOTS];
static const Box shot_box[5] = { { 4, 4, 12, 12 }, { 0, 0, 32, 3 }, { 2, 2, 14, 14 }, { 2, 2, 10, 10 }, { 1, 1, 7, 7 } };

static Shot *shot_new(void)
{
	for (int i = 0; i < MAX_SHOTS; i++) if (!shots[i].on) { memset(&shots[i], 0, sizeof shots[i]); shots[i].on = 1; return &shots[i]; }
	return 0;
}
/* at most this many enemy shots at once (P1g, for the frame rate) */
#define MAX_ENEMY_SHOTS 16
static int enemy_shots_full(void)
{
	int n = 0;
	for (int i = 0; i < MAX_SHOTS; i++) n += shots[i].on && shots[i].enemy;
	return n >= MAX_ENEMY_SHOTS;
}
static void enemy_shot(int kind, int x, int y, fx xs, fx ys, int dmg)
{
	if (enemy_shots_full()) return;
	Shot *s = shot_new();
	if (!s) return;
	s->kind = (uint8_t)kind; s->enemy = 1;
	s->x = FX(x); s->y = FX(y); s->xs = xs; s->ys = ys;
	s->dmg = (int16_t)dmg; s->life = 60 * 4;
	sfx(SFX_ESHOT);
}
/* an enemy shot drawn with picture p (its frames animate; mirrored when it flies right if
 * mirror), falling with ya (units 8.8 per step; 0 = straight), hitting with the middle half of
 * the picture; no sound when quiet. Returns the shot or 0 (all 40 in use). */
__attribute__((unused)) static Shot *enemy_shot_pic(const Pic *p, int x, int y, fx xs, fx ys, fx ya, int dmg, int quiet)
{
	if (enemy_shots_full()) return 0;
	Shot *s = shot_new();
	if (!s) return 0;
	s->kind = SH_PIC; s->enemy = 1; s->pic = p;
	s->x = FX(x); s->y = FX(y); s->xs = xs; s->ys = ys; s->ya = ya;
	s->dmg = (int16_t)dmg; s->life = 60 * 4;
	const int w = 2 * p->fw, h = 2 * p->fh;          /* full size */
	s->box = (Box){ (int16_t)(w / 4), (int16_t)(h / 4), (int16_t)(w - w / 4), (int16_t)(h - h / 4) };
	if (!quiet) sfx(SFX_ESHOT);
	return s;
}

/* ------------------------------------------------------------------ entities */
enum {
	E_NONE, E_DIAMANT, E_ONEUP, E_POWERBLOCK, E_EXTRA, E_SPITTER, E_WALKER, E_GUNTOWER, E_SPIDERBOMB,
	E_PIRANHA, E_MUECKE, E_DECKENTURM, E_KUGEL, E_AUGE, E_SWIMWALKER, E_DRONE, E_PFLANZE,
	E_SPITTERBOMBE, E_NEST, E_BRUECKE, E_PLATTFORM, E_ENDLEVEL, E_SECRET,
	/* the added kinds (game/k2/: one line each in a list, the code beside it) */
#define K2(name, id, picture, l_, t_, r_, b_, en, ae, ap, to, sc, mi, tb, sh) E_##name,
#include K2_LIST
#undef K2
	E_N
};
typedef struct {
	uint8_t type, active, dir, state;   /* dir: 0 left, 1 right */
	uint8_t size, hit, blocks, spare;   /* size: balls 0..3; hit: steps of the hit flash */
	int16_t energy;                     /* hit points (Hurrican's Energy) */
	int16_t anim, animt;                /* frame, steps on it */
	int16_t t1, t2;                     /* timers */
	int16_t v1, v2;                     /* the map's values */
	int32_t x, y;                       /* 8.8, full size, top-left (xPos / yPos) */
	int32_t xo, yo;                     /* the last step's */
	int16_t xs, ys, xa, ya;             /* speeds / accelerations, units 8.8 (up to 127) */
} Ent;
#define MAX_ENTS 1300
static Ent ents[MAX_ENTS];
static int nents_map, nents;
static int16_t active_list[MAX_ENTS], targets[MAX_ENTS];   /* targets: shootable ones near the screen */
static int nactive, ntargets;

typedef struct {
	const Pic *pic;
	Box box;
	int16_t energy;
	int16_t anim_end;      /* frames looped 0..anim_end */
	int16_t anim_per;      /* steps per frame (AnimSpeed * 6) */
	int16_t touch;         /* damage per step while touching (Timer.sync(v)) x 100 */
	int16_t score;
	uint8_t mirror;        /* drawn mirrored when facing right */
	uint8_t testblock;     /* the base movement stops at walls */
	uint8_t shootable;
	uint8_t boss;          /* a stage boss (map objects 120-139): never held back by the enemy budget */
} Kind;
static Kind kinds[E_N];
static uint8_t id_type[256];       /* the map's object number -> an added kind (0 = none) */

static void kinds_init(void)
{
#define K(t, p, l_, t_, r_, b_, en, ae, ap, to, sc, mi, tb, sh) \
	kinds[t] = (Kind){ p, { l_, t_, r_, b_ }, en, ae, ap, to, sc, mi, tb, sh, 0 }
	/*      type          picture          box                energy end per touch score mirror test shoot */
	K(E_DIAMANT,     &P_diamant,      0, 0, 29, 29,       1,  8,  4,   0,   0, 0, 0, 0);
	K(E_ONEUP,       &P_oneup,        0, 0, 40, 36,       1, 19,  3,   0,   0, 0, 1, 0);
	K(E_POWERBLOCK,  &P_powerblock,   0, 0, 40, 40,     100,  0,  6,   0,  10, 0, 0, 1);
	K(E_EXTRA,       &P_extras,       0, 0, 24, 24,       1,  0,  6,   0,   0, 0, 1, 0);
	K(E_SPITTER,     &P_spitter,      0, 0, 22, 23,      20,  4,  8, 100,  50, 1, 1, 1);
	K(E_WALKER,      &P_walker,       0, 10, 45, 45,     10, 11,  3, 400, 100, 1, 1, 1);
	K(E_GUNTOWER,    &P_guntower,     0, 0, 60, 40,      50,  0,  6,   0, 200, 1, 0, 1);
	K(E_SPIDERBOMB,  &P_spiderbomb,   0, 0, 50, 34,      30,  5,  5,   0, 250, 1, 1, 1);
	K(E_PIRANHA,     &P_piranha,      0, 0, 45, 29,      30,  9,  5, 100, 200, 1, 1, 1);
	K(E_MUECKE,      &P_muecke,       0, 0, 34, 35,      10,  9,  3, 150, 200, 1, 0, 1);
	K(E_DECKENTURM,  &P_deckenturm,   0, 0, 38, 40,      50,  0,  6,   0, 200, 0, 0, 1);
	K(E_KUGEL,       &P_kugel[0],     4, 4, 26, 26,      20, 19,  2, 200, 300, 0, 1, 1);
	K(E_AUGE,        &P_auge,         8, 0, 52, 26,       1,  5,  3,   0, 100, 1, 0, 1);
	K(E_SWIMWALKER,  &P_swimwalker,   5, 5, 60, 40,      25,  9,  9, 400, 100, 1, 1, 1);
	K(E_DRONE,       &P_drone,        5, 5, 65, 75,     125,  7,  5,   0, 300, 1, 0, 1);
	K(E_PFLANZE,     &P_pflanze,     15, 20, 95, 90,     80,  9, 10, 150, 300, 1, 0, 1);
	K(E_SPITTERBOMBE,&P_spitterbombe, 2, 0, 27, 50,      20,  5,  3, 100, 200, 0, 1, 1);
	K(E_NEST,        &P_nest,         8, 8, 52, 60,     120,  0,  6,  50, 500, 0, 0, 1);
	K(E_BRUECKE,     &P_bruecke,      0, 0, 10, 10,       1,  0,  6,   0,   0, 0, 0, 0);
	K(E_PLATTFORM,   &P_plattform,    0, 0, 128, 32,      1,  0,  6,   0,   0, 0, 0, 0);
	K(E_ENDLEVEL,    0,               0, 0, 120, 120,     1,  0,  6,   0,   0, 0, 0, 0);
	K(E_SECRET,      0,               0, 0, 40, 40,       1,  0,  6,   0,   0, 0, 0, 0);
#undef K
#define K2(name, id, picture, l_, t_, r_, b_, en, ae, ap, to, sc, mi, tb, sh) \
	kinds[E_##name] = (Kind){ &P_##name, { l_, t_, r_, b_ }, en, ae, ap, to, sc, mi, tb, sh, (id) >= 120 && (id) <= 139 }; \
	if ((id) >= 0 && (id) < 256) id_type[id] = E_##name;
#include K2_LIST
#undef K2
}

static const Box *ent_box(const Ent *e)
{
	static const Box kugel_box[4] = { { 4, 4, 26, 26 }, { 8, 8, 37, 37 }, { 12, 12, 48, 48 }, { 16, 16, 74, 74 } };
	return e->type == E_KUGEL ? &kugel_box[e->size] : &kinds[e->type].box;
}
static const Pic *ent_pic(const Ent *e) { return e->type == E_KUGEL ? &P_kugel[e->size] : kinds[e->type].pic; }

static Ent *ent_new(int type, int x, int y)
{
	int k = -1;
	for (int i = nents_map; i < nents; i++) if (ents[i].type == E_NONE) { k = i; break; }
	if (k < 0) { if (nents >= MAX_ENTS) return 0; k = nents++; }
	Ent *e = &ents[k];
	memset(e, 0, sizeof *e);
	e->type = (uint8_t)type;
	e->x = e->xo = FX(x);
	e->y = e->yo = FX(y);
	e->energy = kinds[type].energy;
	return e;
}

/* the player's centre and the entity's, full size */
static int pcx(void) { return (pl.x >> 8) + 35; }
static int pcy(void) { return (pl.y >> 8) + 40; }
static int ecx(const Ent *e) { const Box *b = ent_box(e); return (e->x >> 8) + (b->l + b->r) / 2; }
static int ecy(const Ent *e) { const Box *b = ent_box(e); return (e->y >> 8) + (b->t + b->b) / 2; }
static int player_distance(const Ent *e)
{
	const Box *b = ent_box(e);
	const int dx = pcx() - ((e->x >> 8) + b->r / 2), dy = pcy() - ((e->y >> 8) + b->b / 2);
	return isqrt(dx * dx + dy * dy);
}

static void player_damage(fx amount);
static void ent_explode(Ent *e);

/* the player can stand on the entity (PlattformTest): returns 1 while he does */
static void ent_platform(Ent *e, int idx)
{
	if (pl.state == PS_DEAD) return;
	const Box *b = ent_box(e);
	const int ex = e->x >> 8, ey = e->y >> 8;
	const int px = pl.x >> 8, py = pl.y >> 8, pyo = pl.yo >> 8;
	if (pl.platform == idx + 1) return;
	if (pl.ys < 0) return;
	const int len = (py > pyo ? py - pyo : pyo - py) + 2;
	for (int i = 0; i < len; i++)
	{
		const int foot = pyo + player_box.b + i + 1;
		if (foot >= ey + b->t && foot <= ey + b->t + 10 &&
		    px + player_box.l <= ex + b->r && px + player_box.r >= ex + b->l)
		{
			pl.platform = idx + 1;
			pl.y = FX(ey + b->t - player_box.b - 1);
			return;
		}
	}
}

static void spawn_extra(int x, int y, int type)
{
	Ent *e = ent_new(E_EXTRA, x, y);
	if (!e) return;
	e->v1 = (int16_t)type;
	e->anim = (int16_t)type;
	e->state = 1;                      /* jumping out */
	e->ys = FX(-20);
	e->ya = FX(5);
	e->xs = FX(rnd(2) ? 4 : -4);
	e->active = 1;
}

static void spawn_kugel(int x, int y, int size, int xs)
{
	Ent *e = ent_new(E_KUGEL, x, y);
	if (!e) return;
	static const int16_t en[4] = { 20, 30, 40, 50 };
	e->size = (uint8_t)size;
	e->energy = en[size];
	e->xs = FX(xs);
	e->ya = FX(5);
	e->active = 1;
}

/* the added kinds' functions (game/k2/, included before ent_step) */
static int stage_clear_t, secret_t;
static int demo;                        /* the demo is playing (options menu) */
/* a kind can hold the camera (Hurrican's elevator: the screen is fixed to the lift): set
 * cam_lock = 1 and the view's top-left in full-size pixels every step; world_step clears it */
static int cam_lock, cam_lock_x, cam_lock_y, cam_lock_held;
static int stage;                       /* the stage played (set below) */
static int dmg_src, dmg_src_shot;       /* PC check (HB_DMG): what hurt the player */

/* ---- the bosses (game/k2/: Hurrican's TileEngine scrolling, HUD boss bar, boss music) */
/* Hurrican's window (640 x 480, XOffset / YOffset): this screen shows its lower 400 lines */
#define CAM_DY 80
__attribute__((unused)) static int cam_xoff(void) { return cam_x * 2; }
__attribute__((unused)) static int cam_yoff(void) { return cam_y * 2 - CAM_DY; }
/* the screen held on a boss's arena: Hurrican's ScrollLevel(x, y, ZUSTAND_SCROLLTOLOCK) - the
 * camera goes there (at speed / 60 Hz step, full-size pixels) and stays until boss_unlock()
 * (Hurrican's ZUSTAND_SCROLLTOPLAYER / SCROLLBAR) */
static int boss_lock_on, boss_lock_x, boss_lock_y, boss_lock_speed;
__attribute__((unused)) static void boss_lock(int xoff, int yoff, int speed)
{
	boss_lock_on = 1; boss_lock_x = xoff; boss_lock_y = yoff + CAM_DY; boss_lock_speed = speed > 0 ? speed : 10;
}
__attribute__((unused)) static void boss_unlock(void) { boss_lock_on = 0; }
__attribute__((unused)) static int boss_locked_there(void)                  /* the camera has arrived (ZUSTAND_LOCKED) */
{
	return boss_lock_on && cam_x == boss_lock_x / 2 && cam_y == boss_lock_y / 2;
}
/* the boss's energy bar under the HUD: call every step while it shows */
static int boss_bar_e, boss_bar_max, boss_bar_t;
__attribute__((unused)) static void boss_bar(int energy, int max) { boss_bar_e = energy; boss_bar_max = max > 0 ? max : 1; boss_bar_t = 3; }
static void music(int track);
static void music_boss(void);
static void music_stage(void);
__attribute__((unused)) static void stage_clear(void) { if (!stage_clear_t) stage_clear_t = 1; }
#define K2(name, id, ...) \
	static void name##_load(void); \
	static void name##_init(Ent *e); \
	static int name##_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching); \
	static int name##_explode(Ent *e); \
	static int name##_draw(const Ent *e, uint32_t t);
#include K2_LIST
#undef K2

/* entity e dies (Energy <= 0): what Hurrican's GegnerExplode does */
static void ent_explode(Ent *e)
{
#ifndef OJ32X_MARS
	if (getenv("HB_DMG")) fprintf(stderr, "KILL %d\n", e->type);
#endif
	switch (e->type)
	{
#define K2(name, id, ...) case E_##name: if (name##_explode(e)) { pl.score += (uint32_t)kinds[e->type].score; if (pl.platform && &ents[pl.platform - 1] == e) pl.platform = 0; e->type = E_NONE; return; } break;
#include K2_LIST
#undef K2
	default: break;
	}
	const int cx = ecx(e), cy = ecy(e), x = e->x >> 8, y = e->y >> 8;
	pl.score += (uint32_t)kinds[e->type].score;
	switch (e->type)
	{
	case E_KUGEL:
		if (e->size > 0)
		{
			const int o = 4 * e->size;
			spawn_kugel(x + o, y + o, e->size - 1, -8);
			spawn_kugel(x + o, y + o, e->size - 1, 8);
		}
		fx_add(e->size >= 2 ? &P_explbig : &P_expl, cx, cy, 2);
		sfx(SFX_EXPLODE);
		break;
	case E_AUGE:
		spawn_extra(x + 10, y, rnd(3) ? 0 : 5);
		fx_add(&P_expl, cx, cy, 2);
		sfx(SFX_EXPLODE);
		break;
	case E_DRONE: case E_NEST: case E_PFLANZE: case E_GUNTOWER: case E_DECKENTURM:
		fx_add(&P_explbig, cx, cy, 2);
		sfx(SFX_BIGEXPLODE);
		break;
	case E_SPITTERBOMBE:
		for (int k = 0; k < 2; k++)
		{
			Ent *s = ent_new(E_SPITTER, x + 2, y + 25);
			if (s) { s->active = 1; s->ys = FX(-25 - 10 * k); s->xs = FX(k ? 12 : -12); s->ya = FX(5); s->dir = (uint8_t)k; }
		}
		fx_add(&P_expl, cx, cy, 2);
		sfx(SFX_EXPLODE);
		break;
	case E_POWERBLOCK:
		fx_add(&P_expl, cx, cy, 2);
		sfx(SFX_EXPLODE);
		break;
	default:
		fx_add(&P_expl, cx, cy, 2);
		sfx(SFX_EXPLODE);
		break;
	}
	if (pl.platform && &ents[pl.platform - 1] == e) pl.platform = 0;
	e->type = E_NONE;
}

static void anim_loop(Ent *e, int start, int end, int per)
{
	if (++e->animt >= per)
	{
		e->animt = 0;
		if (++e->anim > end || e->anim < start) e->anim = (int16_t)start;
	}
}

/* aimed shot speed: towards the player, length v units */
static void aim_at(int fromx, int fromy, int v, fx *xs, fx *ys)
{
	const int dx = pcx() - fromx, dy = pcy() - fromy;
	int d = isqrt(dx * dx + dy * dy);
	if (d < 1) d = 1;
	*xs = FX(v) * dx / d;
	*ys = FX(v) * dy / d;
}

/* the added kinds' code */
/* swarms awake at once (the user's request): bats at most MAX_BATS, the bee-like ones (the
 * mosquito MUECKE, the small wasp) at most MAX_BEES */
#define MAX_BATS 3
#define MAX_BEES 3
static __attribute__((unused)) int bats_awake(void)
{
	int n = 0;
	for (int i = 0; i < nents; i++) n += ents[i].type == E_FLEDERMAUS && ents[i].active;
	return n;
}
#include K2_CODE
static void k2_load(void)
{
#define K2(name, id, ...) name##_load();
#include K2_LIST
#undef K2
}

/* one 60 Hz step of an active entity: its behaviour (DoKI), then the base movement (Run) */
static void ent_step(Ent *e, int idx)
{
	const Kind *k = &kinds[e->type];
	const Box *b = ent_box(e);
	int x = e->x >> 8, y = e->y >> 8;
	const int xo = e->xo >> 8, yo = e->yo >> 8;
	int bl = 0, br = 0, bo = 0, bu = 0;
	if (k->testblock)
	{
		bl = blk_left(x, y, xo, b, 0);
		br = blk_right(x, y, xo, b, 0);
		bo = blk_top(x, y, yo, b, 0);
		bu = blk_below(x, y, yo, b, 0);
	}
	const int touching = pl.state != PS_DEAD &&
		boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box, x, y, b);

	switch (e->type)
	{
	case E_DIAMANT:
		anim_loop(e, 0, 8, 4);
		if (touching)
		{
			pl.gems++;
			pl.score += 100;
			if (pl.gems % 100 == 0) { pl.lives++; sfx(SFX_ONEUP); }
			else sfx(SFX_COLLECT);
			fx_add(&P_flare, ecx(e), ecy(e), 3);
			e->type = E_NONE;
			return;
		}
		return;

	case E_ONEUP:
		anim_loop(e, 0, 19, 3);
		if (e->state == 0) { e->ya = FX(2); if (e->ys > FX(20)) e->ys = FX(20); }
		if (bu & (B_WALL | B_PLATFORM)) { e->ys = 0; e->ya = 0; e->state = 1; }
		if (touching) { pl.lives++; sfx(SFX_ONEUP); e->type = E_NONE; return; }
		break;

	case E_EXTRA:
		if (e->state == 1)
		{
			if (e->ys > FX(20)) e->ys = FX(20);
			if (e->ys > 0 && (bu & (B_WALL | B_PLATFORM))) { e->state = 2; e->ys = 0; e->ya = 0; e->xs = 0; }
			if (++e->t1 > 90) { e->state = 2; e->ys = 0; e->ya = 0; e->xs = 0; }
		}
		if (touching)
		{
			if (e->v1 == 5) pl.energy = FX(100);
			else if (pl.wlevel[pl.sel] < 8) pl.wlevel[pl.sel]++;
			else { if (pl.grenades < 9) pl.grenades++; if (pl.powerlines < 9) pl.powerlines++; }
			pl.score += 100;
			sfx(SFX_COLLECT);
			e->type = E_NONE;
			return;
		}
		break;

	case E_POWERBLOCK:
		/* invisible until shot (state 0); then a block to stand on that drops an extra every
		 * 12 points of damage, 4 in all */
		if (e->state) ent_platform(e, idx);
		if (e->energy <= 0)
		{
			static const int8_t seq[4] = { 0, 5, 0, 0 };
			spawn_extra(x + 10, y, e->v2 ? e->v2 : seq[e->t1 & 3]);
			pl.score += 10;
			e->t1++;
			if (e->t1 < 4) e->energy = 12;
			else { ent_explode(e); return; }
		}
		e->anim = 1;
		return;

	case E_SPITTER:
		anim_loop(e, 0, 4, 8);
		if (e->ys > FX(30)) e->ys = FX(30);
		if (bu & (B_WALL | B_PLATFORM))
		{
			if (e->ys > 0) e->ys = 0;
			e->ya = 0;
			e->xs = pcx() < ecx(e) ? FX(-12) : FX(12);
			e->dir = e->xs > 0;
			if (++e->t1 > 50 + rnd(40)) { e->t1 = 0; e->ys = FX(-30); e->ya = FX(5); }
		}
		else e->ya = FX(5);
		break;

	case E_WALKER:
		if (e->state != 3) anim_loop(e, e->state == 2 ? 12 : 0, e->state == 2 ? 20 : 11, 3);
		if (e->dir == 0 && (bl & (B_WALL | B_EWALL))) { e->dir = 1; e->xs = FX(10); }
		if (e->dir == 1 && (br & (B_WALL | B_EWALL))) { e->dir = 0; e->xs = FX(-10); }
		if (e->hit && e->state == 0) { e->dir = pcx() > ecx(e); e->xs = e->dir ? FX(10) : FX(-10); }
		if (e->state == 0)                                    /* walking */
		{
			if (!(bu & (B_WALL | B_PLATFORM))) { e->state = 1; e->ya = FX(4); }
			if (player_distance(e) <= 220 &&
			    ((e->dir == 0 && (pl.x >> 8) + 45 <= x) || (e->dir == 1 && (pl.x >> 8) - 45 >= x)))
			{
				if (--e->t1 <= 0)
				{
					e->t1 = (int16_t)(60 + rnd(30));
					e->state = 2; e->anim = 12; e->animt = 0; e->xs = 0;
				}
			}
		}
		else if (e->state == 1)                               /* falling */
		{
			if (e->ys > FX(25)) e->ys = FX(25);
			if (bu & (B_WALL | B_PLATFORM)) { e->state = 0; e->ya = 0; e->ys = 0; e->xs = e->dir ? FX(10) : FX(-10); }
		}
		else if (e->state == 2)                               /* shooting */
		{
			e->xs = 0;
			if (e->anim == 17 && e->animt == 0)
				enemy_shot(SH_LASER, e->dir ? x + 30 : x - 18, y + 23, e->dir ? FX(30) : FX(-30), 0, DMG(8));
			if (e->anim >= 20) { e->state = 0; e->anim = 0; e->xs = e->dir ? FX(10) : FX(-10); }
		}
		/* the player jumps on its head: it is squashed */
		if (pl.ys > 0 && pl.state == PS_JUMP && touching && (pl.y >> 8) + player_box.b < y + b->t + 16)
		{
			pl.ys = FX(-43); pl.jumpadd = 0; pl.jumpstart = pl.y;
			e->energy = 0;
		}
		break;

	case E_GUNTOWER:
		/* turns to the player and fires a cannon ball now and then */
		e->dir = pcx() > ecx(e);
		if (player_distance(e) < 500 && --e->t1 <= 0)
		{
			e->t1 = (int16_t)(70 + rnd(40));
			fx xs, ys;
			aim_at(e->dir ? x + 50 : x + 5, y + 15, 30, &xs, &ys);
			enemy_shot(SH_CANON, e->dir ? x + 50 : x + 2, y + 15, xs, ys, DMG(8));
		}
		e->anim = 0;
		break;

	case E_SPIDERBOMB:
		/* crawls towards the player, bursts on touching him */
		anim_loop(e, 0, 5, 5);
		if (e->ys > FX(30)) e->ys = FX(30);
		if (bu & (B_WALL | B_PLATFORM)) { if (e->ys > 0) e->ys = 0; e->ya = 0; }
		else e->ya = FX(5);
		if (player_distance(e) < 400) { e->dir = pcx() > ecx(e); e->xs = e->dir ? FX(14) : FX(-14); }
		if (touching) { player_damage(DMG(20)); e->energy = 0; }
		break;

	case E_PIRANHA:
	case E_SWIMWALKER:
		/* swims to and fro; turns at walls and at the water's edge */
		anim_loop(e, 0, e->type == E_PIRANHA ? 9 : 9, k->anim_per);
		if (e->xs == 0) e->xs = FX(e->type == E_PIRANHA ? -8 : -7);
		{
			const int ahead = e->xs > 0 ? x + b->r + 2 : x + b->l - 2;
			const int wet = blk(ahead / 20, (y + b->b / 2) / 20) & B_LIQUID;
			if ((e->xs < 0 && (bl & (B_WALL | B_EWALL))) || (e->xs > 0 && (br & (B_WALL | B_EWALL))) ||
			    (e->type == E_PIRANHA && !wet))
				e->xs = -e->xs;
		}
		e->dir = e->xs > 0;
		if (e->type == E_PIRANHA)
		{
			/* towards the player's height when he is in the water near it */
			if (pl.in_liquid && player_distance(e) < 300)
				e->ys = pcy() < ecy(e) ? FX(-4) : FX(4);
			else e->ys = 0;
			if (!(blk(ecx(e) / 20, (y + (e->ys < 0 ? b->t - 2 : b->b + 2)) / 20) & B_LIQUID)) e->ys = 0;
		}
		else if (player_distance(e) < 300 && --e->t1 <= 0)
		{
			e->t1 = (int16_t)(90 + rnd(60));
			fx xs, ys;
			aim_at(e->dir ? x + 55 : x - 25, y + 10, 25, &xs, &ys);
			enemy_shot(SH_BULLET, e->dir ? x + 55 : x - 25, y + 10, xs, ys, DMG(10));
		}
		break;

	case E_MUECKE:
		/* flies to a point near the player, picks a new one when there */
		anim_loop(e, 0, 9, 3);
		if (e->t1 <= 0 || (abs(e->v1 - x) < 30 && abs(e->v2 - y) < 30))
		{
			e->v1 = (int16_t)((pl.x >> 8) - 100 + rnd(200));
			e->v2 = (int16_t)((pl.y >> 8) - 100 + rnd(200));
			e->t1 = 120;
		}
		e->t1--;
		e->xa = e->v1 > x ? FX(3) : FX(-3);
		e->ya = e->v2 > y ? FX(3) : FX(-3);
		if (e->xs > FX(15)) e->xs = FX(15);
		if (e->xs < FX(-15)) e->xs = FX(-15);
		if (e->ys > FX(15)) e->ys = FX(15);
		if (e->ys < FX(-15)) e->ys = FX(-15);
		e->dir = e->xs > 0;
		break;

	case E_DECKENTURM:
		/* a turret in the ceiling: opens near the player, shoots, closes */
		if (e->state == 0)
		{
			e->anim = 0;
			if (player_distance(e) < 350) { e->state = 1; e->t1 = 0; }
		}
		else if (e->state == 1)                    /* opening */
		{
			if (++e->t1 % 6 == 0 && e->anim < 9) e->anim++;
			if (e->anim >= 9) { e->state = 2; e->t1 = 40; }
		}
		else if (e->state == 2)                    /* open: shoots */
		{
			const int dx = pcx() - ecx(e);
			e->anim = (int16_t)(dx < -120 ? 10 : dx < -40 ? 12 : dx < 40 ? 14 : dx < 120 ? 16 : 18);
			if (--e->t1 <= 0)
			{
				e->t1 = (int16_t)(45 + rnd(30));
				fx xs, ys;
				aim_at(x + 19, y + 30, 30, &xs, &ys);
				enemy_shot(SH_BULLET, x + 13, y + 30, xs, ys, DMG(6));
			}
			if (player_distance(e) > 450) { e->state = 3; e->t1 = 0; e->anim = 9; }
		}
		else                                       /* closing */
		{
			if (++e->t1 % 6 == 0 && e->anim > 0) e->anim--;
			if (e->anim <= 0) e->state = 0;
		}
		break;

	case E_KUGEL:
		/* spiked balls bounce through the corridors (Gegner_Kugel*.cpp) */
		if (++e->animt >= 2)
		{
			e->animt = 0;
			if (e->xs > 0) { if (++e->anim > 19) e->anim = 0; }
			else if (--e->anim < 0) e->anim = 19;
		}
		if (e->ys > FX(20)) e->ys = FX(20);
		if ((e->ys > 0 && (bu & (B_WALL | B_EWALL | B_PLATFORM))) || (e->ys < 0 && (bo & (B_WALL | B_EWALL))))
		{
			e->ys = -e->ys;
			if (e->ys < 0 && e->ys > FX(-18)) e->ys = FX(-18);
		}
		if ((e->xs < 0 && (bl & (B_WALL | B_EWALL))) || (e->xs > 0 && (br & (B_WALL | B_EWALL)))) e->xs = -e->xs;
		break;

	case E_AUGE:
		/* the flying eye: crosses the screen in waves; shot, it drops an extra */
		anim_loop(e, 0, 5, 3);
		if (e->xs == 0) e->xs = FX(12);
		if (e->ys > FX(10)) e->ya = FX(-4);
		if (e->ys < FX(-10)) e->ya = FX(4);
		if (e->ya == 0) e->ya = FX(4);
		e->dir = e->xs > 0;
		break;

	case E_DRONE:
		/* hovers above the player and fires down at both sides */
		anim_loop(e, 0, 7, 5);
		{
			const int tx = pcx() - 35, ty = (pl.y >> 8) - 180;
			e->xa = tx > x ? FX(2) : FX(-2);
			e->ya = ty > y ? FX(2) : FX(-2);
			if (e->xs > FX(14)) e->xs = FX(14);
			if (e->xs < FX(-14)) e->xs = FX(-14);
			if (e->ys > FX(10)) e->ys = FX(10);
			if (e->ys < FX(-10)) e->ys = FX(-10);
			if ((e->xs < 0 && (blk_left(x, y, x + 1, b, 0) & B_WALL)) || (e->xs > 0 && (blk_right(x, y, x - 1, b, 0) & B_WALL))) e->xs = 0;
			if ((e->ys < 0 && (blk_top(x, y, y + 1, b, 0) & B_WALL)) || (e->ys > 0 && (blk_below(x, y, y - 1, b, 0) & B_WALL))) e->ys = 0;
		}
		if (player_distance(e) < 350 && --e->t1 <= 0)
		{
			e->t1 = (int16_t)(60 + rnd(40));
			enemy_shot(SH_BULLET, x + 58, y + 68, FX(40), FX(9), DMG(6));
			enemy_shot(SH_BULLET, x - 10, y + 68, FX(-40), FX(9), DMG(6));
		}
		break;

	case E_PFLANZE:
		/* the plant spits fireballs at the player */
		e->dir = pcx() > ecx(e);
		if (e->state == 0)
		{
			e->anim = 0;
			if (player_distance(e) < 450 && --e->t1 <= 0) { e->state = 1; e->anim = 0; e->animt = 0; }
		}
		else
		{
			if (++e->animt >= 6)
			{
				e->animt = 0;
				e->anim++;
				if (e->anim == 6)
				{
					fx xs, ys;
					const int sx = e->dir ? x + 84 : x + 12;
					aim_at(sx + 8, y + 24, 30, &xs, &ys);
					enemy_shot(SH_PFLANZE, sx, y + 16, xs, ys, DMG(10));
				}
				if (e->anim > 9) { e->state = 0; e->anim = 0; e->t1 = (int16_t)(60 + rnd(60)); }
			}
		}
		break;

	case E_SPITTERBOMBE:
		/* falls; on the ground it bursts and lets out spitters */
		anim_loop(e, 0, 5, 3);
		if (e->ys == 0 && e->state == 0) e->ys = FX(15);
		if (bu & (B_WALL | B_PLATFORM)) { e->energy = 0; }
		break;

	case E_NEST:
		/* the wasps' nest sends out steel flies while the player is near */
		if (player_distance(e) < 400 && --e->t1 <= 0)
		{
			e->t1 = (int16_t)(150 + rnd(100));
			Ent *m = ent_new(E_MUECKE, x + 10, y + 40);
			if (m) { m->active = 1; m->t1 = 0; }
		}
		break;

	case E_BRUECKE:
		ent_platform(e, idx);
		return;

	case E_PLATTFORM:
		/* moves (value1, value2) and turns at the map's turning points */
		{
			const int tl = blk((x - 1) / 20, (y + 16) / 20), tr = blk((x + b->r + 1) / 20, (y + 16) / 20);
			const int tt = blk((x + 64) / 20, (y - 1) / 20), tb = blk((x + 64) / 20, (y + b->b + 1) / 20);
			if (tl & B_TURN) e->xs = FX(e->v1);
			if (tr & B_TURN) e->xs = FX(-e->v1);
			if (tt & B_TURN) e->ys = FX(e->v2);
			if (tb & B_TURN) e->ys = FX(-e->v2);
		}
		ent_platform(e, idx);
		break;

	case E_ENDLEVEL:
		if (touching && !stage_clear_t) { stage_clear_t = 1; }
		return;

	case E_SECRET:
		if (touching) { pl.score += 1000; secret_t = 120; sfx(SFX_SECRET); e->type = E_NONE; }
		return;

#define K2(name, id, ...) case E_##name: if (name##_step(e, idx, x, y, bl, br, bo, bu, touching)) return; break;
#include K2_LIST
#undef K2
	}

	/* contact damage */
	if (touching && k->touch) player_damage((fx)((int32_t)k->touch * skill_mul[skill] / 600));   /* Timer.sync(touch / 100) x the skill's factor */

	/* the base movement (GegnerClass::Run) */
	e->xo = e->x; e->yo = e->y;
	e->xs += SYNC(e->xa);
	e->ys += SYNC(e->ya);
	if ((e->xs < 0 && !(bl & B_WALL)) || (e->xs > 0 && !(br & B_WALL))) e->x += SYNC(e->xs);
	if ((e->ys < 0 && !(bo & B_WALL)) || (e->ys > 0 && !(bu & B_WALL))) e->y += SYNC(e->ys);
	if (e->type == E_PLATTFORM && pl.platform == idx + 1)
	{
		pl.x += e->x - e->xo;
		pl.y += e->y - e->yo;
	}
	/* out of the level: gone */
	if ((e->x >> 8) + b->r < 0 || (e->y >> 8) + b->b < 0 || (e->x >> 8) > map_w * 20 || (e->y >> 8) > map_h * 20)
		e->type = E_NONE;
	if (e->hit) e->hit--;
}

/* ------------------------------------------------------------------ the player's step */
static int pad, pad_prev;

/* ------------------------------------------------------------------ the demo
 * The demo robot (PC checks: HB_DEMO=1 with HB_STAGE; the attract mode's fallback when no
 * recorded demos are built in): plays the stage by itself (no damage) until a button is
 * pressed, the stage ends or 2 minutes have passed. The demo player: walks on, keeps the
 * fire button held, aims up at an enemy above him, jumps at walls, gaps and what blocks him,
 * and turns back for a while when he gets nowhere. */
static int demo_dir = 1, demo_jump, demo_rest, demo_stuck, demo_back, demo_t;
static fx demo_lastx;
static void demo_reset(void) { demo_dir = 1; demo_jump = demo_rest = demo_stuck = demo_back = demo_t = 0; demo_lastx = pl.x; }
/* aim up at an enemy above (the demo player, and the recorder's player in game/demorec.c) */
static void demo_aim(int *p)
{
	for (int k = 0; k < ntargets; k++)
	{
		const Ent *e = &ents[targets[k]];
		if (e->type == E_NONE) continue;
		const int dx = ecx(e) - pcx(), dy = ecy(e) - pcy();
		if (dy < -60 && dx > -200 && dx < 200)
		{
			*p |= PAD_UP;
			if (dx > -40 && dx < 40) *p &= ~(PAD_LEFT | PAD_RIGHT);
			break;
		}
	}
}
static int demo_pad(void)
{
	const int d = demo_back > 0 ? -1 : 1;
	if (demo_back > 0) demo_back--;
	int p = PAD_B | (d > 0 ? PAD_RIGHT : PAD_LEFT);
	const int px = pl.x >> 8, py = pl.y >> 8;
	const int front = d > 0 ? px + player_box.r + 14 : px + player_box.l - 14;
	const int ground = pl.state != PS_JUMP;
	/* a wall ahead (chest or knee high) */
	const int wall = (blk(front / 20, (py + 40) / 20) | blk(front / 20, (py + 70) / 20)) & B_WALL;
	/* a gap ahead: nothing to stand on for 3 tiles down */
	int gap = 1;
	for (int k = 0; k < 3 && gap; k++)
		if (blk((px + 35 + d * 40) / 20, (py + player_box.b + 4) / 20 + k) & (B_WALL | B_PLATFORM)) gap = 0;
	/* getting nowhere: jump, then turn back for a while */
	if (++demo_t % 60 == 0)
	{
		const int moved = (pl.x > demo_lastx ? pl.x - demo_lastx : demo_lastx - pl.x) >> 8;
		demo_stuck = moved < 20 ? demo_stuck + 1 : 0;
		demo_lastx = pl.x;
		if (demo_stuck >= 3) { demo_back = 120 + rnd(120); demo_stuck = 0; }
	}
	if (demo_jump > 0) { p |= PAD_C; demo_jump--; if (!demo_jump) demo_rest = 4; }
	else if (demo_rest > 0) demo_rest--;
	else if (ground && (wall || gap || demo_stuck || rnd(240) == 0)) demo_jump = 12 + rnd(24);
	demo_aim(&p);
	return p;
}

/* ------------------------------------------------------------------ the recorded demos
 * The attract mode (a minute in the main menu without a button) plays one of the recorded
 * demos, picked at random (game/demo_data.h, made by game/demorec.c along the routes of
 * recordings of the original game - stages 1 and 2, three each, about a minute): the buttons of every 60 Hz step, run-length coded. A
 * recorded demo runs the world step by step the same way on the PC and the 32X (the camera and
 * the entities' wake-up every step, the random numbers from the demo's seed, the drawing leaves
 * them alone) - the frame rate does not change what happens. Every second the player's place
 * is checked against the recording: should it ever differ, that demo ends (the next one plays)
 * rather than showing a player who got lost. */
#ifndef OJ32X_MARS
#include <stdio.h>
#endif
typedef struct
{
	int16_t stage;
	int16_t x, y;                     /* the start (0, 0: the stage's start) */
	uint32_t seed;
	uint32_t steps;
	const uint16_t *rle;              /* pad, count, pad, count, ... */
	uint32_t nrle;                    /* uint16 words */
	const int16_t *chk;               /* x, y (pixels) after every 60 steps */
	uint32_t nchk;                    /* places */
} DemoRec;
#include "demo_data.h"
#ifndef DEMO_RECS
#define DEMO_RECS 0
static const DemoRec demo_recs[1];
#endif
static int demo_rec = -1;             /* the recorded demo playing (-1: none) */
static int pl_off_t;                  /* steps the player has been left off a held screen */
static uint32_t demo_rec_pos, demo_rec_left, demo_rec_t;
static int demo_rec_pad_now;
static int demo_warp, demo_warp_x, demo_warp_y;
static int demo_rec_pad(void)
{
	const DemoRec *d = &demo_recs[demo_rec];
	while (demo_rec_left == 0 && demo_rec_pos + 1 < d->nrle)
	{
		demo_rec_pad_now = d->rle[demo_rec_pos];
		demo_rec_left = d->rle[demo_rec_pos + 1];
		demo_rec_pos += 2;
	}
	if (demo_rec_left == 0) return 0;
	demo_rec_left--;
	return demo_rec_pad_now;
}
/* after a step: 1 when the recorded demo is over or has gone off its path */
static int demo_rec_check(void)
{
	const DemoRec *d = &demo_recs[demo_rec];
	demo_rec_t++;
	if (demo_rec_t >= d->steps) return 1;
	if (demo_rec_t % 60 == 0)
	{
		const uint32_t k = demo_rec_t / 60 - 1;
		if (k < d->nchk)
		{
#ifndef OJ32X_MARS
			if (getenv("HB_DEMOTRACE")) fprintf(stderr, "DEMO %d step %u: %d,%d recorded %d,%d\n", demo_rec, demo_rec_t,
			                                    (int)(pl.x >> 8), (int)(pl.y >> 8), d->chk[2 * k], d->chk[2 * k + 1]);
#endif
			const int dx = (pl.x >> 8) - d->chk[2 * k], dy = (pl.y >> 8) - d->chk[2 * k + 1];
			if (dx > 64 || dx < -64 || dy > 64 || dy < -64)
			{
#ifndef OJ32X_MARS
				fprintf(stderr, "DEMO %d off its path at step %u: %d,%d (recorded %d,%d)\n", demo_rec, demo_rec_t,
				        (int)(pl.x >> 8), (int)(pl.y >> 8), d->chk[2 * k], d->chk[2 * k + 1]);
#endif
				return 1;
			}
		}
	}
	return 0;
}


#ifndef OJ32X_MARS
#include <stdio.h>
static int host_god = -1;            /* PC check: HB_GOD=1, the player takes no damage */
#endif
static void player_damage(fx amount)
{
#ifndef OJ32X_MARS
	if (host_god < 0) host_god = getenv("HB_GOD") != 0;
	if (host_god && amount < FX(150)) return;
#endif
#ifndef OJ32X_MARS
	if (getenv("HB_DMG") && amount > 0 && pl.state != PS_DEAD && !(pl.safe_t > 0 && amount < FX(150)))
		fprintf(stderr, "DMG %d from %s %d\n", (int)(amount * 100 >> 8), dmg_src_shot ? "shot" : "ent", dmg_src);
#endif
#ifndef OJ32X_MARS
	if (demo && amount < FX(150) && !getenv("HB_DEMO_HURT")) return;   /* PC check: HB_DEMO_HURT=1 - the demo player is hurt */
#else
	if (demo && amount < FX(150)) return;   /* the demo player takes no damage (a fall or a crush still costs a life) */
#endif
	if (pl.wheel && amount < FX(150)) return;    /* the wheel is invulnerable (Armour) */
	if (pl.state == PS_DEAD || amount <= 0 || (pl.safe_t > 0 && amount < FX(150))) return;
	if (pl.hurt <= 0) sfx(SFX_HURT);
	pl.energy -= amount;
	pl.hurt = 5;
	if (pl.energy <= 0)
	{
		pl.energy = 0;
		pl.state = PS_DEAD;
		pl.dead_t = 0;
		pl.platform = 0;
		fx_add(&P_explbig, pcx(), pcy(), 2);
		sfx(SFX_BIGEXPLODE);
	}
}

static void player_spawn(fx x, fx y)
{
	pl.x = pl.xo = x;
	pl.y = pl.yo = y;
	pl.xs = pl.ys = 0;
	pl.jumpadd = FX(12.5);
	pl.state = PS_JUMP;
	pl.anim = 0;
	pl.energy = FX(100);
	pl.platform = 0;
	pl.hurt = 0;
	pl.jump_ok = 0;
	pl.ground_y = y;
	/* "all weapons, full, every life" (the user's request): the three weapons at level 8, the
	 * lightning at its full length, Hurrican's new-game stock of grenades / power lines / bombs */
	pl.sel = 0;
	pl.wlevel[0] = pl.wlevel[1] = pl.wlevel[2] = 8;
	pl.blitzlen = 9;                 /* CurrentWeaponLevel[3] 8 -> BlitzLength 9 (HUD.cpp) */
	pl.grenades = 3;
	pl.powerlines = 3;
	pl.smartbombs = 1;
	pl.blitz = 0;
	pl.blitz_t = 0;
	pl.wheel = 0;
	pl.wheel_t = 360;
	pl.wpn_ok = pl.grn_ok = pl.spc_ok = pl.bomb_ok = 1;
}

/* a box from a picture's full-size frame, the middle half (as enemy_shot_pic) */
static Box pic_box(const Pic *p)
{
	const int w = 2 * p->fw, h = 2 * p->fh;
	return (Box){ (int16_t)(w / 4), (int16_t)(h / 4), (int16_t)(w - w / 4), (int16_t)(h - h / 4) };
}

/* fire the selected weapon (Player.cpp PlayerShoot), lean: the level-based shapes of Hurrican
 * drawn with one sprite per shot (no RenderSpriteRotated - the laser/bounce packs hold no turned
 * frames, so they keep their upright picture, mirrored by travel) */
static void player_shoot(void)
{
	/* the gun's muzzle (CalcAustrittsPunkt) and the aim angle: 0 right, 180 left, -90 up */
	int mx, my, ang;
	const int right = pl.dir > 0, run = pl.state == PS_RUN, jump = pl.state == PS_JUMP;
	const int lvl = pl.wlevel[pl.sel];
	if (pl.aim == 2)                          /* up */
	{
		mx = right ? (jump ? 23 : 21) : 33;
		my = jump ? -4 : -3;
		ang = -90;
	}
	else if (pl.aim == 1)                     /* diagonally up */
	{
		if (jump) { mx = right ? 45 : 12; my = 6; }
		else if (run) { mx = right ? 45 : -1; my = right ? 6 : 8; }
		else { mx = right ? 41 : 12; my = 10; }
		ang = -45;
	}
	else                                      /* straight */
	{
		if (pl.state == PS_DUCK) { mx = right ? 52 : 3; my = 46; }
		else if (jump) { mx = right ? 52 : -1; my = 18; }
		else if (run) { mx = right ? 60 : -7; my = 22; }
		else { mx = right ? 52 : 3; my = 23; }
		ang = 0;
	}
	if (!right) ang = 180 - ang;

	if (pl.sel == 0)                          /* ----- spread shot (SPREADSHOT) */
	{
		if (lvl >= 5) { mx -= 2; my -= 2; }
		const int n = lvl >= 3 ? 5 : lvl == 2 ? 3 : 1;   /* 1/3/5 shots 8 deg apart (P1m: 5 at the top, not 7 - for the frame rate) */
		for (int i = 0; i < n; i++)
		{
			const int a = ang + (i - n / 2) * 8;
			Shot *s = shot_new();
			if (!s) break;
			s->kind = SH_SPREAD;
			s->x = pl.x + FX(mx);
			s->y = pl.y + FX(my);
			s->xs = cos_d(a) * 50;
			s->ys = sin_d(a) * 50;
			s->dmg = (int16_t)(6 - lvl * 3 / 4 + (lvl >= 5 ? 3 : 0));
			if (s->dmg < 2) s->dmg = 2;
			s->life = 60 * 2;
			s->anim = (uint8_t)(lvl >= 5);
			s->rot = (uint8_t)((((a + 90) % 360 + 360) % 360 * 16 + 180) / 360 % 16);
		}
	}
	else if (pl.sel == 1)                     /* ----- laser (LASERSHOT, speed 60) */
	{
		const int big = lvl >= 5;             /* LASERSHOTBIG at level 5+ (bigger beam) */
		const Pic *p = big ? &P_plaserbig : &P_plaser;
		const int dmg = big ? (lvl >= 7 ? 16 : 12) : (lvl >= 2 ? 10 : 8);
		const int n = lvl >= 3 ? 2 : 1;   /* more parallel beams by level (P1m: 2 at the top, not 3 - for the frame rate) */
		const int perpx = -sin_d(ang), perpy = cos_d(ang);   /* unit normal (8.8) */
		/* the middle beam's centre on the barrel's line: where the (big) spread shot's middle is
		 * (the spread shot's place is its picture's corner, aligned with the barrel in P1f) */
		const int cx = mx - 2 + P_spreadbig.fw, cy = my - 2 + P_spreadbig.fh;
		for (int i = 0; i < n; i++)
		{
			const int o2 = 2 * i - (n - 1);   /* -1 / +1 for two beams: either side of the barrel's line */
			Shot *s = shot_new();
			if (!s) break;
			s->kind = SH_PLASER;
			s->pic = p;
			s->x = pl.x + FX(cx) + perpx * 6 * o2 - FX(p->fw);
			s->y = pl.y + FX(cy) + perpy * 6 * o2 - FX(p->fh);
			s->xs = cos_d(ang) * 60;
			s->ys = sin_d(ang) * 60;
			s->dmg = (int16_t)dmg;
			s->life = 60 * 2;
			s->box = pic_box(p);
			/* the bolt points up; the pack holds it turned in 16 steps (tools: ROT16, a square) */
			s->rot = (uint8_t)((((ang + 90) % 360 + 360) % 360 * 16 + 180) / 360 % 16);
		}
	}
	else                                      /* ----- bounce (BOUNCESHOT1) */
	{
		Shot *s = shot_new();
		if (s)
		{
			s->kind = SH_BOUNCE;
			s->gen = 0;
			s->pic = &P_bounce[0];
			s->x = pl.x + FX(mx) - FX(P_bounce[0].fw);
			s->y = pl.y + FX(my) - FX(P_bounce[0].fh);
			s->xs = cos_d(ang) * 65;
			s->ys = sin_d(ang) * 65;
			if (s->xs == 0 && s->ys == 0) s->ys = FX(-65);
			s->dmg = 8;
			s->life = 60 * 3;
			s->bounce_walls = (uint8_t)(lvl == 2);   /* level 8: splits instead */
			s->box = pic_box(&P_bounce[0]);
		}
	}
	sfx(SFX_SHOT);
}

/* a bounce fragment (ExplodeShot splits BOUNCESHOT1->2->3); a = whole-degree direction */
static void bounce_split(int gen, fx x, fx y, int a, int lvl)
{
	static const int16_t gdmg[3] = { 8, 8, 6 };
	Shot *s = shot_new();
	if (!s) return;
	s->kind = SH_BOUNCE;
	s->gen = (uint8_t)gen;
	s->pic = &P_bounce[gen];
	s->x = x; s->y = y;
	s->xs = cos_d(a) * 65;
	s->ys = sin_d(a) * 65;
	if (s->xs == 0 && s->ys == 0) s->ys = FX(-65);
	/* medium damage grows with level, small drops by 3 each bounce (Projectiles.cpp) */
	s->dmg = gen == 1 ? (int16_t)(lvl * 2 - 3 > 8 ? 8 : lvl * 2 - 3) : gdmg[gen];
	if (s->dmg < 2) s->dmg = 2;
	s->life = 60 * 3;
	/* medium bounces off walls below level 5, small always (level 8: medium splits, small bounces) */
	s->bounce_walls = (uint8_t)(gen == 2 || (gen == 1 && lvl < 5));
	s->box = pic_box(&P_bounce[gen]);
}

static void fire_grenade(void)
{
	int mx, my, ang;
	const int right = pl.dir > 0;
	ang = pl.aim == 2 ? -90 : pl.aim == 1 ? -45 : 0;
	if (!right) ang = 180 - ang;
	mx = right ? 40 : 18; my = 30;
	Shot *s = shot_new();
	if (!s) return;
	s->kind = SH_GRENADE;
	s->pic = &P_grenade;
	s->x = pl.x + FX(mx) - FX(P_grenade.fw);
	s->y = pl.y + FX(my) - FX(P_grenade.fh);
	s->xs = cos_d(ang) * 60;
	s->ys = sin_d(ang) * 60 - FX(6);      /* thrown up a little */
	s->ya = FX(5);                        /* gravity (yAcc) */
	s->dmg = 300;
	s->life = 60 * 4;
	s->box = pic_box(&P_grenade);
	pl.grenades--;
	sfx(SFX_SHOT);
}

/* the two power lines sweeping left and right along the floor (POWERLINE / POWERLINE2): one tall
 * shot each way, its box the view's height, drawn as a stack of the power-line sprite */
static void fire_powerline(void)
{
	const int top = cam_y * 2, h = VIEW_H * 2;
	for (int d = 0; d < 2; d++)
	{
		Shot *s = shot_new();
		if (!s) break;
		s->kind = SH_POWERLINE;
		s->pic = &P_powerline;
		s->x = pl.x + FX(20);
		s->y = FX(top);
		s->xs = d ? FX(80) : FX(-80);
		s->ys = 0;
		s->dmg = (int16_t)(d ? 12 : 20);
		s->anim = (uint8_t)d;
		s->life = 60 * 2;
		s->box = (Box){ 0, 0, 40, (int16_t)h };
	}
	pl.powerlines--;
	sfx(SFX_EXPLODE);
}

/* the smart bomb: damages everything on the screen, a big flash (SMARTBOMB) */
static void fire_smartbomb(void)
{
	for (int k = 0; k < ntargets; k++)
	{
		Ent *e = &ents[targets[k]];
		if (e->type == E_NONE) continue;
		if (e->type == E_POWERBLOCK && !e->state) continue;
		e->energy -= 400;
		e->hit = 4;
	}
	fx_add(&P_explbig, cam_x * 2 + 320, cam_y * 2 + 200, 2);
	pl.smartbombs--;
	sfx(SFX_BIGEXPLODE);
}

/* the wheel lays a bomb (BOMBE): drops and explodes */
static void lay_bomb(void)
{
	Shot *s = shot_new();
	if (!s) return;
	s->kind = SH_GRENADE;
	s->pic = &P_grenade;
	s->x = pl.x + FX(30) - FX(P_grenade.fw);
	s->y = pl.y + FX(56) - FX(P_grenade.fh);
	s->xs = 0; s->ys = 0; s->ya = FX(5);
	s->dmg = 300;
	s->life = 120;
	s->box = pic_box(&P_grenade);
	sfx(SFX_SHOT);
}

/* the loaded beam (BEAMLADEN -> BLITZBEAM): a fast piercing bolt, its damage by the charge */
static void fire_beam(int charge)
{
	int ang = pl.blitz_ang;
	Shot *s = shot_new();
	if (!s) return;
	s->kind = SH_BEAM;
	s->pic = &P_blitzbeam;
	s->x = pl.x + FX(35) - FX(P_blitzbeam.fw) + cos_d(ang) * 40;
	s->y = pl.y + FX(35) - FX(P_blitzbeam.fh) + sin_d(ang) * 40;
	s->xs = cos_d(ang) * 90;
	s->ys = sin_d(ang) * 90;
	s->dmg = (int16_t)(charge);         /* 20..160 */
	s->life = 20;
	s->box = pic_box(&P_blitzbeam);
	sfx(SFX_BIGEXPLODE);
}

/* the lightning (BLITZEN): a beam from the gun, turned by Left/Right, segments of 32 px until a
 * wall, hurting the enemies it touches. Computed and drawn from pl.blitz_ang / pl.blitz_reach. */
#define BLITZ_RAMP 24                     /* steps before the beam appears (PLAYER_BLITZ_START) */
static void lightning_damage(void)
{
	const int ux = cos_d(pl.blitz_ang), uy = sin_d(pl.blitz_ang);   /* 8.8 unit */
	int sx = (pl.x >> 8) + 20 + 16 * ux / 256;      /* a little out of the gun */
	int sy = (pl.y >> 8) + 21 + 16 * uy / 256;
	pl.blitz_reach = 0;
	if (pl.blitz_t < BLITZ_RAMP) return;            /* still charging: only the flash */
	const int d = skill_mul[skill] * 4 / 256;       /* BLITZ_POWER 4, per step */
	const Box seg = { 0, 0, 32, 32 };
	for (int i = 0; i < pl.blitzlen + 1; i++)
	{
		const int cx = sx + 16, cy = sy + 16;
		if (cx >= 0 && cy >= 0 && (blk(cx / 20, cy / 20) & B_WALL)) break;
		for (int k = 0; k < ntargets; k++)
		{
			Ent *e = &ents[targets[k]];
			if (e->type == E_NONE) continue;
			if (e->type == E_DECKENTURM && e->state == 0) continue;
			if (!boxes_meet(sx, sy, &seg, e->x >> 8, e->y >> 8, ent_box(e))) continue;
			if (e->type == E_POWERBLOCK && !e->state) { e->state = 1; e->energy = 12; }
			else e->energy -= (int16_t)(d > 0 ? d : 1);
			e->hit = 4;
		}
		pl.blitz_reach = i + 1;
		sx += 32 * ux / 256;
		sy += 32 * uy / 256;
	}
	if ((pl.blitz_anim & 7) == 0) sfx(SFX_HIT);
}

static void player_step(void)
{
	const int held_l = pad & PAD_LEFT, held_r = pad & PAD_RIGHT, held_u = pad & PAD_UP, held_d = pad & PAD_DOWN;
	int jump = pad & PAD_C, fire = pad & PAD_B;
	const int b_a = pad & PAD_A, b_x = pad & PAD_X, b_y = pad & PAD_Y, b_z = pad & PAD_Z;

	if (pl.state == PS_DEAD)
	{
		pl.dead_t++;
		return;
	}
	pl.xo = pl.x; pl.yo = pl.y;
	const int px = pl.x >> 8;
	int py = pl.y >> 8;

	pl.in_liquid = (blk((px + 35) / 20, (py + 45) / 20) & B_LIQUID) != 0;

	const Box *box = &player_box;

	/* ---- the weapon cycle (X) ---- */
	if (!b_x) pl.wpn_ok = 1;
	if (b_x && pl.wpn_ok && !pl.wheel && pl.blitz == 0) { pl.wpn_ok = 0; pl.sel = (pl.sel + 1) % 3; }

	/* enter the wheel: DOWN + Z on the ground while there is wheel energy (Armour) */
	/* the wheel: DOWN + Z, or DOWN + A for a 3-button pad (no Z there) */
	if (!pl.wheel && pl.on_ground && held_d && (b_z || (b_a && pl.blitz == 0)) && pl.wheel_t > 0) { pl.wheel = 1; pl.spc_ok = 0; pl.bomb_ok = 0; }

	if (pl.wheel)
	{
		/* roll along the ground (RADELN); invulnerable while the energy lasts */
		if (held_l) pl.dir = -1;
		if (held_r) pl.dir = 1;
		pl.xs = (fx)(pl.dir * (pl.in_liquid ? FX(30) : FX(40)));
		pl.aim = 0;
		if (pl.state == PS_DUCK) pl.state = pl.xs ? PS_RUN : PS_STAND;
		/* Z: power line, UP + Z: smart bomb */
		if (!b_z) pl.spc_ok = 1;
		if (b_z && pl.spc_ok)
		{
			if (held_u) { if (pl.smartbombs > 0) { pl.spc_ok = 0; fire_smartbomb(); } }
			else if (pl.powerlines > 0) { pl.spc_ok = 0; fire_powerline(); }
		}
		/* B: lay a bomb */
		if (!fire) pl.bomb_ok = 1;
		if (fire && pl.bomb_ok) { pl.bomb_ok = 0; lay_bomb(); }
		/* drain the energy; C (jump) or an empty bar turns him back */
		pl.wheel_t--;
		if (pl.wheel_t <= 0 || jump) { pl.wheel = 0; pl.state = pl.on_ground ? PS_STAND : PS_JUMP; }
		fire = 0; jump = 0;
	}
	else
	{
		if (pl.wheel_t < 360) pl.wheel_t++;      /* the energy comes back slowly */

		/* ---- the lightning (A on the ground); A + B loads the beam ---- */
		if (b_a && !held_d && pl.on_ground && pl.state != PS_JUMP)
		{
			if (fire && pl.blitz != 2)
			{
				pl.blitz = 2; pl.blitz_t = 5;
				pl.blitz_ang = pl.dir > 0 ? 0 : 180;
				if (held_u) { pl.dir = -pl.dir; pl.blitz_ang = 270; }
			}
			else if (pl.blitz == 0 && !fire)
			{
				pl.blitz = 1; pl.blitz_t = 0; pl.blitz_anim = 0;
				pl.blitz_ang = pl.dir > 0 ? 0 : 180;
			}
		}
		if (!b_a && pl.blitz == 1) pl.blitz = 0;
		if (pl.blitz == 2 && (!b_a || !fire))
		{
			if (pl.blitz_t >= 20) fire_beam(pl.blitz_t > 160 ? 160 : pl.blitz_t);
			pl.blitz = 0;
		}
		if (pl.blitz && (!pl.on_ground || pl.state == PS_JUMP)) pl.blitz = 0;

		if (pl.blitz)
		{
			pl.xs = 0; pl.state = PS_STAND; pl.aim = 0; fire = 0;
			if (pl.blitz == 1 && pl.blitz_t < BLITZ_RAMP) pl.blitz_t++;
			if (pl.blitz == 2 && pl.blitz_t < 160) pl.blitz_t += pl.blitzlen / 2 + 1;
			if (pl.blitz_t >= BLITZ_RAMP || pl.blitz == 2)
			{
				if (held_r && !held_l) pl.blitz_ang += 3;
				if (held_l && !held_r) pl.blitz_ang -= 3;
			}
			while (pl.blitz_ang < 0) pl.blitz_ang += 360;
			while (pl.blitz_ang >= 360) pl.blitz_ang -= 360;
			if (++pl.blitz_anim >= 400) pl.blitz_anim = 0;
			if (pl.blitz == 1) lightning_damage();
		}
		else
		{
			/* grenade (Y) */
			if (!b_y) pl.grn_ok = 1;
			if (b_y && pl.grn_ok && pl.grenades > 0) { pl.grn_ok = 0; fire_grenade(); }
			/* power line (Z), smart bomb (UP + Z) */
			if (!b_z) pl.spc_ok = 1;
			if (b_z && pl.spc_ok && !held_d)
			{
				if (held_u) { if (pl.smartbombs > 0) { pl.spc_ok = 0; fire_smartbomb(); } }
				else if (pl.powerlines > 0) { pl.spc_ok = 0; fire_powerline(); }
			}
		}
	}

	/* walking / ducking / jumping (not while rolling or lightning-rooted) */
	if (!pl.wheel && pl.blitz == 0)
	{
	pl.xs = 0;
	if (pl.state != PS_DUCK)
	{
		if (held_l && !held_r) { pl.dir = -1; pl.xs = pl.in_liquid ? FX(-40) / 3 : FX(-20); }
		if (held_r && !held_l) { pl.dir = 1; pl.xs = pl.in_liquid ? FX(40) / 3 : FX(20); }
	}
	/* aiming up: standing still, walking stops */
	pl.aim = held_u ? ((held_l || held_r) ? 1 : 2) : 0;
	if (pl.aim == 2 && pl.state != PS_JUMP) pl.xs = 0;

	/* ducking */
	if (pl.state != PS_JUMP && held_d && !held_u) { pl.state = PS_DUCK; pl.xs = 0; pl.aim = 0; }
	else if (pl.state == PS_DUCK && !held_d) pl.state = PS_STAND;
	if (pl.state == PS_DUCK)
	{
		if (held_l && !held_r) pl.dir = -1;
		if (held_r && !held_l) pl.dir = 1;
	}

	if (pl.state != PS_JUMP && pl.state != PS_DUCK)
		pl.state = pl.xs ? PS_RUN : PS_STAND;

	/* jumping */
	if (!jump) pl.jump_ok = 1;
	if (jump && pl.jump_ok && pl.state != PS_JUMP)
	{
		pl.jump_ok = 0;
		pl.state = PS_JUMP;
		pl.anim = 2;
		pl.jumpstart = pl.y;
		pl.ys = FX(-43);
		pl.jumpadd = 0;
		pl.platform = 0;
	}
	}
	if (pl.state == PS_JUMP)
	{
		pl.ys += SYNC(pl.in_liquid ? pl.jumpadd * 2 / 3 : pl.jumpadd);
		pl.y += SYNC(pl.in_liquid ? pl.ys * 2 / 3 : pl.ys);
		if (pl.ys > FX(43)) pl.ys = FX(43);
		/* the top of the jump reached, or the button let go: falling, the faster the lower
		 * the jump was (Player.cpp) */
		if (pl.y < pl.jumpstart - FX(124) || !jump)
		{
			int add = 125 + abs((160 - (int)((pl.ground_y - pl.y) >> 8)) / 10) * 10;   /* tenths */
			if (add > 180) add = 180;
			pl.jumpadd = FX(add) / 10;
		}
	}

	/* standing on an entity (bridge, platform, power block) */
	if (pl.platform)
	{
		const Ent *e = &ents[pl.platform - 1];
		const Box *eb = ent_box(e);
		const int ex = e->x >> 8;
		if (e->type == E_NONE || (pl.x >> 8) + box->r < ex + eb->l || (pl.x >> 8) + box->l > ex + eb->r || pl.ys < 0)
		{
			/* off this one: the next piece of a bridge at the same height takes him over */
			const int foot = (pl.y >> 8) + box->b + 1, px = pl.x >> 8;
			pl.platform = 0;
			if (pl.ys >= 0)
				for (int k = 0; k < nactive; k++)
				{
					const Ent *o = &ents[active_list[k]];
					if (o->type != E_BRUECKE && o->type != E_PLATTFORM && !(o->type == E_POWERBLOCK && o->state)) continue;
					const Box *ob = ent_box(o);
					const int top = (o->y >> 8) + ob->t, ox = o->x >> 8;
					if (top >= foot - 4 && top <= foot + 4 && px + box->r >= ox + ob->l && px + box->l <= ox + ob->r)
					{
						pl.platform = active_list[k] + 1;
						pl.y = o->y + FX(ob->t - box->b - 1);
						break;
					}
				}
		}
		else
		{
			pl.y = e->y + FX(eb->t - box->b - 1);
			if (pl.state == PS_JUMP) { pl.state = pl.xs ? PS_RUN : PS_STAND; pl.ys = 0; pl.jumpadd = 0; sfx(SFX_LAND); }
		}
	}

	/* horizontal movement and walls */
	pl.x += SYNC(pl.xs);
	{
		int x = pl.x >> 8, nx;
		py = pl.y >> 8;
		const int xo = pl.xo >> 8;
		if (pl.xs > 0 && (blk_right(x, py, xo, box, &nx) & B_WALL)) { pl.x = FX(nx); if (pl.wheel) pl.dir = -1; }
		if (pl.xs < 0 && (blk_left(x, py, xo, box, &nx) & B_WALL)) { pl.x = FX(nx); if (pl.wheel) pl.dir = 1; }
		if ((pl.x >> 8) + box->l < 0) { pl.x = FX(-box->l); if (pl.wheel) pl.dir = 1; }
		if ((pl.x >> 8) + box->r > map_w * 20) { pl.x = FX(map_w * 20 - box->r); if (pl.wheel) pl.dir = -1; }
	}

	/* ground: slopes first, then walls and platforms (Player.cpp, Tileengine.cpp) */
	if (!pl.platform)
	{
		int x = pl.x >> 8, y = pl.y >> 8;
		const int yo = pl.yo >> 8;
		int bu = 0;
		if (pl.ys >= 0) bu = blk_slopes(x, &y, box, pl.ys);
		if (bu & (B_SLOPE_L | B_SLOPE_R))
		{
			pl.y = FX(y);
			if (pl.state == PS_JUMP) { pl.state = pl.xs ? PS_RUN : PS_STAND; sfx(SFX_LAND); }
			pl.ys = 0; pl.jumpadd = 0;
			pl.on_ground = 1;
		}
		else
		{
			int ny;
			bu = (pl.ys >= 0) ? blk_below(x, y, yo, box, &ny) : 0;
			if (bu & (B_WALL | B_PLATFORM))
			{
				if (pl.ys >= 0)
				{
					pl.y = FX(ny);
					if (pl.state == PS_JUMP) { pl.state = pl.xs ? PS_RUN : PS_STAND; pl.anim = 0; sfx(SFX_LAND); }
					pl.ys = 0; pl.jumpadd = 0;
				}
				pl.on_ground = 1;
			}
			else
			{
				pl.on_ground = 0;
				if (pl.state != PS_JUMP)
				{
					/* nothing under the feet: falling */
					pl.state = PS_JUMP;
					pl.anim = 0;
					pl.ys = 0;
					pl.jumpstart = pl.y;
					pl.jumpadd = FX(12.5);
				}
			}
		}
		/* the head against the ceiling */
		if (pl.ys < 0)
		{
			int nyy;
			if (blk_top(pl.x >> 8, pl.y >> 8, pl.yo >> 8, box, &nyy) & B_WALL)
			{
				pl.y = FX(nyy);
				pl.ys = 0;
				pl.jumpadd = FX(12.5);
			}
		}
	}
	else pl.on_ground = 1;

	if (pl.on_ground && !pl.platform) pl.ground_y = pl.y;
	/* a safe place to come back to */
	if (pl.on_ground && !pl.platform && !(blk_in(pl.x >> 8, pl.y >> 8, box, B_DAMAGE | B_LIQUID)))
	{
		pl.safe_x = pl.x;
		pl.safe_y = pl.y;
	}

	/* damage tiles */
	{
		static const Box feet = { 20, 12, 50, 82 };      /* the player and the row under his feet */
		if (blk_in(pl.x >> 8, pl.y >> 8, &feet, B_DAMAGE)) player_damage(DMG_T(1 + skill));   /* Hurrican: 10 / 20 / 30 by skill, then x the skill's factor */
	}
	/* fallen out of the level */
	if ((pl.y >> 8) > map_h * 20) player_damage(FX(200));
	/* left behind by a held screen (the elevator: he stayed on a ledge while the lift went on):
	 * Hurrican's CheckLevelExit - he dies and comes back on the lift. Here after 1.5 s off the
	 * screen (a high jump at the top of the shaft leaves it for a moment and comes back) */
	{
		const int px = pl.x >> 8, py = pl.y >> 8, sx = cam_x * 2, sy = cam_y * 2;
		const int off = cam_lock_held && pl.state != PS_DEAD &&
			(px + player_box.r < sx || px + player_box.l > sx + 640 || py + player_box.b < sy || py + player_box.t > sy + 400);
		pl_off_t = off ? pl_off_t + 1 : 0;
		if (pl_off_t > 90) { pl_off_t = 0; player_damage(FX(1000)); }
	}

	/* shooting */
	if (pl.shotdelay > 0) pl.shotdelay--;
	if (fire && pl.shotdelay <= 0)
	{
		player_shoot();
		pl.shotdelay = 11;                 /* PLAYER_SHOTDELAY 1.8 units */
	}

	/* animation: PLAYER_ANIMSPEED 0.4 units = 2.4 steps */
	pl.animcount += 10;
	while (pl.animcount >= 24)
	{
		pl.animcount -= 24;
		if (pl.state == PS_RUN) { if (++pl.anim >= 20) pl.anim = 0; }
		else if (pl.state == PS_JUMP) { if (pl.ys > FX(-43) * 2 / 3 && pl.anim < 11) pl.anim++; }
		else if (pl.state == PS_STAND) { if (++pl.idle_t > 180) { if (++pl.anim >= 19) pl.anim = 0; } else pl.anim = 0; }
	}
	if (pl.state != PS_STAND) pl.idle_t = 0;
	if (pl.hurt > 0) pl.hurt--;
	if (pl.safe_t > 0) pl.safe_t--;
}


/* a grenade / bomb bursts: a big explosion, damaging the enemies near it */
static void grenade_explode(int cx, int cy, int dmg)
{
	fx_add(&P_explbig, cx, cy, 2);
	sfx(SFX_BIGEXPLODE);
	for (int k = 0; k < ntargets; k++)
	{
		Ent *e = &ents[targets[k]];
		if (e->type == E_NONE) continue;
		const int dx = ecx(e) - cx, dy = ecy(e) - cy;
		if (dx * dx + dy * dy > 70 * 70) continue;
		if (e->type == E_POWERBLOCK && !e->state) { e->state = 1; e->energy = 12; }
		else e->energy -= (int16_t)dmg;
		e->hit = 4;
	}
}

/* ------------------------------------------------------------------ shots' step */
static void shots_step(void)
{
	for (int i = 0; i < MAX_SHOTS; i++)
	{
		Shot *s = &shots[i];
		if (!s->on) continue;
		s->x += SYNC(s->xs);
		s->y += SYNC(s->ys);
		if (s->kind == SH_PFLANZE) s->ys += SYNC(FX(2));        /* the fireballs fall in an arc */
		else if (s->kind == SH_PIC) s->ys += s->ya;
		else if (s->kind == SH_GRENADE) s->ys += SYNC(s->ya);   /* the grenade's arc */
		if (--s->life <= 0) { s->on = 0; continue; }
		const Box *b = s->kind >= SH_PIC ? &s->box : &shot_box[s->kind];
		const int x = s->x >> 8, y = s->y >> 8;
		const int cx = x + (b->l + b->r) / 2, cy = y + (b->t + b->b) / 2;

		/* ---- enemy shots: against the player ---- */
		if (s->enemy)
		{
			if (cx >= 0 && cy >= 0 && (blk(cx / 20, cy / 20) & B_WALL)) { s->on = 0; continue; }
			if (x < cam_x * 2 - 200 || x > cam_x * 2 + 840 || y < cam_y * 2 - 200 || y > cam_y * 2 + 600) { s->on = 0; continue; }
			if (pl.state != PS_DEAD && boxes_meet(x, y, b, pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box))
			{
				dmg_src = s->kind; dmg_src_shot = 1;
				player_damage(s->dmg);
				s->on = 0;
			}
			continue;
		}

		/* ---- the player's shots ---- */
		const int pierce = s->kind == SH_PLASER || s->kind == SH_POWERLINE || s->kind == SH_BEAM;
		/* destructible walls */
		if (cx >= 0 && cy >= 0 && (blk(cx / 20, cy / 20) & B_DESTROY) && wall_break(cx / 20, cy / 20))
		{
			fx_add(&P_smoke, cx, cy, 4);
			sfx(SFX_EXPLODE);
			if (s->kind != SH_POWERLINE && s->kind != SH_BEAM) { s->on = 0; continue; }
		}
		const int wall = cx >= 0 && cy >= 0 && (blk(cx / 20, cy / 20) & B_WALL);
		if (s->kind == SH_BOUNCE)
		{
			if (wall)
			{
				if (s->bounce_walls)
				{
					const int hx = cx + (s->xs > 0 ? 10 : -10), hy = cy + (s->ys > 0 ? 10 : -10);
					int rh = hx >= 0 && cy >= 0 && (blk(hx / 20, cy / 20) & B_WALL);
					int rv = cx >= 0 && hy >= 0 && (blk(cx / 20, hy / 20) & B_WALL);
					if (!rh && !rv) rh = rv = 1;
					if (rh) s->xs = -s->xs;
					if (rv) s->ys = -s->ys;
					s->dmg -= 3;
					if (s->dmg <= 0) { fx_add(&P_flare, cx, cy, 3); s->on = 0; continue; }
					s->x += SYNC(s->xs); s->y += SYNC(s->ys);
				}
				else
				{
					bounce_split(s->gen + 1, s->x, s->y, 225, pl.wlevel[2]);
					bounce_split(s->gen + 1, s->x, s->y, 315, pl.wlevel[2]);
					fx_add(&P_flare, cx, cy, 3); sfx(SFX_HIT);
					s->on = 0; continue;
				}
			}
		}
		else if (s->kind == SH_POWERLINE)
		{
			const int ex = s->xs > 0 ? x + b->r : x, my = cam_y * 2 + 200;
			if (ex >= 0 && my >= 0 && (blk(ex / 20, my / 20) & B_WALL)) { s->on = 0; continue; }
		}
		else if (wall)
		{
			if (s->kind == SH_GRENADE) grenade_explode(cx, cy, s->dmg);
			else fx_add(&P_flare, cx, cy, 3);
			s->on = 0; continue;
		}

		/* far off the screen */
		if (x < cam_x * 2 - 200 || x > cam_x * 2 + 840 || y < cam_y * 2 - 300 || y > cam_y * 2 + 700) { s->on = 0; continue; }

		/* against the enemies on the screen */
		for (int k = 0; k < ntargets; k++)
		{
			Ent *e = &ents[targets[k]];
			if (e->type == E_NONE) continue;
			if (!boxes_meet(x, y, b, e->x >> 8, e->y >> 8, ent_box(e))) continue;
			if (e->type == E_DECKENTURM && e->state == 0) { if (!pierce) { s->on = 0; break; } continue; }
			if (s->kind == SH_GRENADE) { grenade_explode(cx, cy, s->dmg); s->on = 0; break; }
			if (e->type == E_POWERBLOCK && !e->state) { e->state = 1; e->energy = 12; }
			else e->energy -= s->dmg;
			e->hit = 4;
			fx_add(&P_flare, x + 8, y + 8, 3);
			sfx(SFX_HIT);
			if (!pierce) { s->on = 0; break; }
		}
	}
}

/* ------------------------------------------------------------------ the world's step */
#define ENEMY_BUDGET 8                   /* P1m: 8, not 10 (for the frame rate) */
#define LIFT_BUDGET 5                   /* while a kind holds the camera (the elevator ride) */
static int enemy_load;                  /* the enemies at or near the screen, big ones twice */
/* how much an enemy kind counts against the budget: 0 for pick-ups, platforms, triggers */
static int enemy_weight(int t)
{
	const Kind *k = &kinds[t];
	if (t == E_POWERBLOCK || k->boss || (!k->shootable && !k->touch)) return 0;
	if (t == E_DIAMANT || t == E_ONEUP || t == E_EXTRA) return 0;
	return k->pic && k->pic->fw * k->pic->fh > 40 * 40 ? 2 : 1;
}
static void world_activate(void)
{
	/* entities come to life when they reach the screen (Hurrican: on screen = Active) and run
	 * from then on; far away (2 screens) they wait */
	const int sx0 = cam_x * 2, sy0 = cam_y * 2, sx1 = sx0 + 640, sy1 = sy0 + 400;
	nactive = 0;
	ntargets = 0;
	/* at most ENEMY_BUDGET enemies at once (for the frame rate): one that comes into view while
	 * the budget is used up waits (not run, not drawn) until there is room - as the enemies on
	 * the screen are shot, the next ones come in; a big one counts twice */
	int budget = (cam_lock_held ? LIFT_BUDGET : ENEMY_BUDGET) - enemy_load;
	int load = 0;
	/* the bats and bees already about (a swarm is never more than MAX_BATS / MAX_BEES) */
	int bats_near = 0, bees_near = 0;
	for (int i = 0; i < nents; i++)
		if (ents[i].active && (ents[i].type == E_FLEDERMAUS || ents[i].type == E_MUECKE || ents[i].type == E_SMALLWESPE))
		{
			const int x = ents[i].x >> 8, y = ents[i].y >> 8;
			if (x > sx0 - 640 && x < sx1 + 640 && y > sy0 - 480 && y < sy1 + 480)
			{
				if (ents[i].type == E_FLEDERMAUS) bats_near++;
				else bees_near++;
			}
			else ents[i].active = 0;             /* flown far off: asleep again (it counts no more) */
		}
	for (int i = 0; i < nents; i++)
	{
		Ent *e = &ents[i];
		if (e->type == E_NONE) continue;
		const Box *b = ent_box(e);
		const int x = e->x >> 8, y = e->y >> 8;
		if (!e->active)
		{
			if (x + b->r > sx0 - 40 && x + b->l < sx1 + 40 && y + b->b > sy0 - 40 && y + b->t < sy1 + 40)
			{
				const int w = enemy_weight(e->type);
				if (w && w > budget) continue;          /* waits for room */
				if (e->type == E_FLEDERMAUS) { if (bats_near >= MAX_BATS) continue; bats_near++; }
				if (e->type == E_MUECKE || e->type == E_SMALLWESPE) { if (bees_near >= MAX_BEES) continue; bees_near++; }
				budget -= w;
				e->active = 1;
				e->dir = pcx() > ecx(e);
				const int d = e->dir ? 1 : -1;
				e->xs *= d;
				e->xa *= d;
				if (e->type == E_WALKER) e->xs = FX(10) * d;
				if (e->type == E_SPIDERBOMB) e->xs = 0;
			}
			else continue;
		}
		if (x > sx1 + 640 || x + b->r < sx0 - 640 || y > sy1 + 480 || y + b->b < sy0 - 480) continue;
		active_list[nactive++] = (int16_t)i;
		if (x + b->r > sx0 - 200 && x < sx1 + 200 && y + b->b > sy0 - 200 && y < sy1 + 200) load += enemy_weight(e->type);
		if (kinds[e->type].shootable && x + b->r > sx0 - 100 && x < sx1 + 100 && y + b->b > sy0 - 100 && y < sy1 + 100)
			targets[ntargets++] = (int16_t)i;
	}
	enemy_load = load;
}

static uint32_t world_steps;           /* steps run (the far enemies' every other step) */
static void world_step(void)
{
	cam_lock = 0;
	dmg_src = -1; dmg_src_shot = 0;
	player_step();
	world_steps++;
	/* far off the screen (P1m, for the frame rate): an enemy runs every other step - not a boss,
	 * nothing to stand on, not while the camera is held (the elevator) */
	const int fx0 = cam_x * 2 - 240, fy0 = cam_y * 2 - 240, fx1 = cam_x * 2 + 640 + 240, fy1 = cam_y * 2 + 400 + 240;
	for (int k = 0; k < nactive; k++)
	{
		Ent *e = &ents[active_list[k]];
		if (e->type == E_NONE) continue;
		if (!cam_lock_held && ((uint32_t)active_list[k] + world_steps) & 1)
		{
			const int x = e->x >> 8, y = e->y >> 8;
			if ((x < fx0 || x > fx1 || y < fy0 || y > fy1) && !kinds[e->type].boss &&
			    e->type != E_PLATTFORM && e->type != E_BRUECKE && e->type != E_POWERBLOCK)
				continue;
		}
		dmg_src = e->type; dmg_src_shot = 0;
		ent_step(e, active_list[k]);
		if (e->type != E_NONE && e->energy <= 0 && ((kinds[e->type].shootable && e->type != E_POWERBLOCK) || e->type > E_SECRET))
			ent_explode(e);
	}
	shots_step();
	fx_tick();
}

/* ------------------------------------------------------------------ the stage */
static void stage_start(void)
{
	const uint8_t *o;
	uint32_t size;
	if (!pack_file("OBJECTS.BIN", &o, &size)) plat_fatal("STAGE PACK: NO OBJECTS.BIN");
	const int n = (int)rd32(o + 4);
	nents = 0;
	memset(ents, 0, sizeof ents);
	memset(shots, 0, sizeof shots);
	memset(fxs, 0, sizeof fxs);
	/* the walls shot away come back */
	memset(broken_bits, 0, sizeof broken_bits);
	nbroken = 0;
	clean_valid = 0;
	bufs[0].valid = bufs[1].valid = 0;
	fx start_x = FX(400), start_y = FX(400);
	int thin_bats = 0;
	static uint8_t thin_lift[64];
	memset(thin_lift, 0, sizeof thin_lift);
	for (int i = 0; i < n && nents < MAX_ENTS - 120; i++)
	{
		const uint8_t *r = o + 8 + 12 * i;
		const int id = rd16(r), x = (int16_t)rd16(r + 2), y = (int16_t)rd16(r + 4);
		const int v1 = (int16_t)rd16(r + 6), v2 = (int16_t)rd16(r + 8), oskill = rd16(r + 10);
		if (oskill > MAP_SKILL) continue;                          /* not at medium */
		int type = E_NONE, size_ = 0;
		switch (id)
		{
		case 0: if (v2 == 0) { start_x = FX(x); start_y = FX(y); } continue;
		case 1: type = E_ONEUP; break;
		case 2: type = E_DIAMANT; break;
		case 3: type = E_POWERBLOCK; break;
		case 4: type = E_SPITTER; break;
		case 5: type = E_WALKER; break;
		case 6: type = E_GUNTOWER; break;
		case 7: type = E_SPIDERBOMB; break;
		case 8: type = E_PIRANHA; break;
		case 9: type = E_MUECKE; break;
		case 10: type = E_DECKENTURM; break;
		case 11: case 12: case 13: case 14: type = E_KUGEL; size_ = id - 11; break;
		case 18: type = E_AUGE; break;
		case 25: type = E_SWIMWALKER; break;
		case 46: type = E_DRONE; break;
		case 48: type = E_PFLANZE; break;
		case 49: type = E_SPITTERBOMBE; break;
		case 50: type = E_NEST; break;
		case 145: type = E_ENDLEVEL; break;
		case 146: type = E_PLATTFORM; break;
		case 149: type = E_BRUECKE; break;
		case 171: type = E_SECRET; break;
		default:
			if (id >= 0 && id < 256 && id_type[id]) { type = id_type[id]; break; }
			continue;
		}
		/* swarms thinned (P1g, for the frame rate): every other bat; on the elevator (stage 3:
		 * the lift is a small space) every other boulder, bomb, eye, side rocket and stilt
		 * walker (P1h had one in three and fewer lava crabs: too empty) */
		if (id == 53 && (++thin_bats & 1)) continue;
		if (stage == 3)
		{
			int keep = 1;
			switch (id)
			{
			case 33: case 35: case 18: case 62: case 34: keep = 2; break;
			default: break;
			}
			if (keep > 1 && (++thin_lift[id] % keep) != 1) continue;
		}
		Ent *e = &ents[nents++];
		memset(e, 0, sizeof *e);
		e->type = (uint8_t)type;
		e->x = e->xo = FX(x);
		e->y = e->yo = FX(y);
		e->v1 = (int16_t)v1;
		e->v2 = (int16_t)v2;
		e->energy = kinds[type].energy;
		switch (type)
		{
		case E_KUGEL:
			{
				static const int16_t en[4] = { 20, 30, 40, 50 };
				e->size = (uint8_t)size_;
				e->energy = en[size_];
				e->ya = FX(5);
				e->xs = v1 ? FX(v1) : FX(8);
			}
			break;
		case E_ONEUP: e->ya = FX(2); break;
		case E_SPITTER: e->ya = FX(5); break;
		case E_SPIDERBOMB: e->ya = FX(5); break;
		case E_PIRANHA: e->xs = FX(8); break;
		case E_SWIMWALKER: e->xs = FX(7); break;
		case E_AUGE: e->xs = FX(12); e->ya = FX(4); break;
		case E_DRONE: e->ys = FX(-22); break;
		case E_SPITTERBOMBE: e->ys = FX(15); break;
		case E_PLATTFORM: e->xs = FX(v1); e->ys = FX(v2); break;
		case E_PFLANZE: e->t1 = 30; break;
		case E_WALKER: e->t1 = 0; break;
#define K2(name, id, ...) case E_##name: name##_init(e); break;
#include K2_LIST
#undef K2
		}
	}
	nents_map = nents;
#ifndef OJ32X_MARS
	if (getenv("HB_TRACE")) fprintf(stderr, "stage: %d objects in the map, %d kept\n", n, nents);
	if (getenv("HB_WARP"))                 /* PC check: start somewhere else (x,y) */
	{
		int wx = 0, wy = 0;
		if (sscanf(getenv("HB_WARP"), "%d,%d", &wx, &wy) == 2) { start_x = FX(wx); start_y = FX(wy); }
	}
#endif
	if (demo_warp) { start_x = FX(demo_warp_x); start_y = FX(demo_warp_y); }
	/* what a stage starts from (a recorded demo needs the same world every time) */
	enemy_load = 0; cam_lock = cam_lock_held = 0; pl_off_t = 0; secret_t = 0; world_steps = 0;
	player_spawn(start_x, start_y);
	pl.dir = 1;
	pl.safe_x = start_x;
	pl.safe_y = start_y;
	stage_clear_t = 0;
	boss_lock_on = 0; boss_bar_t = 0;
}

/* ------------------------------------------------------------------ camera */
static void camera_follow(int snap)
{
	/* the player a little left of the middle when he looks right (and the other way round),
	 * a little below the middle; eased */
	int tx = ((pl.x >> 8) + 35) / 2 - SCREEN_W / 2 + pl.dir * 30;
	int ty = ((pl.y >> 8) + 40) / 2 - VIEW_H / 2 - 10 - HUD_H / 2;   /* the middle of the level's part */
	if (cam_lock) { tx = cam_lock_x / 2; ty = cam_lock_y / 2; }
	cam_lock_held = cam_lock;
	if (boss_lock_on && !snap)
	{
		/* to the arena at the boss's speed, then held */
		const int bx = boss_lock_x / 2, by = boss_lock_y / 2, sp = boss_lock_speed / 12 + 1;
		cam_x += bx > cam_x ? (bx - cam_x < sp ? bx - cam_x : sp) : -(cam_x - bx < sp ? cam_x - bx : sp);
		cam_y += by > cam_y ? (by - cam_y < sp ? by - cam_y : sp) : -(cam_y - by < sp ? cam_y - by : sp);
		return;
	}
	int nx = snap ? tx : cam_x + (tx - cam_x) / 4;
	int ny = snap ? ty : cam_y + (ty - cam_y) / 3;
	if (!snap)
	{
		if (nx == cam_x && tx != cam_x) nx += tx > cam_x ? 1 : -1;
		if (ny == cam_y && ty != cam_y) ny += ty > cam_y ? 1 : -1;
	}
	const int mx = map_w * 10 - VIEW_W, my = map_h * 10 - VIEW_H;
	if (nx > mx) nx = mx;
	if (ny > my) ny = my;
	if (nx < 0) nx = 0;
	if (ny < 0) ny = 0;
	cam_x = nx;
	cam_y = ny;
}

/* ------------------------------------------------------------------ drawing a frame */
static void draw_player(uint32_t t)
{
	if (pl.state == PS_DEAD) return;
	if ((pl.hurt > 0 || pl.safe_t > 0) && !pl.wheel && (t & 1)) return;   /* hurt or just back: blinks */
	const Pic *p;
	int f = pl.anim;
	if (pl.wheel && P_rad.count)         /* rolling as a ball (RADELN) */
	{
		draw_pic(&P_rad, (int)(t / 2 % (uint32_t)P_rad.count), pl.x + FX(10), pl.y + FX(40), pl.dir < 0);
		return;
	}
	if (pl.state == PS_DUCK) { p = &P_crouch; f = 0; }
	else if (pl.state == PS_JUMP) p = pl.aim == 2 ? &P_jumpup : pl.aim == 1 ? &P_jumpdiag : &P_jump;
	else if (pl.aim == 2) { p = &P_shootup; f = 0; }
	else if (pl.aim == 1) { p = &P_shootdiag; f = 0; }
	else if (pl.state == PS_RUN) p = &P_run;
	else p = &P_idle;
	if (!p->count) p = &P_idle;
	draw_pic(p, f % (p->count ? p->count : 1), pl.x, pl.y, pl.dir < 0);
}

static void draw_world(uint32_t t)
{
	/* map objects, enemies, pick-ups */
	const int sx0 = view_x * 2 - 300, sy0 = view_y * 2 - 300, sx1 = view_x * 2 + 660, sy1 = view_y * 2 + 420;
	for (int k = 0; k < nactive; k++)
	{
		const Ent *e = &ents[active_list[k]];
		if (e->type == E_NONE || e->type == E_ENDLEVEL || e->type == E_SECRET) continue;
		if (e->type == E_POWERBLOCK && !e->state) continue;     /* hidden until shot */
		const int x = e->x >> 8, y = e->y >> 8;
		if (x < sx0 || x > sx1 || y < sy0 || y > sy1) continue;
		if (e->hit && (t & 1)) continue;                         /* hit: flickers */
		switch (e->type)
		{
#define K2(name, id, ...) case E_##name: if (name##_draw(e, t)) continue; break;
#include K2_LIST
#undef K2
		default: break;
		}
		const Pic *p = ent_pic(e);
		if (!p || !p->count) continue;
		draw_pic(p, e->anim, e->x, e->y, kinds[e->type].mirror && e->dir);
	}
	draw_player(t);
	/* the lightning: a few blitzstrahl sprites along the beam (no per-pixel line) */
	if (pl.blitz == 1 && pl.blitz_reach > 0)
	{
		const int ux = cos_d(pl.blitz_ang), uy = sin_d(pl.blitz_ang);
		int sx = (pl.x >> 8) + 20 + 16 * ux / 256, sy = (pl.y >> 8) + 21 + 16 * uy / 256;
		for (int i = 0; i < pl.blitz_reach; i++)
		{
			draw_pic(&P_blitz[(pl.blitz_anim / 2 + i) & 3], 0, FX(sx), FX(sy), 0);
			sx += 32 * ux / 256; sy += 32 * uy / 256;
		}
	}
	/* shots */
	for (int i = 0; i < MAX_SHOTS; i++)
	{
		const Shot *s = &shots[i];
		if (!s->on) continue;
		const Pic *p;
		int f = 0, mirror = 0;
		if (s->kind == SH_POWERLINE)
		{
			/* a vertical stack of the power-line sprite down the column */
			const int ph = 2 * P_powerline.fh > 0 ? 2 * P_powerline.fh : 20;
			for (int yy = view_y * 2; yy < (view_y + PLAY_H) * 2; yy += ph)
				draw_pic(&P_powerline, s->anim % (P_powerline.count ? P_powerline.count : 1),
				         s->x, FX(yy), s->xs > 0);
			continue;
		}
		switch (s->kind)
		{
		case SH_SPREAD: p = s->anim ? &P_spreadbig : &P_spread; f = s->rot % (p->count ? p->count : 1); break;
		case SH_LASER: p = &P_laser; mirror = s->xs > 0; break;
		case SH_PFLANZE: p = &P_pflshot; f = (int)(t / 2 % 4); break;
		case SH_CANON: p = &P_canon; break;
		case SH_PLASER: p = s->pic; f = p->count ? s->rot % p->count : 0; break;
		case SH_BOUNCE: p = s->pic; break;
		case SH_GRENADE: p = &P_grenade; break;
		case SH_BEAM: p = s->pic; f = p->count ? (int)(t / 2 % (uint32_t)p->count) : 0; mirror = s->xs > 0; break;
		case SH_PIC: p = s->pic; f = p->count ? (int)(t / 3 % (uint32_t)p->count) : 0; mirror = s->anim && s->xs > 0; break;
		default: p = &P_dronebullet; f = (int)(t / 2 % 4); break;
		}
		draw_pic(p, f, s->x, s->y, mirror);
	}
	/* effects */
	for (int i = 0; i < MAX_FX; i++)
		if (fxs[i].p) draw_pic(fxs[i].p, fxs[i].frame, fxs[i].x, fxs[i].y, 0);
}

static int find_colour(int r, int g, int b)
{
	int best = 1, bd = 1 << 30;
	for (int i = 1; i < 97; i++)                  /* the resident colours: the same in every stage */
	{
		const int c = palette[i];
		const int dr = (c & 31) - r, dg = (c >> 5 & 31) - g, db = (c >> 10 & 31) - b;
		const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

static void draw_hud(uint32_t frame)
{
	char t[48], *q;
	/* the band: drawn again only when something in it changed (P1m) */
	static int32_t sig_last[16];
	static int band_ok;
	const int e = pl.energy > 0 ? (int)(pl.energy >> 8) : 0;
	const int32_t sig[16] = { e, pl.lives, (int32_t)pl.score, pl.gems, pl.wlevel[0], pl.wlevel[1], pl.wlevel[2], pl.blitzlen,
	                          pl.sel, pl.grenades, pl.powerlines, pl.smartbombs, col_dark, col_light, col_yellow, col_grey };
	if (!band_ok || memcmp(sig, sig_last, sizeof sig))
	{
		memcpy(sig_last, sig, sizeof sig);
		band_ok = 1;
		hud_into_band = 1;
		memset(hud_band, col_dark, sizeof hud_band);
		/* energy bar */
		hud_box(6, 3, 54, 8, col_dark);
		const int w = e / 2;
		if (w > 0) hud_box(8, 5, w, 4, e > 50 ? col_green : e > 25 ? col_yellow : col_red);
		/* the original's HUD (HUD.cpp: lives, score, the weapons with their levels, grenades /
		 * power lines / smart bombs, diamonds) stripped down to numbers and short names, two rows */
		q = put_str(t, "x");
		put_dec(q, (uint32_t)(pl.lives > 0 ? pl.lives : 0), 1);
		hud_text(64, 1, t, col_light);
		put_dec(t, pl.score, 7);
		hud_text(96, 1, t, col_light);
		q = put_str(t, "GEMS ");
		put_dec(q, (uint32_t)pl.gems, 3);
		hud_text(SCREEN_W - 8 * 8 - 6, 1, t, col_light);
		static const char *const wn[4] = { "SPR", "LAS", "BNC", "LIT" };
		for (int i = 0; i < 4; i++)
		{
			q = put_str(t, wn[i]);
			put_dec(q, (uint32_t)(i < 3 ? pl.wlevel[i] : pl.blitzlen - 1), 1);
			hud_text(6 + 40 * i, 12, t, i == pl.sel ? col_yellow : col_grey);
		}
		q = put_str(t, "GR");
		q = put_dec(q, (uint32_t)(pl.grenades > 0 ? pl.grenades : 0), 1);
		q = put_str(q, " PL");
		q = put_dec(q, (uint32_t)(pl.powerlines > 0 ? pl.powerlines : 0), 1);
		q = put_str(q, " SB");
		put_dec(q, (uint32_t)(pl.smartbombs > 0 ? pl.smartbombs : 0), 1);
		hud_text(SCREEN_W - 11 * 8 - 6, 12, t, col_light);
		hud_into_band = 0;
	}
	for (int r = 0; r < HUD_H; r++) fb_put(lin_of(cam_x, cam_y + r), hud_band[r], SCREEN_W, 0);
	/* the boss's energy */
	if (boss_bar_t > 0)
	{
		const int bw = boss_bar_e > 0 ? boss_bar_e * 120 / boss_bar_max : 0;
		hud_box(SCREEN_W / 2 - 62, VIEW_H - 16, 124, 8, col_dark);
		if (bw > 0) hud_box(SCREEN_W / 2 - 60, VIEW_H - 14, bw > 120 ? 120 : bw, 4, col_red);
		boss_bar_t--;
	}
	(void)frame;
}

/* ------------------------------------------------------------------ main */
static const uint8_t *need(const char *name)
{
	const uint8_t *d;
	uint32_t size;
	if (!pack_file(name, &d, &size)) plat_fatal(name);
	return d;
}

enum { GS_MENU, GS_INTRO, GS_PLAY, GS_PAUSE, GS_CLEAR, GS_OVER, GS_CREDITS, GS_STORY };


/* CD audio tracks (tools/it2wav.c, build.sh): 2 the menu, 3 stage clear, 4 game over,
 * 5-13 the stages' music */
#define TRACK_MENU     2
#define TRACK_CLEAR    3
#define TRACK_OVER     4
#define TRACK_STAGE(n) (4 + (n))
#define TRACK_ONCE     0x8000
/* the music (options menu: MUSIC ON / OFF): every track goes through music() */
static int opt_music = 1;
static int music_in_job, music_queued = -1;   /* P1m: the steps on the slave SH2 leave the CD to the master */
static void music(int track)
{
	if (music_in_job) { music_queued = track; return; }
	plat_music(opt_music ? track : 0);
}
#define TRACK_BOSS(n)  (14 + (n))       /* 15-23: the stages' boss songs (14 = the credits) */
__attribute__((unused)) static void music_boss(void) { music(TRACK_BOSS(stage)); }
__attribute__((unused)) static void music_stage(void) { music(TRACK_STAGE(stage)); }

static int stage = 1;

/* what is in the cart from start-up: the resident pack's pictures and colours */
static void load_resident(void)
{
	if (!bank_open(&bank_res, "RSPRITES.BIN")) plat_fatal("RESIDENT PACK: NO RSPRITES.BIN");
	memset(&bank_stage, 0, sizeof bank_stage);
	memset(palette, 0, sizeof palette);
	const uint8_t *d;
	uint32_t size;
	if (pack_file("PALRES.BIN", &d, &size))
		for (uint32_t i = 0; i < size / 2 && i < 256; i++) palette[i] = rd16(d + 2 * i);
	palette[0] = 0x0400;                           /* black behind the title and the menus (0x0000 would be see-through) */
	col_dark = find_colour(1, 1, 2);
	col_light = find_colour(31, 31, 31);
	col_red = find_colour(28, 3, 2);
	col_yellow = find_colour(30, 27, 4);
	col_green = find_colour(4, 28, 6);
	col_grey = find_colour(14, 14, 15);
	make_sintab();
	rng_state ^= plat_ticks_ms();
}

/* stage n's pack from the disc (the platform shows the loading screen meanwhile). The CD
 * plays no music while it reads data (Kobo rule): the music stops first. */
static int stage_in_cart;          /* the stage whose pack is in the RAM cart (0 = none yet) */
static void load_stage(int n)
{
	/* the pack of the stage loaded last is still in the cart (the SH2 only reads it): playing
	 * that stage again (restart, game over, main menu) needs no disc read - only a new mount */
	int32_t len = 1;
	if (n != stage_in_cart)
	{
		stage_in_cart = 0;
		plat_music(0);
		len = plat_load_pack(PACK_LEVEL_ID0 + n, STAGE_CART, STAGE_MAX);
	}
	if (len <= 0 || !pack_mount_at(1, plat_cart_ptr(STAGE_CART)))
	{
		static char msg[40];
		char *q = put_str(msg, "STAGE PACK OJ00");
		q = put_dec(q, (uint32_t)n, 1);
		put_str(q, ".PAK: NOT LOADED");
		plat_fatal(msg);
	}
	stage_in_cart = n;
	if (!bank_open(&bank_stage, "SSPRITES.BIN")) plat_fatal("STAGE PACK: NO SSPRITES.BIN");
	const uint8_t *p = need("PALETTE.BIN");
	for (int i = 0; i < 256; i++) palette[i] = rd16(p + 2 * i);
	p = need("TILES.BIN");
	ntiles = (int)rd32(p + 4);
	tile_px = p + 12 + ((ntiles + 3) & ~3);
	p = need("MAP.BIN");
	map_w = rd16(p + 4); map_h = rd16(p + 6); map_bw = rd16(p + 8);
	const int ncells = (int)rd32(p + 12);
	map_cells = p + 20;
	map_blockmap = map_cells + CELL_BYTES * ncells;
	map_blocks = map_blockmap + 2 * map_bw * rd16(p + 10);
	if ((uint32_t)map_w * (uint32_t)map_h > sizeof broken_bits * 8u) plat_fatal("MAP TOO BIG");
	p = need("LIGHT.BIN");
	light_blockmap = p + 16;
	light_blocks = light_blockmap + 2 * rd16(p + 8) * rd16(p + 10);
	shade = need("SHADE.LUT");
	load_pictures();
	kinds_init();
}

/* ------------------------------------------------------------------ the menu */
static int menu_page, menu_sel, opt_stage = 1, opt_lives = 3, menu_hold;

/* the menu screen: drawn whole every frame (the view at the world's corner) */
static void menu_draw(uint32_t frame)
{
	cam_x = cam_y = view_x = view_y = 0;
	for (uint32_t l = 0; l < (uint32_t)VIEW_H * RING_W; l += 2) PUT2(ring, l, 0, 0);
	const Pic logo = pic("hurrican-logo.png");
	if (logo.count)
	{
		Frame f;
		frame_of(&logo, 0, &f);
		/* its visible part centred (the frame's trimmed box), like the words under it */
		draw_frame(&f, logo.fw, SCREEN_W / 2 - f.x0 - f.w / 2, 22, 0, 0);
	}
	char t[40], *q;
	if (menu_page == 0)
	{
		/* the words centred under the logo, the cursor in front of them */
		hud_text(SCREEN_W / 2 - 40, 100, "START GAME", menu_sel == 0 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 28, 118, "OPTIONS", menu_sel == 1 ? col_yellow : col_light);
		hud_text(menu_sel == 0 ? SCREEN_W / 2 - 56 : SCREEN_W / 2 - 44, menu_sel == 0 ? 100 : 118, ">", col_yellow);
	}
	else if (menu_page == 2)
	{
		/* OPTIONS - CONTROLS (two columns): the button in yellow, what it does in white */
		static const char *const ctl[14][2] = {
			{ "L/R", "WALK" }, { "UP", "AIM UP" }, { "U+L/R", "DIAG" },
			{ "DOWN", "DUCK" }, { "B", "SHOOT" }, { "C", "JUMP" }, { "A", "LIGHTNING" },
			{ "A+B", "BEAM" }, { "X", "NEXT WPN" }, { "Y", "GRENADE" },
			{ "Z", "PWR LINE" }, { "DN+Z/A", "WHEEL" }, { "UP+Z", "SMART B" }, { "START", "PAUSE" } };
		hud_text(SCREEN_W / 2 - 32, 96, "CONTROLS", col_light);
		for (int i = 0; i < 14; i++)
		{
			const int col = i / 7, r = i % 7;
			const int bx = col ? 182 : 8, ax = col ? 232 : 70;
			hud_text(bx, 110 + 11 * r, ctl[i][0], col_yellow);
			hud_text(ax, 110 + 11 * r, ctl[i][1], col_light);
		}
		hud_text(SCREEN_W / 2 - 24, 188, "> BACK", col_yellow);
	}
	else
	{
		hud_text(SCREEN_W / 2 - 28, 86, "OPTIONS", col_light);
		q = put_str(t, menu_sel == 0 ? "> LEVEL       < STAGE " : "  LEVEL       < STAGE ");
		q = put_dec(q, (uint32_t)opt_stage, 1);
		put_str(q, " >");
		hud_text(SCREEN_W / 2 - 104, 98, t, menu_sel == 0 ? col_yellow : col_light);
		q = put_str(t, menu_sel == 1 ? "> LIVES       < " : "  LIVES       < ");
		q = put_dec(q, (uint32_t)opt_lives, 1);
		put_str(q, " >");
		hud_text(SCREEN_W / 2 - 104, 109, t, menu_sel == 1 ? col_yellow : col_light);
		q = put_str(t, menu_sel == 2 ? "> MUSIC       < " : "  MUSIC       < ");
		q = put_str(q, opt_music ? "ON" : "OFF");
		put_str(q, " >");
		hud_text(SCREEN_W / 2 - 104, 120, t, menu_sel == 2 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 104, 131, menu_sel == 3 ? "> CONTROLS" : "  CONTROLS", menu_sel == 3 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 104, 142, menu_sel == 4 ? "> STORY" : "  STORY", menu_sel == 4 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 104, 153, menu_sel == 5 ? "> CREDITS     (AND MUSIC)" : "  CREDITS     (AND MUSIC)", menu_sel == 5 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 104, 164, menu_sel == 6 ? "> BACK" : "  BACK", menu_sel == 6 ? col_yellow : col_light);
		hud_text(SCREEN_W / 2 - 68, 189, "LEFT/RIGHT CHANGE", col_grey);
	}
	(void)frame;
}

/* one menu frame of input; returns 1 when a game is to start, 2 for the demo, 3 the credits, 4 the story */
static int menu_input(int hit, int held)
{
#ifndef OJ32X_MARS
	/* PC check: HB_STAGE=n starts stage n at once */
	static int host_auto = 1;
	if (host_auto && getenv("HB_STAGE"))
	{
		host_auto = 0;
		opt_stage = atoi(getenv("HB_STAGE"));
		if (opt_stage < 1 || opt_stage > NSTAGES) opt_stage = 1;
		return getenv("HB_DEMO") ? 2 : 1;          /* HB_DEMO=1: the demo */
	}
#endif
	const int items = menu_page == 0 ? 2 : menu_page == 2 ? 1 : 7;
	if (hit & PAD_UP) menu_sel = (menu_sel + items - 1) % items;
	if (hit & PAD_DOWN) menu_sel = (menu_sel + 1) % items;
	/* left / right: once per press, then repeating while held */
	int step = 0;
	if (held & (PAD_LEFT | PAD_RIGHT))
	{
		const int d = (held & PAD_LEFT) ? -1 : 1;
		if (hit & (PAD_LEFT | PAD_RIGHT)) { step = d; menu_hold = 0; }
		else if (++menu_hold > 12 && (menu_hold & 1)) step = d;
	}
	else menu_hold = 0;
	const int ok = hit & (PAD_START | PAD_B | PAD_C | PAD_A);
	if (menu_page == 0)
	{
		if (ok && menu_sel == 0) return 1;
		if (ok && menu_sel == 1) { menu_page = 1; menu_sel = 0; }
	}
	else if (menu_page == 2)
	{
		if (ok) { menu_page = 1; menu_sel = 3; }   /* CONTROLS -> back to OPTIONS */
	}
	else
	{
		if (menu_sel == 0 && step) opt_stage = (opt_stage - 1 + step + NSTAGES) % NSTAGES + 1;
		if (menu_sel == 1 && step)
		{
			opt_lives += step;
			if (opt_lives < 1) opt_lives = 99;
			if (opt_lives > 99) opt_lives = 1;
		}
		if (menu_sel == 2 && (step || ok)) { opt_music ^= 1; music(TRACK_MENU); }
		if (ok && menu_sel == 3) { menu_page = 2; menu_sel = 0; }
		if (ok && menu_sel == 4) return 4;          /* the story */
		if (ok && menu_sel == 5) return 3;          /* the credits */
		if (ok && menu_sel == 6) { menu_page = 0; menu_sel = 1; }
	}
	return 0;
}

/* a stage from its start (the pack loaded, the music on) */
static void stage_go(int n)
{
	stage = n;
	load_stage(n);
	stage_start();
	camera_follow(1);
	music(TRACK_STAGE(n));
}

/* a recorded demo from its start: the same world every time (see "the recorded demos") */
static void demo_rec_start(int k)
{
	const DemoRec *d = &demo_recs[k];
	memset(&pl, 0, sizeof pl);
	pl.lives = 99;
	demo = 1; demo_rec = k;
	demo_rec_pos = demo_rec_left = demo_rec_t = 0; demo_rec_pad_now = 0;
	demo_warp = d->x || d->y; demo_warp_x = d->x; demo_warp_y = d->y;
	rng_state = d->seed;
	stage_go(d->stage);
	demo_warp = 0;
}

/* a demo of the stage-playing robot (OPTIONS - DEMO: every stage in turn) */
static void demo_bot_start(int n)
{
	demo = 1; demo_rec = -1;
	pl.lives = 99; pl.score = 0; pl.gems = 0;
	stage_go(n);
	demo_reset();
}

#include "credits.c"
#include "story.c"

/* ---- the main loop's state (P1m: file-wide, the slave SH2 runs the steps) */
static int gs = GS_MENU, gs_t, demo_next, steps, hit;
static uint32_t last_ms, acc;
static int par_on = 1;                  /* PAUSE: 2ND CPU ON / OFF */

/* this frame's steps (60 a second) */
static void run_steps(void)
{
	for (int s = 0; s < steps; s++)
	{
		gs_t++;
		switch (gs)
		{
		case GS_INTRO:
			if (demo_rec >= 0) { camera_follow(0); world_activate(); }   /* step by step (see the recorded demos) */
			if (gs_t > 120 || (hit & (PAD_B | PAD_C | PAD_A))) { gs = GS_PLAY; gs_t = 0; }
			break;
		case GS_PLAY:
			if (demo_rec >= 0) pad = demo_rec_pad();
			else if (demo) pad = demo_pad();
			world_step();
			if (demo_rec >= 0)
			{
				camera_follow(0);
				world_activate();
				if (demo_rec_check()) { demo_next = 1; s = steps; break; }
			}
			if (pl.state == PS_DEAD && pl.dead_t > 90)
			{
				pl.lives--;
				if (pl.lives <= 0) { gs = GS_OVER; gs_t = 0; music(TRACK_OVER | TRACK_ONCE); }
				else
				{
					/* the user's request: every life starts with all weapons full again, so
					 * no "weapon one level down" here - player_spawn refills them */
					player_spawn(pl.safe_x, pl.safe_y - FX(10));
					pl.safe_t = 150;          /* 2.5 s without damage */
				}
			}
			if (stage_clear_t && demo) { demo_next = 1; s = steps; break; }   /* the demo ends with the stage */
			if (stage_clear_t) { gs = GS_CLEAR; gs_t = 0; music(TRACK_CLEAR | TRACK_ONCE); sfx(SFX_SECRET); }
			break;
		case GS_PAUSE:
			break;
		case GS_CLEAR:
			fx_tick();
			if (gs_t > 60 * 8 || (gs_t > 120 && (hit & PAD_START)))
			{
				if (stage < NSTAGES) { stage_go(stage + 1); gs = GS_INTRO; }
				else { gs = GS_MENU; menu_page = 0; menu_sel = 0; music(TRACK_MENU); }
				gs_t = 0;
				s = steps;
				last_ms = plat_ticks_ms(); acc = 0;
			}
			break;
		case GS_OVER:
			fx_tick();
			if (gs_t > 60 * 8 || (gs_t > 120 && (hit & PAD_START)))
			{
				gs = GS_MENU; gs_t = 0; menu_page = 0; menu_sel = 0;
				music(TRACK_MENU);
				s = steps;
			}
			break;
		default:
			break;
		}
	}
}

/* after the steps: the camera and the entities' wake-up (a recorded demo does them each step) */
static void after_steps(void)
{
	if (demo_rec < 0)
	{
		if (gs == GS_PLAY || gs == GS_INTRO) camera_follow(0);
		world_activate();
	}
}

/* the frame buffer to draw and the view (the camera as it is now) */
static void frame_begin(void)
{
	ring = (volatile uint8_t *)plat_scroll_ring();
	cur = &bufs[plat_back_index()];
	view_x = cam_x & ~1;              /* the line table starts lines at even bytes */
	view_y = cam_y + HUD_H;           /* the level below the HUD's band */
}

/* the level's tiles: what came into view into CLEAN, and into this frame buffer */
static void draw_tiles(void)
{
	/* the clean tile layers: what came into view */
	{
		Rect parts[2];
		const int n = clean_valid ? view_new_parts(view_x, view_y, clean_vx, clean_vy, parts)
		                          : (parts[0] = (Rect){ (int16_t)view_x, (int16_t)view_y, (int16_t)(view_x + VIEW_W), (int16_t)(view_y + PLAY_H) }, 1);
		for (int i = 0; i < n; i++) compose_clean(parts[i].x0, parts[i].y0, parts[i].x1, parts[i].y1);
		clean_valid = 1; clean_vx = view_x; clean_vy = view_y;
	}
	/* this buffer: what came into view since its last frame, and what that frame drew on */
	if (!cur->valid || cur->overflow)
		rect_from_clean(view_x, view_y, view_x + VIEW_W, view_y + PLAY_H);
	else
	{
		Rect parts[2];
		const int n = view_new_parts(view_x, view_y, cur->vx, cur->vy, parts);
		for (int i = 0; i < n; i++) rect_from_clean(parts[i].x0, parts[i].y0, parts[i].x1, parts[i].y1);
		for (int i = 0; i < cur->nrects; i++)
			rect_from_clean(cur->rects[i].x0, cur->rects[i].y0, cur->rects[i].x1, cur->rects[i].y1);
	}
	cur->valid = 1; cur->overflow = 0; cur->nrects = 0;
	cur->vx = view_x; cur->vy = view_y;
}

/* the parallel job: 0 (this SH2) the tiles, 1 (the slave) the steps. The slave's own stack is
 * 2 KB (port/mars/crt0_oj.s, unchanged): the steps run on a stack of their own */
static void par_steps(void) { run_steps(); after_steps(); }
#ifdef OJ32X_MARS
static uint32_t slave_stack[2048] __attribute__((aligned(16)));   /* 8 KB */
void call_on_stack(void (*fn)(void), void *top);
__asm__(
	"	.text\n"
	"	.align 2\n"
	"	.global _call_on_stack\n"
	"_call_on_stack:\n"           /* r4 = fn, r5 = the new stack's top */
	"	mov.l	r14,@-r15\n"
	"	sts.l	pr,@-r15\n"
	"	mov	r15,r14\n"
	"	mov	r5,r15\n"
	"	jsr	@r4\n"
	"	nop\n"
	"	mov	r14,r15\n"
	"	lds.l	@r15+,pr\n"
	"	rts\n"
	"	mov.l	@r15+,r14\n");
#endif
static void par_job(void *arg, int part)
{
	(void)arg;
	if (part == 0) draw_tiles();
#ifdef OJ32X_MARS
	else call_on_stack(par_steps, slave_stack + 2048);
#else
	else par_steps();
#endif
}

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	plat_init();
	load_resident();
	music(TRACK_MENU);

	int attract = 0, demo_first = 1;
	uint32_t idle_ms = 0;
	last_ms = plat_ticks_ms();
	uint32_t frame = 0;
	/* the timing line (pause menu: TIMING ON / OFF): milliseconds per frame, averaged over 16 frames */
	enum { T_LOGIC, T_MAP, T_COPY, T_SPR, T_OVR, T_HUD, T_WAIT, T_N };
	int show_times = 0, pause_sel = 0;
	uint32_t tacc[T_N] = { 0 }, tshow[T_N] = { 0 }, tframes = 0, tms = 0, fps10 = 0;
	static char tline[64], tline2[64];
	for (;;)
	{
		/* time: 60 steps a second, at most 6 a frame */
		const uint32_t now = plat_ticks_ms();
		uint32_t dms = now - last_ms;
		last_ms = now;
		if (dms > 200) dms = 200;
		acc += dms * 60u;
		steps = (int)(acc / 1000u);
		acc -= (uint32_t)steps * 1000u;
		if (steps > 6) steps = 6;

		pad = plat_read_pad();
		const int pad_real = pad;               /* the demo replaces pad in its steps */
		hit = pad & ~pad_prev;
		uint32_t t0 = plat_prof_us(), t1;
		if (demo && gs != GS_MENU && !hit && (demo_next || (demo_rec < 0 && gs == GS_PLAY && demo_t > 60 * 120)))
		{
			/* this demo is over: the next one (the recorded ones in turn; OPTIONS - DEMO: the
			 * stages in turn), after the last back to the menu */
			demo_next = 0;
			int more = 0;
			if (demo_rec >= 0) { }               /* one recorded demo, then the title */
			else
			{
				const int n = stage % NSTAGES + 1;
				if (n != demo_first) { demo_bot_start(n); more = 1; }
			}
			if (more)
			{
				gs = GS_INTRO; gs_t = 0;
				pad_prev = pad;
				last_ms = plat_ticks_ms(); acc = 0;
				continue;
			}
			hit = PAD_START;                    /* the end: as a button */
		}
		if (demo && gs != GS_MENU && hit)
		{
			/* the demo ends: back to the options menu (the attract mode: the main menu) */
			demo = 0; demo_rec = -1; demo_next = 0;
			gs = GS_MENU;
			menu_page = 0; menu_sel = 0; attract = 0;   /* (the demos play only in the attract mode) */
			idle_ms = 0;
			music(TRACK_MENU);
			pad_prev = pad;
			continue;
		}
		if (gs == GS_PLAY && (hit & PAD_START)) { gs = GS_PAUSE; pause_sel = 0; }
		else if (gs == GS_PAUSE)
		{
			/* the pause menu: CONTINUE / MAIN MENU / TIMING ON-OFF */
			if (hit & PAD_UP) pause_sel = (pause_sel + 3) % 4;
			if (hit & PAD_DOWN) pause_sel = (pause_sel + 1) % 4;
			if ((hit & PAD_START) || ((hit & (PAD_B | PAD_C | PAD_A)) && pause_sel == 0)) gs = GS_PLAY;
			else if ((hit & (PAD_B | PAD_C | PAD_A)) && pause_sel == 1)
			{
				gs = GS_MENU; menu_page = 0; menu_sel = 0;
				music(TRACK_MENU);
				pad_prev = pad;
				continue;
			}
			else if ((hit & (PAD_B | PAD_C | PAD_A)) && pause_sel == 2) show_times ^= 1;
			else if ((hit & (PAD_B | PAD_C | PAD_A)) && pause_sel == 3) par_on ^= 1;
		}

		if (gs == GS_STORY)
		{
			/* OPTIONS - STORY: Hurrican's intro (story.c); any button ends it */
			int over = 0;
			for (int s = 0; s < steps; s++) over |= story_step();
			gs_t += steps;
			if ((hit && gs_t > 30) || over)
			{
				story_leave();
				gs = GS_MENU; menu_page = 1; menu_sel = 4;
				pad_prev = pad;
				continue;
			}
			pad_prev = pad;
			plat_wait_flip();
			ring = (volatile uint8_t *)plat_scroll_ring();
			cur = &bufs[plat_back_index()];
			cur->nrects = 0;
			story_draw();
			bufs[0].valid = bufs[1].valid = 0;
			plat_border_colour(0);
			plat_present_scroll(palette, 0, 0, VIEW_H);
			frame++;
			continue;
		}
		if (gs == GS_CREDITS)
		{
			/* OPTIONS - CREDITS: the plasma, the credits, the music player (credits.c) */
			if (credits_input(hit))
			{
				gs = GS_MENU; menu_page = 1; menu_sel = 5;
				music(TRACK_MENU);
				pad_prev = pad;
				continue;
			}
			pad_prev = pad;
			plat_wait_flip();
			ring = (volatile uint8_t *)plat_scroll_ring();
			cur = &bufs[plat_back_index()];
			cur->nrects = 0;
			credits_draw(steps > 0 ? (steps + 0) : 1);
			bufs[0].valid = bufs[1].valid = 0;
			clean_valid = 0;
			plat_border_colour(0);
			plat_present_scroll(palette, 0, 0, VIEW_H);
			frame++;
			continue;
		}
		if (gs == GS_MENU)
		{
			int go = menu_input(hit, pad);
			if (go == 3) { gs = GS_CREDITS; credits_enter(); pad_prev = pad; continue; }
			if (go == 4) { if (story_enter()) { gs = GS_STORY; gs_t = 0; } pad_prev = pad; last_ms = plat_ticks_ms(); acc = 0; continue; }
			/* the attract mode: a minute in the menus without a button - a random stage plays as
			 * the demo (back to the main menu afterwards) */
			idle_ms = pad ? 0 : idle_ms + dms;
			attract = 0;
#ifndef OJ32X_MARS
			if (getenv("HB_ATTRACT")) idle_ms = 60000u;   /* PC check: HB_ATTRACT=1 - the attract mode at once */
#endif
			if (!go && idle_ms >= 60000u) { go = 2; attract = 1; }
			if (go)
			{
				idle_ms = 0;
				demo_next = 0;
				if (attract && DEMO_RECS > 0)
				{
					/* a recorded demo at random, not the one shown last */
					static int last = -1;
					int k = rnd(DEMO_RECS ? DEMO_RECS : 1);
					if (k == last && DEMO_RECS > 1) k = (k + 1 + rnd(DEMO_RECS - 1)) % (DEMO_RECS ? DEMO_RECS : 1);
#ifndef OJ32X_MARS
					if (getenv("HB_DEMO_N")) k = (int)strtol(getenv("HB_DEMO_N"), 0, 10) % (DEMO_RECS ? DEMO_RECS : 1);   /* PC check: that demo */
#endif
					last = k;
					demo_rec_start(k);
				}
				else if (go == 2) { demo_first = attract ? 1 + rnd(NSTAGES) : opt_stage; demo_bot_start(demo_first); }
				else
				{
					demo = 0; demo_rec = -1;
					pl.lives = opt_lives; pl.score = 0; pl.gems = 0;
					stage_go(opt_stage);
				}
				gs = GS_INTRO; gs_t = 0;
				pad_prev = pad;
				last_ms = plat_ticks_ms(); acc = 0;
				continue;
			}
			pad_prev = pad;
			plat_wait_flip();
			ring = (volatile uint8_t *)plat_scroll_ring();
			cur = &bufs[plat_back_index()];
			cur->nrects = 0;
			menu_draw(frame);
			bufs[0].valid = bufs[1].valid = 0;
			clean_valid = 0;
			if (sfx_want) { plat_sfx(sfx_want); sfx_want = 0; }
			plat_border_colour(0);
			plat_present_scroll(palette, 0, 0, VIEW_H);
			frame++;
			continue;
		}

		/* P1m: the second SH2 runs the steps while this one brings the level's tiles up to date
		 * (the view of the camera before them) - only while playing; PAUSE: 2ND CPU ON / OFF */
		const int par = par_on && gs == GS_PLAY && steps > 0;
		if (par)
		{
			t1 = plat_prof_us(); tacc[T_LOGIC] += t1 - t0;
			plat_wait_flip();
			t0 = plat_prof_us(); tacc[T_WAIT] += t0 - t1;
			frame_begin();
			wall_defer = 1; music_in_job = 1;
			plat_parallel(par_job, 0);
			wall_defer = 0; music_in_job = 0;
#ifdef OJ32X_MARS
			*(volatile uint8_t *)0xFFFFFE92 = 0x11;   /* the slave's writes: purge this SH2's cache */
#endif
			for (int i = 0; i < nwall_pending; i++)
			{
				const int k = wall_pending[i];
				wall_redraw(broken[k].tx, broken[k].ty);
				rect_from_clean(broken[k].tx * 10, broken[k].ty * 10, broken[k].tx * 10 + 10, broken[k].ty * 10 + 10);
			}
			nwall_pending = 0;
			if (music_queued >= 0) { music(music_queued); music_queued = -1; }
			t1 = plat_prof_us(); tacc[T_LOGIC] += t1 - t0; t0 = t1;
		}
		else run_steps();
		pad_prev = pad_real;                    /* the player's own buttons (not the demo's) */
		tms += dms;

		if (gs == GS_MENU) continue;           /* the game ended: the menu from the next frame */
		if (!par)
		{
			after_steps();

			/* ---- draw */
			t1 = plat_prof_us(); tacc[T_LOGIC] += t1 - t0;
			plat_wait_flip();
			t0 = plat_prof_us(); tacc[T_WAIT] += t0 - t1;
			frame_begin();
			draw_tiles();
			t1 = plat_prof_us(); tacc[T_COPY] += t1 - t0; t0 = t1;
		}


		/* sprites, then the walls over them */
		over_list_n = 0;
		const uint32_t rng_keep = rng_state;    /* the drawing leaves the random numbers alone */
		draw_world(frame);
		t1 = plat_prof_us(); tacc[T_SPR] += t1 - t0; t0 = t1;
		for (int i = 0; i < over_list_n; i++) over_rect(over_list[i].x0, over_list[i].y0, over_list[i].x1, over_list[i].y1);
		t1 = plat_prof_us(); tacc[T_OVR] += t1 - t0; t0 = t1;

		/* HUD and messages */
		draw_hud(frame);
		rng_state = rng_keep;
		if (gs == GS_INTRO)
		{
			char t[12];
			put_dec(put_str(t, "STAGE "), (uint32_t)stage, 1);
			hud_text(SCREEN_W / 2 - 28, 84, t, col_light);
		}
		if (demo && (frame / 30) % 2) hud_text(SCREEN_W / 2 - 92, VIEW_H - 16, "DEMO - PRESS ANY BUTTON", col_yellow);
		if (gs == GS_PAUSE)
		{
			hud_text(SCREEN_W / 2 - 20, 70, "PAUSE", col_light);
			hud_text(SCREEN_W / 2 - 52, 92, pause_sel == 0 ? "> CONTINUE" : "  CONTINUE", pause_sel == 0 ? col_yellow : col_light);
			hud_text(SCREEN_W / 2 - 52, 108, pause_sel == 1 ? "> MAIN MENU" : "  MAIN MENU", pause_sel == 1 ? col_yellow : col_light);
			hud_text(SCREEN_W / 2 - 52, 124, show_times ? (pause_sel == 2 ? "> TIMING ON" : "  TIMING ON") : (pause_sel == 2 ? "> TIMING OFF" : "  TIMING OFF"),
			         pause_sel == 2 ? col_yellow : col_light);
			hud_text(SCREEN_W / 2 - 52, 140, par_on ? (pause_sel == 3 ? "> 2ND CPU ON" : "  2ND CPU ON") : (pause_sel == 3 ? "> 2ND CPU OFF" : "  2ND CPU OFF"),
			         pause_sel == 3 ? col_yellow : col_light);
		}
		if (gs == GS_CLEAR) hud_text(SCREEN_W / 2 - 48, 84, "STAGE CLEAR", col_light);
		if (gs == GS_OVER) hud_text(SCREEN_W / 2 - 36, 84, "GAME OVER", col_light);
		if (secret_t > 0) { hud_text(SCREEN_W / 2 - 52, 60, "SECRET AREA!", col_yellow); secret_t -= steps; }
		if (++tframes == 16)
		{
			for (int i = 0; i < T_N; i++) { tshow[i] = tacc[i] / 1600u; tacc[i] = 0; }   /* tenths of ms */
			fps10 = tms ? 16u * 10000u / tms : 0;
			tframes = 0; tms = 0;
			/* two lines: FPS, enemies at once, enemy shots; then the milliseconds of each part */
			int nshots = 0;
			for (int i = 0; i < MAX_SHOTS; i++) nshots += shots[i].on && shots[i].enemy;
			char *q = put_str(tline, "FPS ");
			q = put_dec(q, fps10 / 10, 1); q = put_str(q, "."); q = put_dec(q, fps10 % 10, 1);
			q = put_str(q, "  ENEMIES "); q = put_dec(q, (uint32_t)enemy_load, 1);
			q = put_str(q, "  SHOTS "); q = put_dec(q, (uint32_t)nshots, 1);
			static const char *const nm[T_N] = { "AI", " MAP", " CPY", " SPR", " OVR", " HUD", " WAIT" };
			q = tline2;
			for (int i = 0; i < T_N; i++) { q = put_str(q, nm[i]); q = put_dec(q, tshow[i] / 10, 1); }
		}
		if (show_times) { hud_text(4, VIEW_H - 26, tline, col_light); hud_text(4, VIEW_H - 14, tline2, col_light); }

		/* the copy of the ring's start after its end (lines that run past byte 76800) */
		for (uint32_t i = 0; i < MIRROR_N; i += 2)
		{
			const uint16_t w = GET2(ring, i);
			PUT2(ring, RING_LIN + i, HI(w), LO(w));
		}

		if (sfx_want) { plat_sfx(sfx_want); sfx_want = 0; }
#ifndef OJ32X_MARS
		{
			/* the work a frame does, averaged (HB_WORK=1) */
			static uint64_t sc, sp, ss, sa, n;
			sc += px_composed; sp += px_copied; ss += px_sprites; sa += (uint64_t)nactive; n++;
			px_composed = px_copied = px_sprites = 0;
			if (getenv("HB_WORK") && frame % 300 == 299)
				fprintf(stderr, "frame %u: per frame: composed %llu px, copied %llu px, sprites %llu px, active %llu\n", frame,
				        (unsigned long long)(sc / n), (unsigned long long)(sp / n), (unsigned long long)(ss / n), (unsigned long long)(sa / n));
		}
		if (getenv("HB_TRACE") && frame % 30 == 0)
			fprintf(stderr, "frame %u: player %d,%d state %d energy %d lives %d score %u gems %d weapon %d active %d broken %d\n",
			        frame, (int)(pl.x >> 8), (int)(pl.y >> 8), pl.state, (int)(pl.energy >> 8), pl.lives, pl.score, pl.gems,
			        pl.sel, nactive, nbroken);
		if (getenv("HB_ENEMIES") && frame % 30 == 0)
		{
			int on = 0, act = 0, map_in = 0;
			const int sx0 = cam_x * 2, sy0 = cam_y * 2;
			for (int i = 0; i < nents; i++)
			{
				const Ent *e = &ents[i];
				const int x = e->x >> 8, y = e->y >> 8;
				const int in = x > sx0 - 40 && x < sx0 + 640 && y > sy0 - 40 && y < sy0 + 400;
				if (e->type != E_NONE && kinds[e->type].shootable && e->type != E_POWERBLOCK && in) { on++; act += e->active; }
			}
			(void)map_in;
			fprintf(stderr, "ENEMIES frame %u at %d,%d: on screen %d (active %d) bats %d\n", frame, (int)(pl.x >> 8), (int)(pl.y >> 8), on, act, bats_awake());
		}
#endif
		plat_border_colour(0);
		plat_present_scroll(palette, (int)((cam_y + cam_x / RING_W) % VIEW_H), cam_x % RING_W, VIEW_H);
		t1 = plat_prof_us(); tacc[T_HUD] += t1 - t0;
		frame++;
	}
}
