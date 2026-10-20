/*
 * OPTIONS - CREDITS: the credits over a plasma, with a CD music player (included by p1.c).
 *
 * The plasma (after Sonic 1+1's credits screen, tails/saver.asm + SH2C/sonic32_master.c):
 *   pattern 0  OpenJazz's own credits plasma (src/menu/plasma.cpp: its sine table ojsin.h, its
 *              four moving phases, the 16 colours of MENU.000's main menu palette), redrawn every
 *              frame - GPL-2.0, as OpenJazz
 *   1-18       Sonic 1+1's colour-cycling patterns: drawn once (into CLEAN, the tile-layer copy,
 *              unused in the menus), copied to the screen every frame and animated by turning
 *              their 64 colours (palette entries 160-223, stage colours - unused in the menus)
 * The credits scroll up over it in Hurrican's own menu font (menufont.png, halved), with a
 * shadow. The player line at the bottom (the small font on a dark band).
 *
 * Buttons: Left / Right choose a track, A plays it, B stops it, C the next plasma, Y hides all
 * the text but the track's number and name (and shows it again), Up / Down
 * the plasma faster / slower (nothing is shown for it - Sonic 1+1's "SPEED" line is left out),
 * START back to OPTIONS.
 * No pause: the Sub-CPU program (boot path, unchanged) has play (CMD 45) and stop (CMD 54) only.
 *
 * The CD tracks (build.sh): 2 menu, 3 stage clear, 4 game over, 5-13 stages 1-9, 14 credits,
 * 15-23 the bosses of stages 1-9, 24 the story (intro.it);
 * the titles are the modules' own song names (the 26 bytes after "IMPM" in each .it file).
 */
#include "ojsin.h"

#define TRACK_CREDITS  14
#define CR_FIRST_TRACK 2
#define CR_LAST_TRACK  24
#define CR_PATTERNS    19            /* 0 = OpenJazz's own, 1-18 Sonic 1+1's */
#define CR_PAL         160           /* the plasma's palette entries 160-223 */
#define CR_TEXT_BOTTOM 160           /* the credits scroll above the player's lines */
#define CR_LINE_H      13            /* the credits' line height (the 11-line font + 2) */

static const char *const cr_track_title[CR_LAST_TRACK + 1] = {
	0, 0,
	"MAINMENU", "HURRICAN - STAGE CLEAR", "GAME OVER",
	"JUNGLE BEATS", "AZTECH - LONG FORGOTTEN...", "ELEVATOR ACTION", "CAVE-STAGE",
	"SPIDERFACTORY-STAGE", "POWER TOWER", "EIS-STAGE", "THE REAL CAVE",
	"LOST STATION",                  /* its module says "untitled": the file's name */
	"CREDITS",
	"ROBOFIST-BOSS", "PHARAO-BOSS", "ELEVATOR-BOSS", "BOSS GOLEM", "GIANT SPIDER-BOSS",
	"THE DRAGON", "ICE-BOSS", "ONSLAUGHT-BOSS", "BOSS SKELETOR/THE WALL", "THE BEGINNING" };
static const char *const cr_track_use[CR_LAST_TRACK + 1] = {
	0, 0, "MENU", "STAGE CLEAR", "GAME OVER", "STAGE 1", "STAGE 2", "STAGE 3", "STAGE 4",
	"STAGE 5", "STAGE 6", "STAGE 7", "STAGE 8", "STAGE 9", "CREDITS",
	"STAGE 1 BOSS", "STAGE 2 BOSS", "STAGE 3 BOSS", "STAGE 4 BOSS", "STAGE 5 BOSS", "STAGE 6 BOSS",
	"STAGE 7 BOSS", "STAGE 8 BOSS", "STAGE 9 BOSS", "STORY" };

static const char *const cr_lines[] = {
	"HURRICAN", "", "(C) 2007 POKE53280", "", "",
	"JOERG 'EISWUXE' WINTERSTEIN", "PROGRAMMING", "GAME DESIGN", "SOUND & MUSIC", "LEVEL DESIGN", "",
	"THOMAS 'TURRI' SCHREITER", "GRAPHICS", "GAME DESIGN", "ENEMY DESIGN", "LEVEL DESIGN", "", "",
	"SPECIAL THANKS", "T.R. SCHMIDT", "GERHARD 'SCHLAUCHI' WEIHRAUCH", "ADDITIONAL LEVEL DESIGN", "", "",
	"BETA TESTERS", "WENKMAN", "GEREON 'NTROPI' BARTEL", "KAI 'FLANDERZ' SCHMIEGELT",
	"MICHAEL 'RAM-BRAND' DAHMS", "CARSTEN 'JAZZOID' PRZYLUCZKY", "HEIKO 'THEWANDERER' KALISTA",
	"MATTHIAS 'KINGKIKAPU' KLOCKE", "MANUEL 'HEIMDALL' ROELKE", "FRIEDRICH 'BADDATA' VOLMERING",
	"CHRISTIAN 'GRYPHON' KLEINSTEINBERG", "MARC 'DAWN2069MS' SCHMITT", "", "",
	"SDL PORT", "PICKLE136", "STEFAN 'THRIMBOR' SCHMIDT", "LEANDRO 'DRFIEMOST' NINI", "", "", "",
	"SEGA CD32X EDITION", "", "",
	"CD32X BOOT", "FROM THE OPENLARA CD32X PORT", "",
	"D32XR 32X CODE", "VICTOR LUCHITS", "",
	"SEGA CD AND 32X FRAMEWORK", "CHILLY WILLY", "",
	"PLASMA", "OPENJAZZ", "",
	"SEGA CD32X PORT", "MICRONUT99", "", "", "",
	"THANK YOU FOR PLAYING!", 0 };

/* ------------------------------------------------------------------ the plasma */
static int8_t cr_sin[256];                       /* Sonic 1+1's sine from parabolas, -127 .. 127 */
static uint16_t cr_pal[CR_PATTERNS - 1][64];
static int cr_pat, cr_speed = 8, cr_drawn_pat = -1;
static int cr_p0, cr_p1, cr_p2, cr_p3;           /* OpenJazz's phases */
static uint32_t cr_acc, cr_rot;
static int cr_col[320];

static uint16_t cr_rgb(int r, int g, int b)
{
	const uint16_t v = (uint16_t)((r >> 3) | (g >> 3) << 5 | (b >> 3) << 10);
	return v ? v : 0x0400;                       /* 0x0000 is see-through on the 32X */
}
static int cr_abs(int v) { return v < 0 ? -v : v; }
static void cr_ramp(uint16_t *t, int r0, int g0, int b0, int r1, int g1, int b1)
{
	for (int i = 0; i < 64; i++)
	{
		const int k = i < 32 ? i * 8 : (64 - i) * 8;            /* 0 .. 256 .. 0 */
		t[i] = cr_rgb(r0 + (r1 - r0) * k / 256, g0 + (g1 - g0) * k / 256, b0 + (b1 - b0) * k / 256);
	}
}
static void cr_wheel(uint16_t *t, const uint8_t (*c)[3], int n)
{
	for (int i = 0; i < 64; i++)
	{
		const int pos = i * n * 256 / 64, k = pos >> 8, f = pos & 255;
		const uint8_t *a = c[k % n], *b = c[(k + 1) % n];
		t[i] = cr_rgb(a[0] + (b[0] - a[0]) * f / 256, a[1] + (b[1] - a[1]) * f / 256, a[2] + (b[2] - a[2]) * f / 256);
	}
}
static void cr_tables(void)
{
	static const uint8_t fire[][3] = { {40, 0, 0}, {200, 20, 0}, {255, 140, 0}, {255, 240, 120}, {255, 140, 0}, {200, 20, 0} };
	static const uint8_t ocean[][3] = { {0, 10, 60}, {0, 70, 180}, {0, 190, 230}, {200, 250, 255}, {0, 190, 230}, {0, 70, 180} };
	static const uint8_t aurora[][3] = { {0, 40, 40}, {0, 220, 120}, {0, 160, 220}, {150, 60, 255}, {255, 80, 200}, {0, 220, 120} };
	static const uint8_t candy[][3] = { {255, 120, 200}, {255, 240, 120}, {120, 230, 255}, {200, 150, 255}, {140, 255, 170} };
	static const uint8_t sunset[][3] = { {60, 0, 90}, {200, 0, 140}, {255, 90, 60}, {255, 190, 40}, {255, 90, 60}, {200, 0, 140} };
	for (int i = 0; i < 256; i++)
	{
		const int x = i & 127, v = x * (128 - x) * 127 / 4096;
		cr_sin[i] = (int8_t)(i < 128 ? v : -v);
	}
	cr_ramp(cr_pal[0], 0, 70, 255, 255, 150, 0);      /* rings: blue / orange */
	cr_ramp(cr_pal[1], 130, 0, 255, 255, 230, 0);     /* crossing waves: violet / yellow */
	cr_ramp(cr_pal[2], 220, 20, 40, 0, 230, 230);     /* vertical waves: red / cyan */
	cr_ramp(cr_pal[3], 20, 200, 40, 230, 0, 200);     /* horizontal waves: green / magenta */
	cr_ramp(cr_pal[4], 0, 170, 160, 255, 110, 80);    /* diamonds: teal / coral */
	cr_ramp(cr_pal[5], 150, 230, 0, 110, 0, 200);     /* checks: lime / purple */
	cr_ramp(cr_pal[6], 255, 190, 0, 40, 30, 160);     /* tunnel: gold / indigo */
	cr_ramp(cr_pal[7], 255, 120, 180, 0, 160, 90);    /* X: pink / green */
	cr_ramp(cr_pal[8], 0, 120, 255, 255, 200, 60);    /* two ripples: azure / amber */
	cr_ramp(cr_pal[9], 255, 60, 0, 0, 150, 255);      /* zigzag: orange-red / sky blue */
	cr_ramp(cr_pal[10], 100, 0, 160, 220, 255, 60);   /* cells: plum / chartreuse */
	cr_ramp(cr_pal[11], 0, 90, 60, 255, 140, 170);    /* moire: pine / rose */
	for (int i = 0; i < 64; i++)                      /* the colour wheel */
	{
		const int pos = i * 24, seg = pos >> 8, f = pos & 255;
		int r, g, b;
		switch (seg)
		{
		case 0: r = 255; g = f; b = 0; break;
		case 1: r = 255 - f; g = 255; b = 0; break;
		case 2: r = 0; g = 255; b = f; break;
		case 3: r = 0; g = 255 - f; b = 255; break;
		case 4: r = f; g = 0; b = 255; break;
		default: r = 255; g = 0; b = 255 - f; break;
		}
		cr_pal[12][i] = cr_rgb(r, g, b);
	}
	cr_wheel(cr_pal[13], fire, 6);
	cr_wheel(cr_pal[14], ocean, 6);
	cr_wheel(cr_pal[15], aurora, 6);
	cr_wheel(cr_pal[16], candy, 5);
	cr_wheel(cr_pal[17], sunset, 6);
}
/* Sonic 1+1's patterns (its plasma_value), pattern 1-18 here = its 0-17 */
static int cr_value(int pat, int x, int y)
{
	const int dx = x - 160, dy = y - 100;
	int d2;
	switch (pat)
	{
	case 0: return 2 * cr_sin[((dx * dx + dy * dy) >> 6) & 255] + cr_sin[(x * 2) & 255] + cr_sin[(y * 2 + x) & 255];
	case 1: return cr_sin[(x * 3 + y) & 255] + cr_sin[(x - y * 2) & 255] + cr_sin[(x + y * 3) & 255] + cr_sin[(y * 4) & 255];
	case 2: return 3 * cr_sin[(x * 4 + (cr_sin[(y * 2) & 255] >> 1)) & 255];
	case 3: return 3 * cr_sin[(y * 4 + (cr_sin[(x * 2) & 255] >> 1)) & 255];
	case 4: return 3 * cr_sin[((cr_abs(dx) + cr_abs(dy)) * 3) & 255];
	case 5: return 2 * cr_sin[(x * 5) & 255] + 2 * cr_sin[(y * 5) & 255];
	case 6: d2 = cr_abs(dx) + cr_abs(dy) + 8; return 3 * cr_sin[(8192 / d2 + x / 8) & 255];
	case 7: return 3 * cr_sin[((dx * dy) >> 7) & 255];
	case 8: return 2 * cr_sin[(((dx + 70) * (dx + 70) + dy * dy) >> 7) & 255]
	             + 2 * cr_sin[(((dx - 70) * (dx - 70) + dy * dy) >> 7) & 255];
	case 9: return 3 * cr_sin[(x * 2 + cr_abs(((y * 2) & 127) - 64) * 2) & 255];
	case 10: return cr_sin[(x * 5) & 255] + cr_sin[(y * 5) & 255] + 2 * cr_sin[((x + y) * 3) & 255];
	case 11: return 2 * cr_sin[((dx * dx + dy * dy) >> 5) & 255] + 2 * cr_sin[(x * 3) & 255];
	default: return cr_sin[(x * 2) & 255] + cr_sin[(y * 3) & 255] + cr_sin[(x + y * 2) & 255] + cr_sin[((x * x + y * y) >> 7) & 255];
	}
}
/* a colour-cycling pattern into CLEAN (once, when chosen) */
static void cr_draw_pattern(int pat)
{
	for (int y = 0; y < VIEW_H; y++)
		for (int x = 0; x < 320; x++)
			CLEAN[y * RING_W + x] = (uint8_t)(CR_PAL + ((((cr_value(pat, x, y) + 512) * 64) >> 10) & 63));
	cr_drawn_pat = pat;
}
/* the screen from CLEAN (pattern 1-18) */
static void cr_copy_pattern(void)
{
	for (int y = 0; y < VIEW_H; y++)
	{
		const uint8_t *s = CLEAN + y * RING_W;
		const uint32_t l = (uint32_t)y * RING_W;
		for (int x = 0; x < 320; x += 2) PUT2(ring, l + x, s[x], s[x + 1]);
	}
}
/* OpenJazz's own plasma, straight onto the screen (it moves: redrawn every frame) */
#define CR_OJCOS(a) ((int)ojSin[((a) + 256) & 1023])
static void cr_draw_oj(void)
{
	int t1 = cr_p0, t2 = cr_p1, t3 = cr_p2, t4 = cr_p3;
	for (int x = 0; x < 320; x++, t3 += 3, t4 += 2)
		cr_col[x] = CR_OJCOS(t3 * 4) * 8 + CR_OJCOS(t4 * 4) * 8;
	for (int y = 0; y < VIEW_H; y++, t1 += 2, t2 += 1)
	{
		const int colb = CR_OJCOS(t1 * 4) * 8 + CR_OJCOS(t2 * 4) * 8 + (32 << 10);
		const uint32_t l = (uint32_t)y * RING_W;
		for (int x = 0; x < 320; x += 2)
			PUT2(ring, l + x, CR_PAL + (((colb + cr_col[x]) >> 10) & 15), CR_PAL + (((colb + cr_col[x + 1]) >> 10) & 15));
	}
}
/* time: OpenJazz's phases (70 % of its pace, as Sonic 1+1) and the colour turn, by the speed */
static void cr_tick(int frames)
{
	static const uint8_t oj_pal[16][3] = {
		{0, 0, 56}, {4, 0, 60}, {8, 0, 65}, {12, 0, 69}, {16, 0, 73}, {20, 0, 77}, {24, 0, 81}, {28, 0, 85},
		{32, 0, 89}, {28, 0, 85}, {24, 0, 81}, {20, 0, 77}, {16, 0, 73}, {12, 0, 69}, {8, 0, 65}, {4, 0, 60} };
	if (cr_pat == 0)
	{
		cr_acc += (uint32_t)frames * 303u * (uint32_t)cr_speed / 8u;
		for (int n = (int)(cr_acc >> 8); n > 0; n--)
		{
			cr_p0 = cr_p0 < 256 ? cr_p0 + 1 : 1;
			cr_p1 = cr_p1 < 256 ? cr_p1 + 2 : 2;
			cr_p2 = cr_p2 < 256 ? cr_p2 + 3 : 3;
			cr_p3 = cr_p3 < 256 ? cr_p3 + 4 : 4;
		}
		cr_acc &= 255;
		for (int i = 0; i < 16; i++) palette[CR_PAL + i] = cr_rgb(oj_pal[i][0], oj_pal[i][1], oj_pal[i][2]);
	}
	else
	{
		cr_rot += 77u * (uint32_t)cr_speed / 8u * (uint32_t)frames;
		for (int i = 0; i < 64; i++) palette[CR_PAL + i] = cr_pal[cr_pat - 1][(i + (cr_rot >> 9)) & 63];
	}
}

/* ------------------------------------------------------------------ the credits text */
static Pic cr_font;
static int cr_hide;                     /* Y: the text hidden but the track's number and name */
static int cr_scroll;                    /* 8.8: how far the text has scrolled up */
static int cr_text_h;
/* one line of the menu font, centred, at screen row y (clipped to the text area); shadow then letters */
static int cr_glyph_w(int c)
{
	if (c == ' ' || c < 33 || c - 33 >= cr_font.count) return 5;
	Frame f;
	frame_of(&cr_font, c - 33, &f);
	return f.x0 + f.w + 1;
}
static void cr_glyph(int c, int sx, int sy, int colour)
{
	if (c < 33 || c - 33 >= cr_font.count) return;
	Frame f;
	frame_of(&cr_font, c - 33, &f);
	if (!f.data) return;
	for (int r = 0; r < f.h; r++)
	{
		const int y = sy + f.y0 + r;
		if (y < 0 || y >= CR_TEXT_BOTTOM) continue;
		const uint8_t *row = f.data + rd32(f.data + 4 * r);
		const int runs = rd16(row);
		row += 2;
		int x = 0;
		memset(rowbuf, 0, (size_t)f.w + 2);
		for (int k = 0; k < runs; k++)
		{
			x += rd16(row);
			const int n = rd16(row + 2);
			for (int i = 0; i < n && x + i < (int)sizeof rowbuf; i++) rowbuf[x + i] = colour >= 0 ? (uint8_t)colour : row[4 + i];
			x += n;
			row += 4 + ((n + 1) & ~1);
		}
		const int x0 = sx + f.x0;
		if (x0 < 0 || x0 + f.w > SCREEN_W) continue;
		fb_put(lin_of(x0, y), rowbuf, f.w, 1);
	}
}
static void cr_line(const char *t, int y)
{
	int w = 0;
	for (const char *p = t; *p; p++) w += cr_glyph_w(*p);
	int x = (SCREEN_W - w) / 2;
	for (const char *p = t; *p; p++)
	{
		cr_glyph(*p, x + 1, y + 1, col_dark);        /* the shadow */
		cr_glyph(*p, x, y, -1);
		x += cr_glyph_w(*p);
	}
}

/* ------------------------------------------------------------------ the player */
static int cr_track = TRACK_CREDITS, cr_playing;

static void credits_enter(void)
{
	static int made;
	if (!made) { cr_tables(); made = 1; }
	cr_font = pic("creditfont.png");          /* the menu font at 11 lines, hard edges (tools/hur_convert.py) */
	if (!cr_font.count) cr_font = pic("menufont.png");
	cr_hide = 0;
	cr_scroll = 0;
	cr_text_h = 0;
	for (int i = 0; cr_lines[i]; i++) cr_text_h += CR_LINE_H;
	cr_pat = 0;
	cr_drawn_pat = -1;
	cr_track = TRACK_CREDITS;
	cr_playing = opt_music ? TRACK_CREDITS : 0;
	if (cr_playing) plat_music(TRACK_CREDITS);
	else plat_music(0);
	clean_valid = 0;                             /* CLEAN holds plasma now: the stage recomposes it */
}

/* one frame of input; returns 1 to go back to OPTIONS */
static int credits_input(int hit)
{
	if (hit & PAD_START) return 1;
	if (hit & PAD_LEFT) { cr_track = cr_track > CR_FIRST_TRACK ? cr_track - 1 : CR_LAST_TRACK; }
	if (hit & PAD_RIGHT) { cr_track = cr_track < CR_LAST_TRACK ? cr_track + 1 : CR_FIRST_TRACK; }
	if (hit & PAD_A) { plat_music(cr_track); cr_playing = cr_track; }
	if (hit & PAD_B) { plat_music(0); cr_playing = 0; }
	if (hit & PAD_C) cr_pat = (cr_pat + 1) % CR_PATTERNS;
	if (hit & PAD_Y) cr_hide ^= 1;               /* Y: only the track's number and name */
	if ((hit & PAD_UP) && cr_speed < 16) cr_speed++;
	if ((hit & PAD_DOWN) && cr_speed > 1) cr_speed--;
	return 0;
}

static void credits_draw(int frames)
{
	cam_x = cam_y = view_x = view_y = 0; draw_cx = draw_cy = 0;
	cr_tick(frames);
	if (cr_pat == 0) cr_draw_oj();
	else
	{
		if (cr_drawn_pat != cr_pat - 1) cr_draw_pattern(cr_pat - 1);
		cr_copy_pattern();
	}
	/* the credits: scroll up from the band, over and over */
	cr_scroll += frames * 51;                    /* 12 pixels a second (0.2 a 60 Hz step) */
	const int top = CR_TEXT_BOTTOM - (cr_scroll >> 8);
	if (top + cr_text_h < -16) cr_scroll = 0;
	if (!cr_hide)
		for (int i = 0, y = top; cr_lines[i]; i++, y += CR_LINE_H)
			if (y > -CR_LINE_H && y < CR_TEXT_BOTTOM && cr_lines[i][0]) cr_line(cr_lines[i], y);
	/* the player band */
	/* the player's lines straight over the plasma (shadowed text, no band) */
	char t[48], *q;
	q = put_str(t, "< ");
	q = put_dec(q, (uint32_t)cr_track, 2);
	q = put_str(q, " >  ");
	put_str(q, cr_track_title[cr_track]);
	hud_text(6, CR_TEXT_BOTTOM + 5, t, col_yellow);
	q = put_str(t, cr_track_use[cr_track]);
	q = put_str(q, "  -  ");
	if (cr_playing == cr_track) put_str(q, "PLAYING");
	else if (cr_playing) { q = put_str(q, "TRACK "); q = put_dec(q, (uint32_t)cr_playing, 2); put_str(q, " PLAYING"); }
	else put_str(q, "STOPPED");
	if (cr_hide) return;
	hud_text(6, CR_TEXT_BOTTOM + 16, t, col_light);
	hud_text(6, CR_TEXT_BOTTOM + 27, "A:PLAY B:STOP C:FX Y:TEXT START:BACK", col_light);
}
