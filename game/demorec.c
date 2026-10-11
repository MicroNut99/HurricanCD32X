/*
 * Hurrican CD32X - PC tool: the recorded demos (game/demo_data.h).
 *
 * The demos are the player's buttons, one entry per 60 Hz step (run-length coded). On the console
 * the demo runs the world exactly as here (p1.c: in a demo the camera and the entities' wake-up
 * are done every step, the random numbers start from the demo's seed and the drawing does not
 * use them), so the same buttons give the same game on the PC and on the 32X.
 *
 * This tool finds the buttons: it follows a route of way points (taken from a recording of the
 * original game), trying a set of button patterns at every decision point and looking ahead with
 * the game's own world step (fork() keeps the world as it is - every try runs in a copy).
 *
 *   make -C host demorec
 *   HB_DIR=<packs> HB_MAPIMG=out.ppm host/hurrican_demorec          the stage's map as a picture
 *   HB_DIR=<packs> HB_ROUTE="x,y x,y ..." HB_SECS=75 [HB_WARP=x,y] [HB_SEED=n] HB_DEMO_OUT=d.txt \
 *       host/hurrican_demorec                                         one demo, written as text
 *   HB_DIR=<packs> HB_PLAY=d.txt host/hurrican_demorec              replays it, prints the path
 */
#define main game_main
#include "p1.c"
#undef main

#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

static void map_image(const char *path)
{
	const int W = map_w * 10, H = map_h * 10;
	uint8_t *img = calloc((size_t)W * H, 3);
	static uint8_t ringmem[RING_LIN + 4096];
	ring = ringmem;
	for (int by = 0; by < H; by += 200)
		for (int bx = 0; bx < W; bx += 320)
		{
			const int x1 = bx + 320 < W ? bx + 320 : W, y1 = by + 200 < H ? by + 200 : H;
			compose_clean(bx, by, x1, y1);
			for (int y = by; y < y1; y++)
				for (int x = bx; x < x1; x++)
				{
					const uint8_t v = CLEAN[lin_of(x, y)];
					const uint16_t c = palette[v];
					uint8_t *o = img + 3 * ((size_t)y * W + x);
					o[0] = (uint8_t)((c & 31) << 3); o[1] = (uint8_t)((c >> 5 & 31) << 3); o[2] = (uint8_t)((c >> 10 & 31) << 3);
				}
		}
	/* walls that can be shot away: magenta tint; deadly: red tint; air vents: cyan dots */
	for (int ty = 0; ty < map_h; ty++)
		for (int tx = 0; tx < map_w; tx++)
		{
			const int f = blk(tx, ty);
			if (!(f & (B_DESTROY | B_DAMAGE))) continue;
			for (int y = ty * 10; y < ty * 10 + 10; y++)
				for (int x = tx * 10; x < tx * 10 + 10; x++)
				{
					uint8_t *o = img + 3 * ((size_t)y * W + x);
					if (f & B_DESTROY) { o[0] = (uint8_t)((o[0] + 255) / 2); o[2] = (uint8_t)((o[2] + 255) / 2); }
					else { o[0] = 255; }
				}
		}
	/* things to find: gems yellow, the secret green, the level end red, power blocks blue, start white */
	for (int i = 0; i < nents; i++)
	{
		const Ent *e = &ents[i];
		uint8_t r = 0, g = 0, b = 0;
		switch (e->type)
		{
		case E_DIAMANT: r = 255; g = 230; break;
		case E_SECRET: g = 255; break;
		case E_ENDLEVEL: r = 255; break;
		case E_POWERBLOCK: b = 255; g = 128; break;
		case E_VENT: g = 255; b = 255; break;
		case E_PLATTFORM: case E_BRUECKE: r = 255; g = 255; b = 255; break;
		default: continue;
		}
		const int cx = (e->x >> 9) + 5, cy = (e->y >> 9) + 5;
		for (int dy = -3; dy <= 3; dy++)
			for (int dx = -3; dx <= 3; dx++)
			{
				const int x = cx + dx, y = cy + dy;
				if (x < 0 || y < 0 || x >= W || y >= H) continue;
				uint8_t *o = img + 3 * ((size_t)y * W + x);
				o[0] = r; o[1] = g; o[2] = b;
			}
	}
	FILE *f = fopen(path, "wb");
	fprintf(f, "P6\n%d %d\n255\n", W, H);
	fwrite(img, 3, (size_t)W * H, f);
	fclose(f);
	printf("map %d x %d world pixels (picture at half size), start %d,%d\n", map_w * 20, map_h * 20, (int)(pl.x >> 8), (int)(pl.y >> 8));
	for (int i = 0; i < nents; i++)
		if (ents[i].type == E_SECRET || ents[i].type == E_ENDLEVEL)
			printf("%s at %d,%d\n", ents[i].type == E_SECRET ? "secret" : "level end", (int)(ents[i].x >> 8), (int)(ents[i].y >> 8));
}

/* ------------------------------------------------------------------ the world, as a recorded demo runs it */
static void rec_begin(int wx, int wy, uint32_t seed)
{
	memset(&pl, 0, sizeof pl);
	pl.lives = 99;
	demo = 1; demo_rec = -1;
	demo_warp = wx || wy; demo_warp_x = wx; demo_warp_y = wy;
	rng_state = seed;
	stage_go(stage);
	demo_warp = 0;
	for (int t = 0; t < 121; t++) { camera_follow(0); world_activate(); }   /* GS_INTRO's steps */
}

static int crush_trig = -1;           /* a crusher set off (its entity), since cleared */
static uint8_t crush_prev[MAX_ENTS];
static void rec_step(int p)
{
	pad = p;
	world_step();
	camera_follow(0);
	world_activate();
	for (int i = 0; i < nents; i++)
		if (ents[i].type == E_STAMPFSTEIN)
		{
			if (ents[i].state == 1 && crush_prev[i] == 0) crush_trig = i;
			crush_prev[i] = ents[i].state;
		}
	if (pl.state == PS_DEAD && pl.dead_t > 90)
	{
		pl.lives--;
		player_spawn(pl.safe_x, pl.safe_y - FX(10));
		pl.safe_t = 150;
	}
}

/* ------------------------------------------------------------------ the search */
#define K      8                      /* steps a choice is held */
#define NMAC   8                      /* left / none / right, each without and with the jump; short steps */
#define DEPTH  3
#define ROLL   50
typedef struct { int x, y, r; int mv, dir, h, walk; } Way;   /* mv: a move found by HB_PATH */
static Way way[64];
static int nway, wi;
static int lives0;

static int mac_pad(int m, int k, int extra)
{
	/* 0-5: left / none / right, without and with the jump, for all K steps;
	 * 6, 7: a short step (3 steps) right / left, then standing */
	if (m >= 6)
	{
		int p = PAD_B | extra;
		if (k < 3) p |= m == 6 ? PAD_RIGHT : PAD_LEFT;
		demo_aim(&p);
		return p;
	}
	const int h = m % 3, j = m / 3;
	int p = PAD_B | extra;
	if (h == 0) p |= PAD_LEFT;
	if (h == 2) p |= PAD_RIGHT;
	if (j) p |= PAD_C;
	demo_aim(&p);
	return p;
}

static int pcxw(void) { return (pl.x >> 8) + 35; }
static int pcyw(void) { return (pl.y >> 8) + 60; }
static int at_way(const Way *w);
static int dist_to(const Way *w)
{
	const int dx = pcxw() - w->x, dy = pcyw() - w->y;
	return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

/* a way point is reached within r pixels; r < 0: standing there (not in a jump) within -r */
static int at_way(const Way *w)
{
	if (w->r < 0) return pl.state != PS_JUMP && dist_to(w) < -w->r;
	return dist_to(w) < w->r;
}

/* how far from the end of the route: to the way point aimed at, then along the rest */
static int rest_len[64];
static int potential(int wj) { return dist_to(&way[wj]) + rest_len[wj]; }

static double try_seq(const int *seq, int extra, int roll_hop)
{
	/* the way points reached on the way count (and the next one is then aimed at) */
	int wj = wi;
	int t = 0;
#define REACHED() do { while (wj + 1 < nway && at_way(&way[wj])) wj++; } while (0)
	double early = 0;                 /* progress made sooner counts more (no putting it off) */
	for (int d = 0; d < DEPTH; d++)
	{
		for (int k = 0; k < K; k++, t++)
		{
			rec_step(mac_pad(seq[d], k, d == 0 && k == 0 ? extra : 0));
			if (pl.lives < lives0 || pl.state == PS_DEAD) return -1e9 + t;
			REACHED();
		}
		early += potential(wj);
	}
	const double dseq = potential(wj);
	/* then on towards the way point (a jump when blocked, in one of the two tries) */
	int lastx = pcxw(), still = 0, hop = 0;
	for (int k = 0; k < ROLL; k++, t++)
	{
		const int dx = way[wj].x - pcxw();
		still = pcxw() == lastx ? still + 1 : 0;
		lastx = pcxw();
		if (roll_hop && still >= 3 && hop == 0 && (dx > 10 || dx < -10)) hop = 20;
		int p = PAD_B | (dx > 10 ? PAD_RIGHT : dx < -10 ? PAD_LEFT : 0);
		if (hop > 4) p |= PAD_C;
		if (hop > 0) hop--;
		rec_step(p);
		if (pl.lives < lives0 || pl.state == PS_DEAD) return -1e8 + t - dseq;
		REACHED();
	}
#undef REACHED
	/* a crusher coming down over him (it would hit him just after the look-ahead) */
	for (int i = 0; i < nents; i++)
		if (ents[i].type == E_STAMPFSTEIN && ents[i].state == 1)
		{
			const int ex = ents[i].x >> 8, px = pl.x >> 8;
			if (px + player_box.r > ex - 10 && px + player_box.l < ex + 90) return -1e7 + t;
		}
	double sc = -0.6 * dseq - potential(wj) - 0.2 * early;
	if (pl.state == PS_JUMP) sc -= 5;
	for (int d = 0; d < DEPTH; d++) if (seq[d] >= 3 && seq[d] < 6) sc -= 3;   /* no needless hops */
	return sc;
}

static uint16_t pads[60 * 300];
static int np;
/* a move of the path search, after a shift of n steps (right for n > 0, left for n < 0) */
/* a crusher in the way: set it off from beside it, step back, and wait until it is pulled up
 * far enough to walk under it (it falls again only from the top) */
static int crusher_dance(int i)
{
	Ent *e = &ents[i];
	const int cx0 = (e->x >> 8) + 40;
	const int side = pcxw() < cx0 ? -1 : 1;
	const int S = cx0 + side * 70;
	for (int k = 0; k < 240 && (pcxw() - S > 3 || pcxw() - S < -3); k++)
	{
		const int b = PAD_B | (pcxw() < S ? PAD_RIGHT : PAD_LEFT);
		rec_step(b); pads[np++] = (uint16_t)b;
	}
	int k;
	for (k = 0; k < 40 && e->state == 0; k++)
	{
		const int b = PAD_B | (side < 0 ? PAD_RIGHT : PAD_LEFT);
		rec_step(b); pads[np++] = (uint16_t)b;
	}
	if (e->state == 0) return 0;
	for (k = 0; k < 14; k++)
	{
		const int b = PAD_B | (side < 0 ? PAD_LEFT : PAD_RIGHT);
		rec_step(b); pads[np++] = (uint16_t)b;
	}
	for (k = 0; k < 700; k++)
	{
		if (e->state == 3 && (e->y >> 8) + 244 < (pl.y >> 8) + player_box.t - 10) break;
		int b = PAD_B;
		demo_aim(&b);
		b &= ~(PAD_LEFT | PAD_RIGHT);
		rec_step(b); pads[np++] = (uint16_t)b;
	}
	return pl.state != PS_DEAD && pl.lives >= lives0 && e->state == 3;
}

static int do_move(const Way *w, int n, int h, int pre)
{
	for (int k = 0; k < (n < 0 ? -n : n); k++) { const int b = PAD_B | (n > 0 ? PAD_RIGHT : PAD_LEFT); rec_step(b); pads[np++] = (uint16_t)b; }
	if (n) for (int k = 0; k < 2; k++) { rec_step(PAD_B); pads[np++] = PAD_B; }
	int left_ground = 0;
	for (int t = 0; t < 400; t++)
	{
		int b = PAD_B;
		if (w->dir < 0) b |= PAD_LEFT;
		if (w->dir > 0) b |= PAD_RIGHT;
		if (h > 0 && t >= pre && t < pre + h) b |= PAD_C;
		if (h == 0 && t >= pre) b &= ~(PAD_LEFT | PAD_RIGHT);
		rec_step(b); pads[np++] = (uint16_t)b;
		if (pl.lives < lives0 || pl.state == PS_DEAD) return 0;
		if (pl.state == PS_JUMP) left_ground = 1;
		if (pl.state != PS_JUMP && (h == 0 ? t >= pre + 2 : left_ground && t > pre + h)) break;
	}
	return 1;
}

int rec_main(void)
{
	const char *route = getenv("HB_ROUTE");
	int wx = 0, wy = 0;
	if (getenv("HB_WARP")) sscanf(getenv("HB_WARP"), "%d,%d", &wx, &wy);
	const uint32_t seed = getenv("HB_SEED") ? (uint32_t)strtoul(getenv("HB_SEED"), 0, 0) : 0x48555252u;
	const int max_steps = (getenv("HB_SECS") ? atoi(getenv("HB_SECS")) : 75) * 60;
	const int wpn_every = getenv("HB_WPN") ? atoi(getenv("HB_WPN")) * 60 : 0;
	/* the route: "x,y[,r] x,y[,r] ..." */
	for (const char *q = route; q && *q && nway < 64; )
	{
		while (*q == ' ') q++;
		if (!*q) break;
		Way w = { 0, 0, 40, 0, 0, 0, 0 };
		int n = 0;
		if (q[0] == 'w' && q[1] == ':')           /* w:n - stand and shoot for n steps */
		{
			sscanf(q + 2, "%d%n", &w.h, &n);
			q += 2 + n;
			w.mv = 2;
			way[nway++] = w;
			continue;
		}
		if (q[0] == 'm' && q[1] == ':')           /* m:dir,hold,walk,x,y - a move, landing at x,y */
		{
			sscanf(q + 2, "%d,%d,%d,%d,%d%n", &w.dir, &w.h, &w.walk, &w.x, &w.y, &n);
			q += 2 + n;
			w.mv = 1; w.r = -30;
			way[nway++] = w;
			continue;
		}
		sscanf(q, "%d,%d%n", &w.x, &w.y, &n);
		q += n;
		if (*q == ',') { q++; sscanf(q, "%d%n", &w.r, &n); q += n; }
		way[nway++] = w;
	}
	for (int k = 1; k < nway; k++) if (way[k].mv == 2) { way[k].x = way[k - 1].x; way[k].y = way[k - 1].y; way[k].r = 1000; }
	for (int k = nway - 1; k >= 0; k--)
	{
		if (k == nway - 1) { rest_len[k] = 0; continue; }
		const int dx = way[k + 1].x - way[k].x, dy = way[k + 1].y - way[k].y;
		rest_len[k] = rest_len[k + 1] + (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
	}
	rec_begin(wx, wy, seed);
	int tail = -1;
	double *score = mmap(0, sizeof(double) * 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	int prev = 2;
	while (np < max_steps)
	{
		if (wi >= nway) { if (tail < 0) tail = 120; }
		if (tail >= 0)
		{
			if (tail-- == 0) break;
			const int p = PAD_B;
			rec_step(p); pads[np++] = (uint16_t)p;
			continue;
		}
		lives0 = pl.lives;
		if (way[wi].mv == 2)
		{
			/* a pause: shooting at what is about (aimed up at what is above) */
			for (int k = 0; k < way[wi].h; k++)
			{
				int p = PAD_B;
				demo_aim(&p);
				p &= ~(PAD_LEFT | PAD_RIGHT);
				rec_step(p); pads[np++] = (uint16_t)p;
			}
			wi++;
			continue;
		}
		if (way[wi].mv == 1 && !getenv("HB_NOMOVES"))
		{
			/* a move of the path search: tried in a copy first; when it lands where it should,
			 * done for real, else the way point is reached by searching */
			const Way *w = &way[wi];
			static const int holds[7] = { 0, 4, 8, 14, 22, 32, 45 };
			const int h = holds[w->h];
			const int pre = w->h == 0 ? (w->walk == 0 ? 4 : w->walk == 1 ? 12 : 30) : w->walk * 8;
			/* tried as it is, then after a few steps' shift left or right (the path search
			 * started from an exact place; with the enemies about he may stand a little off) */
			static const int shifts[13] = { 0, 1, -1, 2, -2, 3, -3, 4, -4, 6, -6, 8, -8 };
			static int *trig;
			if (!trig) trig = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			int ok = 0, shift = 0, danced = 0;
			for (int v = 0; v < 13 && !ok; v++)
			{
				*trig = -1;
				const pid_t c = fork();
				if (c > 0)
				{
					int st = 0; waitpid(c, &st, 0); ok = WIFEXITED(st) && WEXITSTATUS(st) == 7; shift = shifts[v];
					if (!ok && *trig >= 0 && danced < 3)
					{
						/* it set off a crusher: first the crusher, then the move again */
						const int ci = *trig;
						const pid_t c2 = fork();
						if (c2 == 0) _exit(crusher_dance(ci) ? 7 : 1);
						int st2 = 0; waitpid(c2, &st2, 0);
						danced++;
						if (WIFEXITED(st2) && WEXITSTATUS(st2) == 7)
						{
							crusher_dance(ci);
							fprintf(stderr, "step %d: crusher %d passed\n", np, ci);
							v = -1;
						}
					}
					continue;
				}
				crush_trig = -1;
				const int good = do_move(w, shifts[v], h, pre) && at_way(w);
				for (int k = 0; good && k < 60; k++) rec_step(PAD_B);
				const int alive = good && pl.lives >= lives0 && pl.state != PS_DEAD;
				if (!alive && crush_trig >= 0) *trig = crush_trig;
				_exit(alive ? 7 : 1);
			}
			if (ok)
			{
				do_move(w, shift, h, pre);
				fprintf(stderr, "step %d: move %d (%d,%d,%d) to %d,%d done (shift %d)\n", np, wi, w->dir, w->h, w->walk, w->x, w->y, shift);
			}
			if (ok) { wi++; continue; }
			fprintf(stderr, "step %d: move %d to %d,%d by search\n", np, wi, w->x, w->y);
			way[wi].mv = 0;
		}
		/* a way point not reached for 6 s: left out (the next one is aimed at) */
		{
			static int last_wi = -1, since = 0;
			if (wi != last_wi) { last_wi = wi; since = np; }
			else if (np - since > 360)
			{
				/* stuck: the demo ends where it last got on (a second after), nothing of the trying */
				fprintf(stderr, "step %d: way point %d not reached - the demo ends at step %d\n", np, wi, since + 60);
				np = since + 60;
				rec_begin(wx, wy, seed);
				for (int i = 0; i < np; i++) rec_step(pads[i]);
				break;
			}
		}
		const int extra = wpn_every && np > 0 && np % wpn_every < K ? PAD_X : 0;
		int nseq = 1;
		for (int d = 0; d < DEPTH; d++) nseq *= NMAC;
		int running = 0;
		for (int i = 0; i < 2 * nseq; i++)
		{
			if (running >= 4) { wait(0); running--; }
			const pid_t c = fork();
			if (c == 0)
			{
				int seq[DEPTH], v = i % nseq;
				for (int d = 0; d < DEPTH; d++) { seq[d] = v % NMAC; v /= NMAC; }
				score[i] = try_seq(seq, extra, i >= nseq);
				_exit(0);
			}
			running++;
		}
		while (running > 0) { wait(0); running--; }
		for (int i = 0; i < nseq; i++) if (score[i + nseq] > score[i]) score[i] = score[i + nseq];
		int best = 0;
		for (int i = 1; i < nseq; i++)
		{
			const double a = score[i] + ((i % NMAC) == prev ? 1 : 0), b = score[best] + ((best % NMAC) == prev ? 1 : 0);
			if (a > b) best = i;
		}
		const int m = best % NMAC;
		if (getenv("HB_RECDBG"))
		{
			fprintf(stderr, "np %d at %d,%d state %d xs %d: best seq %d", np, pcxw(), pcyw(), pl.state, (int)pl.xs, best);
			for (int i = 0; i < NMAC; i++) fprintf(stderr, " m%d=%.0f", i, score[i]);
			fprintf(stderr, "\n");
		}
		prev = m;
		for (int k = 0; k < K; k++)
		{
			const int p = mac_pad(m, k, k == 0 ? extra : 0);
			rec_step(p);
			pads[np++] = (uint16_t)p;
			if (at_way(&way[wi]))
			{
				fprintf(stderr, "step %d: way point %d (%d,%d) reached\n", np, wi, way[wi].x, way[wi].y);
				wi++;
				break;
			}
		}
		if (np % 300 < K)
			fprintf(stderr, "step %d: at %d,%d lives %d, way point %d (%d,%d) dist %d, best %.0f\n", np, pcxw(), pcyw(), pl.lives,
			        wi, way[wi < nway ? wi : nway - 1].x, way[wi < nway ? wi : nway - 1].y, wi < nway ? dist_to(&way[wi]) : 0, score[best]);
		if (pl.lives < 99) { fprintf(stderr, "LOST A LIFE at step %d\n", np); }
	}
	fprintf(stderr, "done: %d steps (%.1f s), way points %d of %d, lives %d\n", np, np / 60.0, wi, nway, pl.lives);
	/* the result: "warp x y seed s" then the pads */
	FILE *f = fopen(getenv("HB_DEMO_OUT") ? getenv("HB_DEMO_OUT") : "demo.txt", "w");
	fprintf(f, "%d %d %d %u %d\n", stage, wx, wy, seed, np);
	for (int i = 0; i < np; i++) fprintf(f, "%d\n", pads[i]);
	fclose(f);
	return wi >= nway ? 0 : 2;
}

/* replays a demo text file the way p1.c will; prints the path, writes the places every 60 steps */
static int play_main(const char *path)
{
	FILE *f = fopen(path, "r");
	int st, wx, wy, np; unsigned seed;
	if (!f || fscanf(f, "%d %d %d %u %d", &st, &wx, &wy, &seed, &np) != 5) { fprintf(stderr, "bad %s\n", path); return 1; }
	stage = st;
	rec_begin(wx, wy, seed);
	for (int i = 0; i < np; i++)
	{
		int p = 0;
		if (fscanf(f, "%d", &p) != 1) break;
		rec_step(p);
		if ((i + 1) % 60 == 0) printf("%d %d\n", (int)(pl.x >> 8), (int)(pl.y >> 8));
	}
	fprintf(stderr, "replayed %d steps: at %d,%d lives %d\n", np, (int)(pl.x >> 8), (int)(pl.y >> 8), pl.lives);
	fclose(f);
	return 0;
}

/* ------------------------------------------------------------------ a route by search (HB_PATH)
 * As game/reach.c: the places the player can stand, found with the game's own movement (walk,
 * walk off, jumps held for different times, to the left, up and right), the rope bridges and the
 * air vents working, enemies left out; the walls that can be shot away stay. Then the way from
 * the start to the place nearest the goal, printed as way points for HB_ROUTE. */
#define PCELLX 10
#define PCELLY 6
typedef struct { int32_t x, y; int32_t parent; int8_t dir, h, walk; } PNode;
static PNode *pq;
static uint8_t *pseen;
static int pseen_w, pseen_h;
static int pmark(int32_t x, int32_t y)
{
	const int cx = (x >> 8) / PCELLX, cy = (y >> 8) / PCELLY;
	if (cx < 0 || cy < 0 || cx >= pseen_w || cy >= pseen_h) return 0;
	const uint32_t bit = (uint32_t)cy * (uint32_t)pseen_w + (uint32_t)cx;
	if (pseen[bit >> 3] & (1u << (bit & 7))) return 0;
	pseen[bit >> 3] |= (uint8_t)(1u << (bit & 7));
	return 1;
}
static void path_world_step(void)
{
	for (int i = 0; i < nents; i++)
	{
		Ent *e = &ents[i];
		if (e->type == E_BRUECKE) ent_platform(e, i);
		else if (e->type == E_VENT) VENT_step(e, i, e->x >> 8, e->y >> 8, 0, 0, 0, 0, 0);
	}
}
static int path_main(void)
{
	int sx, sy, tx, ty;
	if (sscanf(getenv("HB_PATH"), "%d,%d %d,%d", &sx, &sy, &tx, &ty) != 4) return 1;
	pseen_w = map_w * 20 / PCELLX + 2; pseen_h = map_h * 20 / PCELLY + 2;
	pseen = calloc((size_t)pseen_w * pseen_h / 8 + 1, 1);
	const int QMAX = 600000;
	pq = malloc(sizeof(PNode) * QMAX);
	demo = 1;
	player_spawn(FX(sx), FX(sy));
	pad = 0;
	for (int i = 0; i < 200; i++) { player_step(); path_world_step(); }
	int qh = 0, qt = 0;
	pq[qt++] = (PNode){ pl.x, pl.y, -1, 0, 0, 0 };
	pmark(pl.x, pl.y);
	static const int holds[7] = { 0, 4, 8, 14, 22, 32, 45 };
	int best = 0, bestd = 1 << 30;
	while (qh < qt)
	{
		const int me = qh;
		const PNode s = pq[qh++];
		for (int dir = -1; dir <= 1; dir++)
			for (int h = 0; h < 7; h++)
				for (int walk = 0; walk < (h == 0 ? 3 : 3); walk++)
				{
					if (h == 0 && dir == 0) continue;
					player_spawn(s.x, s.y);
					pl.state = PS_STAND; pl.ys = 0; pl.jumpadd = 0; pl.on_ground = 1;
					pl.safe_x = s.x; pl.safe_y = s.y;
					pl.jump_ok = 1;
					int left_ground = 0;
					/* h = 0: walk 4 / 12 / 30 steps; h > 0: walk 0, 8 or 16 steps, then the jump held h steps */
					const int pre = h == 0 ? (walk == 0 ? 4 : walk == 1 ? 12 : 30) : walk * 8;
					for (int t = 0; t < 400; t++)
					{
						int b = 0;
						if (dir < 0) b |= PAD_LEFT;
						if (dir > 0) b |= PAD_RIGHT;
						if (h > 0 && t >= pre && t < pre + holds[h]) b |= PAD_C;
						if (h == 0 && t >= pre) b &= ~(PAD_LEFT | PAD_RIGHT);
						pad = b;
						player_step();
						path_world_step();
						if (pl.state == PS_DEAD || pl.hurt > 0) break;
						if (pl.state == PS_JUMP) left_ground = 1;
						const int still = pl.state != PS_JUMP && (h == 0 ? t >= pre + 2 : left_ground && t > pre + holds[h]);
						if (still)
						{
							if (pmark(pl.x, pl.y) && qt < QMAX)
							{
								pq[qt] = (PNode){ pl.x, pl.y, me, (int8_t)dir, (int8_t)h, (int8_t)walk };
								const int dx = (pl.x >> 8) + 35 - tx, dy = (pl.y >> 8) + 60 - ty;
								const int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
								if (d < bestd) { bestd = d; best = qt; }
								qt++;
							}
							break;
						}
					}
				}
	}
	/* the way back from the best place */
	static int chain[4096];
	int n = 0;
	for (int k = best; k >= 0 && n < 4096; k = pq[k].parent) chain[n++] = k;
	fprintf(stderr, "places %d; nearest to the goal: %d,%d (%d away), %d jumps/walks\n", qt, (pq[best].x >> 8) + 35, (pq[best].y >> 8) + 60, bestd, n - 1);
	/* way points: every place on the way (the recorder reaches them within 40) */
	for (int i = n - 2; i >= 0; i--)
	{
		const PNode *a = &pq[chain[i + 1]], *b = &pq[chain[i]];
		printf("%d,%d,-16 m:%d,%d,%d,%d,%d ", (a->x >> 8) + 35, (a->y >> 8) + 60, b->dir, b->h, b->walk, (b->x >> 8) + 35, (b->y >> 8) + 60);
	}
	printf("\n");
	return 0;
}

int main(void)
{
	plat_init();
	load_resident();
	stage = getenv("HB_STAGE") ? atoi(getenv("HB_STAGE")) : 1;
	load_stage(stage);
	stage_start();
	if (getenv("HB_MAPIMG")) { map_image(getenv("HB_MAPIMG")); return 0; }
	if (getenv("HB_PLAY")) return play_main(getenv("HB_PLAY"));
	if (getenv("HB_PATH")) return path_main();
	if (getenv("HB_TRY"))          /* PC check: HB_TRY=pad,steps;pad,steps... from HB_WARP */
	{
		int wx = 0, wy = 0;
		if (getenv("HB_WARP")) sscanf(getenv("HB_WARP"), "%d,%d", &wx, &wy);
		rec_begin(wx, wy, 1);
		const char *q = getenv("HB_TRY");
		int t = 0;
		while (*q)
		{
			int p, n, c = 0;
			if (sscanf(q, "%d,%d%n", &p, &n, &c) < 2) break;
			q += c; if (*q == ';') q++;
			for (int i = 0; i < n; i++, t++)
			{
				rec_step(p);
				if (t % 4 == 0) printf("t %d pad %d: %d,%d state %d ground %d xs %d\n", t, p, (int)(pl.x >> 8), (int)(pl.y >> 8), pl.state, pl.on_ground, (int)pl.xs);
			}
		}
		return 0;
	}
	if (getenv("HB_ENTS"))         /* the objects in an area: type, place, v1, v2 */
	{
		int x0, y0, x1, y1;
		sscanf(getenv("HB_ENTS"), "%d,%d,%d,%d", &x0, &y0, &x1, &y1);
		for (int i = 0; i < nents; i++)
		{
			const Ent *e = &ents[i];
			const int x = e->x >> 8, y = e->y >> 8;
			if (e->type != E_NONE && x >= x0 && x <= x1 && y >= y0 && y <= y1)
				printf("type %3d at %5d,%5d v1 %d v2 %d box %d,%d-%d,%d\n", e->type, x, y, e->v1, e->v2,
				       kinds[e->type].box.l, kinds[e->type].box.t, kinds[e->type].box.r, kinds[e->type].box.b);
		}
#define K2(name, id, ...) printf("%s=%d(map %d) ", #name, E_##name, id);
#include K2_LIST
#undef K2
		printf("\n");
		printf("E_VENT %d E_PLATTFORM %d E_BRUECKE %d E_POWERBLOCK %d E_DIAMANT %d\n", E_VENT, E_PLATTFORM, E_BRUECKE, E_POWERBLOCK, E_DIAMANT);
		return 0;
	}
	if (getenv("HB_BLK"))          /* the block flags of an area, one letter a tile: # wall, = platform, ~ liquid, ! deadly, x shoot away */
	{
		int x0, y0, x1, y1;
		sscanf(getenv("HB_BLK"), "%d,%d,%d,%d", &x0, &y0, &x1, &y1);
		for (int ty = y0 / 20; ty <= y1 / 20; ty++)
		{
			printf("%5d ", ty * 20);
			for (int tx = x0 / 20; tx <= x1 / 20; tx++)
			{
				const int f = blk(tx, ty);
				putchar(f & B_DESTROY ? 'x' : f & B_DAMAGE ? '!' : f & B_WALL ? '#' : f & B_PLATFORM ? '=' : f & B_LIQUID ? '~' : (f & (B_SLOPE_L | B_SLOPE_R)) ? '/' : '.');
			}
			putchar('\n');
		}
		printf("      x from %d, a letter = 20 px\n", x0 / 20 * 20);
		return 0;
	}
	if (getenv("HB_ROUTE")) return rec_main();
	return 0;
}
