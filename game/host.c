/*
 * Hurrican CD32X - the PC stand-in for port/plat_mars.c (the J19b calls the game uses), for
 * checking the game on the PC: the same game/p1.c, two 128 KB "frame buffers", the line table
 * read back the way the 32X reads it, frames saved as PPM pictures; sound effects and music
 * written to the log (HB_LOG=1).
 *
 *   HB_DIR=<folder with IMAGE8.RAW and OJ001.PAK>  HB_OUT=<folder for frames>
 *   HB_FRAMES=<stop after n frames>  HB_SAVE_EVERY=<n>  HB_MS=<ms per frame, default 66>
 *   HB_PAD="frame:BUTTONS,..."  (buttons UP DOWN LEFT RIGHT A B C X Y Z START, joined with +;
 *                                "-" = none)
 */
#include "plat.h"
#include "pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint8_t fbmem[2][0x20000];
static int back;                       /* the buffer being drawn */
static uint8_t *cart;                  /* 4 MB */
static uint32_t ticks, frame;
static uint16_t shown_pal[256];
static int border;
static uint32_t boot_bytes;

static const char *env(const char *n, const char *d) { const char *v = getenv(n); return v && *v ? v : d; }

void plat_fatal(const char *msg) { fprintf(stderr, "FATAL: %s\n", msg); exit(1); }
void plat_log(const char *msg) { fprintf(stderr, "[log] %s\n", msg); }
void plat_exit(int code) { exit(code); }

static uint32_t read_file(const char *name, uint32_t off, uint32_t max)
{
	char path[1024];
	snprintf(path, sizeof path, "%s/%s", env("HB_DIR", "."), name);
	FILE *f = fopen(path, "rb");
	if (!f) return 0;
	const size_t n = fread(cart + off, 1, max, f);
	fclose(f);
	return (uint32_t)n;
}

void plat_init(void)
{
	cart = calloc(1, 0x400000);
	boot_bytes = read_file("IMAGE8.RAW", PLAT_CART_DATA_OFFSET, 0x180000);
	if (!boot_bytes || !pack_mount(cart + PLAT_CART_DATA_OFFSET)) plat_fatal("no resident pack IMAGE8.RAW in HB_DIR");
}

const uint8_t *plat_cart_ptr(uint32_t off) { return cart + off; }

int32_t plat_load_pack(int id, uint32_t off, uint32_t max)
{
	char name[32];
	snprintf(name, sizeof name, "OJ%03d.PAK", id - PACK_LEVEL_ID0);
	if (getenv("HB_TRACE")) fprintf(stderr, "disc read: %s\n", name);
	const uint32_t n = read_file(name, off, max);
	return n ? (int32_t)n : -1;
}

#if OJ32X_CART_MB
/* ---- the 8 / 16 MB cart, modelled (port/plat.h, port/cartcache.c; as OpenJazz's
 * port/plat_host.c). HB_CART_MB=4|8|16 is the board (4: no mapper, 8: pages 4-7 mirror 0-3),
 * HB_CART_FILE=<file> keeps the pages from one run to the next. Window 1's content
 * (cart 0x200000-0x3EFFFF) is swapped with the page's store on a page change. */
#define HP_WIN   0x200000u
#define HP_ROOM  0x1F0000u
static int hp_now = 1, hp_board;
static uint8_t *hp_store[8];
static uint8_t hp_piece[8][16 * 4096];

static int hp_model(void)
{
	if (!hp_board) {
		hp_board = atoi(env("HB_CART_MB", "0"));
		if (!hp_board) hp_board = OJ32X_CART_MB;
		if (hp_board != 4 && hp_board != 8) hp_board = 16;
		fprintf(stderr, "[host] cart: %d MB board\n", hp_board);
	}
	return hp_board;
}
static int hp_phys(int page) { return hp_model() == 4 ? 1 : hp_model() == 8 ? (page & 3) : page; }

static void hp_save(void)
{
	const char *path = getenv("HB_CART_FILE");
	if (!path || !*path || !cart) return;
	const int now = hp_phys(hp_now);
	if (!hp_store[now]) hp_store[now] = malloc(HP_ROOM);
	memcpy(hp_store[now], cart + HP_WIN, HP_ROOM);
	FILE *f = fopen(path, "wb");
	if (!f) return;
	for (int i = 0; i < 8; i++) {
		const uint8_t has = hp_store[i] != NULL;
		fwrite(&has, 1, 1, f);
		if (has) fwrite(hp_store[i], 1, HP_ROOM, f);
	}
	fwrite(hp_piece, 1, sizeof hp_piece, f);
	fclose(f);
}
static void hp_restore(void)
{
	static int done;
	if (done) return;
	done = 1;
	atexit(hp_save);
	const char *path = getenv("HB_CART_FILE");
	FILE *f = path && *path ? fopen(path, "rb") : NULL;
	if (!f) return;
	for (int i = 0; i < 8; i++) {
		uint8_t has = 0;
		if (fread(&has, 1, 1, f) != 1) break;
		if (has) {
			hp_store[i] = malloc(HP_ROOM);
			if (fread(hp_store[i], 1, HP_ROOM, f) != HP_ROOM) break;
		}
	}
	if (fread(hp_piece, 1, sizeof hp_piece, f) != sizeof hp_piece) memset(hp_piece, 0, sizeof hp_piece);
	fclose(f);
	if (hp_store[1]) memcpy(cart + HP_WIN, hp_store[1], HP_ROOM);   /* page 1 after reset */
	fprintf(stderr, "[host] cart pages restored from %s\n", path);
}

int plat_cart_page_now(void) { return hp_now; }

int plat_cart_page(int page)
{
	hp_restore();
	if (page < 1 || page > 7) return 0;
	const int from = hp_phys(hp_now), to = hp_phys(page);
	hp_now = page;
	if (from == to) return 1;
	if (!hp_store[from]) hp_store[from] = malloc(HP_ROOM);
	if (!hp_store[to]) { hp_store[to] = malloc(HP_ROOM); memset(hp_store[to], 0xFF, HP_ROOM); }
	memcpy(hp_store[from], cart + HP_WIN, HP_ROOM);
	memcpy(cart + HP_WIN, hp_store[to], HP_ROOM);
	return 1;
}

static long hp_file(int id, uint8_t **data)
{
	char path[1024];
	snprintf(path, sizeof path, "%s/OJ%03d.PAK", env("HB_DIR", "."), id - PACK_LEVEL_ID0);
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	if (data) {
		fseek(f, 0, SEEK_SET);
		*data = malloc((size_t)len);
		if (fread(*data, 1, (size_t)len, f) != (size_t)len) len = -1;
	}
	fclose(f);
	return len;
}

int32_t plat_pack_open(int id) { return (int32_t)hp_file(id, NULL); }

int32_t plat_load_pack_at(int id, uint32_t cart_offset, uint32_t max_bytes)
{
	const long l = hp_file(id, NULL);
	if (l <= 0 || (uint32_t)l > max_bytes) return -1;
	if (getenv("HB_TRACE")) fprintf(stderr, "disc read: OJ%03d.PAK -> cart %06X\n", id - PACK_LEVEL_ID0, (unsigned)cart_offset);
	return plat_load_pack(id, cart_offset, max_bytes);
}

int plat_pack_same_as_disc(int id, uint32_t cart_offset, uint32_t len)
{
	uint8_t *d = NULL;
	const long l = hp_file(id, &d);
	int same = l == (long)len;
	if (same) {
		const uint8_t *c = cart + cart_offset;
		const uint32_t chunks = (len + 32767) >> 15, last = (chunks - 1) << 15;
		const uint32_t n0 = len < 32768 ? len : 32768, n1 = len - last;
		same = memcmp(c, d, n0) == 0 && memcmp(c + last, d + last, n1) == 0;
	}
	free(d);
	return same;
}

int plat_cart_piece_write(int piece, const uint16_t *words, int n)
{
	hp_restore();
	uint8_t *p = hp_piece[hp_phys(hp_now)] + (piece & 15) * 4096;
	for (int i = 0; i < n && i < 2048; i++) { p[2 * i] = (uint8_t)(words[i] >> 8); p[2 * i + 1] = (uint8_t)words[i]; }
	return 1;
}

void plat_cart_piece_read(int piece, uint16_t *words, int n)
{
	hp_restore();
	const uint8_t *p = hp_piece[hp_phys(hp_now)] + (piece & 15) * 4096;
	for (int i = 0; i < n && i < 2048; i++) words[i] = (uint16_t)(p[2 * i] << 8 | p[2 * i + 1]);
}

void plat_save_area_enter(void) { }
void plat_save_area_leave(void) { }
#endif

void plat_sfx(int id) { if (getenv("HB_LOG")) fprintf(stderr, "[%u] sfx %d\n", frame, id); }
void plat_music(int track) { if (getenv("HB_LOG")) fprintf(stderr, "[%u] music %d%s\n", frame, track & 0x7FFF, track & 0x8000 ? " once" : ""); }

uint8_t *plat_scroll_ring(void) { return fbmem[back] + 0x200; }
uint8_t *plat_scroll_panel(void) { return fbmem[back] + 0x12E00; }
int plat_back_index(void) { return back; }
void plat_wait_flip(void) { }
void plat_border_colour(int index) { border = index & 255; }
/* game time: HB_MS per frame (default 66: a 15 fps console) */
uint32_t plat_ticks_ms(void) { return ticks; }

uint32_t plat_prof_us(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)(t.tv_sec * 1000000ull + t.tv_nsec / 1000);
}

uint16_t plat_read_pad(void)
{
	const char *s = env("HB_PAD", "");
	uint16_t state = 0;
	while (*s)
	{
		char *colon;
		const unsigned long at = strtoul(s, &colon, 10);
		if (*colon != ':') break;
		const char *v = colon + 1;
		const char *end = strchr(v, ',');
		if (!end) end = v + strlen(v);
		if (at <= frame)
		{
			uint16_t b = 0;
			for (const char *p = v; p < end; )
			{
				const char *q = p;
				while (q < end && *q != '+') q++;
				const size_t l = (size_t)(q - p);
				if (l == 1 && *p == 'A') b |= PAD_A;
				if (l == 1 && *p == 'B') b |= PAD_B;
				if (l == 1 && *p == 'C') b |= PAD_C;
				if (l == 1 && *p == 'X') b |= PAD_X;
				if (l == 1 && *p == 'Y') b |= PAD_Y;
				if (l == 1 && *p == 'Z') b |= PAD_Z;
				if (l == 5 && strncmp(p, "START", 5) == 0) b |= PAD_START;
				if (l == 2 && strncmp(p, "UP", 2) == 0) b |= PAD_UP;
				if (l == 4 && strncmp(p, "DOWN", 4) == 0) b |= PAD_DOWN;
				if (l == 4 && strncmp(p, "LEFT", 4) == 0) b |= PAD_LEFT;
				if (l == 5 && strncmp(p, "RIGHT", 5) == 0) b |= PAD_RIGHT;
				p = q < end ? q + 1 : q;
			}
			state = b;
		}
		s = *end ? end + 1 : end;
	}
	return state;
}

/* what the 32X shows: 224 lines through the line table of the buffer just finished */
static void save_frame(const uint8_t *buf, int shift)
{
	char path[1024];
	snprintf(path, sizeof path, "%s/frame_%05u.ppm", env("HB_OUT", "."), frame);
	FILE *f = fopen(path, "wb");
	if (!f) return;
	fprintf(f, "P6\n320 224\n255\n");
	for (int line = 0; line < 224; line++)
	{
		const uint32_t word = (uint32_t)buf[2 * line] << 8 | buf[2 * line + 1];
		const uint8_t *px = buf + word * 2 + shift;
		for (int x = 0; x < 320; x++)
		{
			const uint16_t c = shown_pal[px[x]];
			const uint8_t rgb[3] = { (uint8_t)((c & 31) << 3), (uint8_t)((c >> 5 & 31) << 3), (uint8_t)((c >> 10 & 31) << 3) };
			fwrite(rgb, 1, 3, f);
		}
	}
	fclose(f);
}

/* port/plat_mars.c plat_present_scroll (J19b): 12 border lines, view lines from the ring
 * (384 per row, 200 rows), the panel below, the blank line beyond */
void plat_present_scroll(const uint16_t *palette555, int row0, int xoff, int viewH)
{
	uint8_t *fb = fbmem[back];
	for (int line = 0; line < 256; line++)
	{
		const int g = line - 12;
		uint32_t byte;
		if (g < 0 || g >= 200) byte = 0x16000;
		else if (g < viewH) byte = 0x200 + (uint32_t)((row0 + g) % 200) * 384u + (uint32_t)(xoff & ~1);
		else byte = 0x12E00 + (uint32_t)(g - viewH) * 320u;
		fb[2 * line] = (uint8_t)(byte >> 9);
		fb[2 * line + 1] = (uint8_t)(byte >> 1);
	}
	memset(fb + 0x16000, border, 320);
	memcpy(shown_pal, palette555, sizeof shown_pal);
	const unsigned every = (unsigned)atoi(env("HB_SAVE_EVERY", "10"));
	if (every && frame % every == 0) save_frame(fb, xoff & 1);
	frame++;
	ticks += (uint32_t)atoi(env("HB_MS", "66"));
	back ^= 1;
	if (frame >= (uint32_t)atoi(env("HB_FRAMES", "300"))) exit(0);
}

/* the 32X runs fn(arg, 1) on the slave SH2 while fn(arg, 0) runs on the master; here one after
 * the other in the same order of effects (part 0 sees the world before part 1's steps) */
void plat_parallel(void (*fn)(void *, int), void *arg) { fn(arg, 0); fn(arg, 1); }
