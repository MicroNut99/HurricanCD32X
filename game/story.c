/*
 * OPTIONS - STORY: Hurrican's intro (Intro.cpp), included by p1.c.
 *
 * The pack OJ010.PAK (tools/hur_story.py) is read into the cart's stage area like a stage
 * pack (the loading screen shows meanwhile; the stage loaded last must then be read again).
 * Six pictures with their own colours, the story's 24 lines; the song intro.it is CD track 24
 * (played once). As Hurrican: a new line every 5.1 s (Counter += Timer.sync(25.5) to 1300),
 * typed out from the left over 2.7 s, four lines a picture (Background[(TextOff - 1) / 4]);
 * the newest line bright, the older ones darker, over the dark band at the bottom. Between two
 * pictures the colours fade out and in (Hurrican cross-fades). At the end (or any button) the
 * colours fade out and OPTIONS comes back.
 */
#define TRACK_STORY  24
#define STORY_PACK   10                  /* OJ010.PAK */
#define STORY_LINES_MAX 32
#define STORY_BAND   152                 /* the text's first line */

static const uint8_t *story_pic[6];
static const uint8_t *story_pal[6];
static const char *story_line[STORY_LINES_MAX];
static int story_nlines, story_off, story_count, story_horiz, story_shown, story_fade, story_end;
static uint16_t story_saved[256];
static int story_saved_dark;

/* returns 0 when the pack is not on the disc (then nothing changes) */
static int story_enter(void)
{
	plat_music(0);                                   /* no CD data reads while music plays */
	genbg_off();                                     /* the loading screen's black is see-through */
	stage_in_cart = 0;                               /* the stage area holds the story now */
	const int32_t len = plat_load_pack(PACK_LEVEL_ID0 + STORY_PACK, STAGE_CART, STAGE_MAX);
	if (len <= 0 || !pack_mount_at(1, plat_cart_ptr(STAGE_CART))) { music(TRACK_MENU); return 0; }
	char name[8] = "PIC0";
	uint32_t size;
	for (int k = 0; k < 6; k++)
	{
		name[0] = 'P'; name[1] = 'I'; name[2] = 'C'; name[3] = (char)('0' + k);
		if (!pack_file(name, &story_pic[k], &size)) story_pic[k] = 0;
		name[1] = 'A'; name[2] = 'L';
		if (!pack_file(name, &story_pal[k], &size)) story_pal[k] = 0;
	}
	const uint8_t *t;
	story_nlines = 0;
	if (pack_file("TEXT", &t, &size))
		for (uint32_t i = 0; i < size && story_nlines < STORY_LINES_MAX; )
		{
			story_line[story_nlines++] = (const char *)t + i;
			while (i < size && t[i]) i++;
			i++;
		}
	memcpy(story_saved, palette, sizeof story_saved);
	story_saved_dark = col_dark;
	story_off = 0; story_count = 0; story_horiz = 0; story_shown = 0; story_fade = 0; story_end = 0;
	if (opt_music) plat_music(TRACK_STORY | TRACK_ONCE);
	return 1;
}

static void story_leave(void)
{
	memcpy(palette, story_saved, sizeof story_saved);
	col_dark = story_saved_dark;
	clean_valid = 0;
	music(TRACK_MENU);
}

/* one 60 Hz step; returns 1 when the story is over */
static int story_step(void)
{
	if (story_end)
	{
		if (story_fade > 0) story_fade -= 2;
		return story_fade <= 0;
	}
	/* the picture for the shown lines: fade the old one out, swap, fade the new one in (all per
	   step - the console draws one frame every four steps, so a per-frame fade never got there) */
	int a = (story_off - 1) / 4;
	if (a < 0) a = 0;
	if (a > 5) a = 5;
	if (a != story_shown)
	{
		story_fade -= 2;
		if (story_fade <= 0) { story_fade = 0; story_shown = a; }
	}
	else if (story_fade < 32) story_fade++;
	story_count += 17;                               /* Timer.sync(25.5) x 4 a step */
	story_horiz += 17;
	if (story_count > 1300 * 4)
	{
		story_count = 0; story_horiz = 0;
		if (++story_off > story_nlines - 1) story_end = 1;
	}
	if (story_horiz > 1300 * 4) story_horiz = 0;
	return 0;
}

static void story_draw(void)
{
	cam_x = cam_y = view_x = view_y = 0; draw_cx = draw_cy = 0;
	const uint8_t *pic = story_pic[story_shown], *pal = story_pal[story_shown];
	/* the colours, faded */
	if (pal)
		for (int i = 0; i < 256; i++)
		{
			const int v = rd16(pal + 2 * i);
			const int r = (v & 31) * story_fade / 32, g = (v >> 5 & 31) * story_fade / 32, b = (v >> 10 & 31) * story_fade / 32;
			palette[i] = (uint16_t)((r | g << 5 | b << 10) ? (r | g << 5 | b << 10) : 0x0400);
		}
	/* the picture */
	for (int y = 0; y < VIEW_H; y++)
	{
		const uint32_t l = (uint32_t)y * RING_W;
		if (pic)
		{
			const uint8_t *s = pic + y * 320;
			for (int x = 0; x < 320; x += 2) PUT2(ring, l + x, s[x], s[x + 1]);
		}
		else for (int x = 0; x < 320; x += 2) PUT2(ring, l + x, 0, 0);
	}
	/* the text: the story's lines so far, cut at 39 letters, the last four shown lines */
	col_dark = 254;
	char shown[8][40];
	int colour[8], n = 0;
	for (int k = 0; k <= story_off && k < story_nlines; k++)
	{
		const char *s = story_line[k];
		int len = (int)strlen(s);
		if (k == story_off)                          /* typed out: Hurrican's xr up to 700 of 640 */
		{
			const int xr = story_horiz / 4 > 640 ? 640 : story_horiz / 4;
			len = len * xr / 640;
		}
		for (int p = 0; p < len; )
		{
			int c = len - p > 39 ? 39 : len - p;
			if (len - p > 39) { int b = c; while (b > 20 && s[p + b] != ' ') b--; if (b > 20) c = b; }
			if (n == 8) { memmove(shown[0], shown[1], 7 * 40); memmove(colour, colour + 1, 7 * sizeof colour[0]); n = 7; }
			memcpy(shown[n], s + p, (size_t)c);
			shown[n][c] = 0;
			colour[n] = k == story_off ? 251 : k == story_off - 1 ? 252 : 253;
			n++;
			p += c;
			while (p < len && s[p] == ' ') p++;
		}
	}
	const int first = n > 4 ? n - 4 : 0;
	for (int i = first; i < n; i++) hud_text(6, STORY_BAND + 11 * (i - first), shown[i], colour[i]);
	col_dark = story_saved_dark;
}
