/*
 * Hurrican CD32X - PC check: can the end of the stage be reached? A search over the places
 * the player can stand, moving him with the game's own step (player_step, the same tile tests
 * and jump physics as on the console): from each place, walk, walk off, and jump (held for
 * different times) to the left, straight up and to the right; every place he lands on is
 * searched from in turn. Walls that can be shot away count as gone; the rope bridges and
 * power blocks (found) can be stood on; enemies are left out.
 *   make -C host reach && HB_DIR=<packs> [HB_STAGE=n] host/hurrican_p1_reach
 * Prints the furthest places reached and whether a level-end object was touched.
 */
#define main game_main
#include "p1.c"
#undef main

#define CELLX 8
#define CELLY 4
static uint8_t seen[(13000 / CELLX + 8) * (5200 / CELLY + 8) / 8 + 1];
static int seen_w;
typedef struct { int32_t x, y; } Pos;
static Pos queue[400000];

static int mark(int32_t x, int32_t y)
{
	const int cx = (x >> 8) / CELLX, cy = (y >> 8) / CELLY;
	if (cx < 0 || cy < 0 || cx >= seen_w || (uint32_t)cy * seen_w + cx >= sizeof seen * 8) return 0;
	const uint32_t bit = (uint32_t)cy * (uint32_t)seen_w + (uint32_t)cx;
	if (seen[bit >> 3] & (1u << (bit & 7))) return 0;
	seen[bit >> 3] |= (uint8_t)(1u << (bit & 7));
	return 1;
}

static int touches_end(void)
{
	for (int i = 0; i < nents; i++)
		if (ents[i].type == E_ENDLEVEL &&
		    boxes_meet(pl.x >> 8, pl.y >> 8, &player_box, ents[i].x >> 8, ents[i].y >> 8, &kinds[E_ENDLEVEL].box))
			return 1;
	return 0;
}

/* stand-ins for the world: only the things to stand on */
static void platforms_step(void)
{
	for (int i = 0; i < nents; i++)
	{
		Ent *e = &ents[i];
		if (e->type == E_BRUECKE || (e->type == E_POWERBLOCK)) ent_platform(e, i);
	}
}

int main(void)
{
	plat_init();
	load_resident();
	stage = getenv("HB_STAGE") ? atoi(getenv("HB_STAGE")) : 1;
	load_stage(stage);
	pl.lives = 3; pl.weapon = 1;
	stage_start();
	for (int ty = 0; ty < map_h; ty++)
		for (int tx = 0; tx < map_w; tx++)
			if (blk(tx, ty) & B_DESTROY) wall_break(tx, ty);
	for (int i = 0; i < nents; i++) if (ents[i].type == E_POWERBLOCK) ents[i].state = 1;
	seen_w = 13000 / CELLX + 8;
	/* settle on the ground (HB_START=x,y: from there instead) */
	if (getenv("HB_START"))
	{
		int sx = 0, sy = 0;
		sscanf(getenv("HB_START"), "%d,%d", &sx, &sy);
		player_spawn(FX(sx), FX(sy));
	}
	if (getenv("HB_JUMP"))       /* trace a jump: HB_JUMP=dir (-1, 0, 1) */
	{
		const int d = atoi(getenv("HB_JUMP"));
		for (int i = 0; i < 200; i++) { pad = 0; player_step(); platforms_step(); }
		printf("start %d,%d safe %d,%d\n", (int)(pl.x >> 8), (int)(pl.y >> 8), (int)(pl.safe_x >> 8), (int)(pl.safe_y >> 8));
		for (int t = 0; t < 120; t++)
		{
			pad = PAD_C | (d < 0 ? PAD_LEFT : d > 0 ? PAD_RIGHT : 0);
			player_step(); platforms_step();
			if (t % 4 == 0) printf("t %d: %d,%d state %d ys %d add %d\n", t, (int)(pl.x >> 8), (int)(pl.y >> 8), pl.state, (int)pl.ys, (int)pl.jumpadd);
		}
		return 0;
	}
	if (getenv("HB_WALK"))       /* trace a walk: HB_WALK=-1 or 1 */
	{
		const int d = atoi(getenv("HB_WALK"));
		for (int i = 0; i < 200; i++) { pad = 0; player_step(); platforms_step(); }
		for (int t = 0; t < 600; t++)
		{
			pad = d < 0 ? PAD_LEFT : PAD_RIGHT;
			player_step(); platforms_step();
			if (t % 20 == 0) printf("t %d: %d,%d state %d liquid %d\n", t, (int)(pl.x >> 8), (int)(pl.y >> 8), pl.state, pl.in_liquid);
		}
		return 0;
	}
	pad = 0;
	for (int i = 0; i < 200; i++) { player_step(); platforms_step(); }
	int qh = 0, qt = 0;
	queue[qt++] = (Pos){ pl.x, pl.y };
	mark(pl.x, pl.y);
	int best_x = 0, end_found = 0;
	Pos best = { 0, 0 };
	static const int holds[6] = { 0, 4, 8, 14, 22, 40 };
	while (qh < qt)
	{
		const Pos s = queue[qh++];
		for (int dir = -1; dir <= 1; dir++)
			for (int h = 0; h < 6; h++)
				for (int walk = 0; walk < (h == 0 ? 3 : 2); walk++)
				{
					if (h == 0 && dir == 0) continue;
					player_spawn(s.x, s.y);
					pl.state = PS_STAND; pl.ys = 0; pl.jumpadd = 0; pl.on_ground = 1;
					pl.safe_x = s.x; pl.safe_y = s.y;
					pl.jump_ok = 1;
					int left_ground = 0;
					/* h = 0: walk 4 / 12 / 30 steps; h > 0: walk 0 or 8 steps, then jump held h steps */
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
						platforms_step();
						if (pl.state == PS_DEAD) break;
						if (touches_end()) end_found = 1;
						if (pl.state == PS_JUMP) left_ground = 1;
						const int still = pl.state != PS_JUMP && (h == 0 ? t >= pre + 2 : left_ground && t > pre + holds[h]);
						if (still)
						{
							if (mark(pl.x, pl.y) && qt < (int)(sizeof queue / sizeof queue[0]))
								queue[qt++] = (Pos){ pl.x, pl.y };
							if ((pl.x >> 8) > best_x) { best_x = pl.x >> 8; best = (Pos){ pl.x, pl.y }; }
							break;
						}
					}
				}
	}
	printf("places reached: %d; furthest right: %d,%d; level end touched: %s\n", qt, (int)(best.x >> 8), (int)(best.y >> 8),
	       end_found ? "YES" : "no");
	/* a coarse map of the places reached (one character per 200 x 160 pixels) */
	for (int ry = 0; ry < map_h * 20; ry += 160)
	{
		char line[100];
		int n = 0;
		for (int rx = 0; rx < map_w * 20 && n < 98; rx += 200)
		{
			int hit = 0;
			for (int i = 0; i < qt && !hit; i++)
			{
				const int x = queue[i].x >> 8, y = queue[i].y >> 8;
				if (x >= rx && x < rx + 200 && y >= ry && y < ry + 160) hit = 1;
			}
			int endc = 0;
			for (int i = 0; i < nents; i++)
				if (ents[i].type == E_ENDLEVEL && (ents[i].x >> 8) >= rx && (ents[i].x >> 8) < rx + 200 && (ents[i].y >> 8) >= ry && (ents[i].y >> 8) < ry + 160) endc = 1;
			line[n++] = endc ? 'E' : hit ? '#' : '.';
		}
		line[n] = 0;
		printf("%5d %s\n", ry, line);
	}
	return 0;
}
