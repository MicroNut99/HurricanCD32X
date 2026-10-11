/*
 * Group b4: the bosses of stage 8 (the caves) and stage 9 (the end) - Hurrican's
 * bosses/Boss_SchmidtOrgie.cpp (128 GEGNERSTURM), Boss_MetalHead.cpp (129), Boss_Skeletor.cpp
 * (138), Boss_Skull.cpp (the skull, spawned by Skeletor and the wall) and Boss_TheWall.cpp (139,
 * the final boss), with the shots they need that the engine's plain shots cannot do (helper
 * kinds: the metal head's fire bomb, Skeletor's grenade, the homing rocket / golem orb, the
 * wall's electric shot, its electric floor and its big laser).
 *
 * Timers are kept in 60 Hz steps (Hurrican's tenths x 6); angles in degrees (1/16 degree where
 * Hurrican adds fractions each frame). One boss runs at a time: their state is kept in a static
 * struct each. Left out everywhere: particles, glows, smoke, screen shaking, colour tints, the
 * rotation of pictures (drawn upright), the slow motion of the end, the music fades (the stage
 * music goes on until the boss music starts, music_stage() comes back after the boss).
 */

static Pic b4_kiefer, b4_kiefer2, b4_turbine, b4_hals, b4_flame, b4_fireball, b4_skflame;
static Pic b4_such2, b4_spider2, b4_tuer, b4_kringel2, b4_hirni, b4_rahmen, b4_vorne;
static int b4_col_laser, b4_col_core;    /* the laser's magenta and its pale core */

/* ---------------------------------------------------------------- helpers */

#ifndef OJ32X_MARS
/* PC check: HB_B4=1 prints the boss's state every second */
static void b4_trace(const char *name, const Ent *e, int a, int b)
{
	static uint32_t n;
	if (getenv("HB_B4") && n++ % 60 == 0)
		fprintf(stderr, "B4 %s: state %d energy %d at %d,%d (%d %d) cam %d,%d locked %d\n", name, e->state, e->energy,
		        (int)(e->x >> 8), (int)(e->y >> 8), a, b, cam_xoff(), cam_yoff(), boss_locked_there());
}
/* PC check: HB_B4_HP=n starts the bosses with n energy (to get to their ends quickly) */
static int b4_hp(int v) { const char *h = getenv("HB_B4_HP"); return h ? atoi(h) : v; }
#else
#define b4_trace(name, e, a, b) ((void)0)
#define b4_hp(v) (v)
#endif

/* a picture of one bank by name (16 letters) and, if fw, its frame width (half size): the packs
 * hold "metalhead_kiefer.png" and "metalhead_kiefer2.png" under the same 16 letters, and the
 * stage's "metalhead_turbine2.png" (the glow) before the resident "metalhead_turbine.png" */
static Pic b4_pic_in(const Bank *b, const char *name, int fw)
{
	Pic p = { 0, 0, 0, 0, 0 };
	for (int i = 0; i < b->ntex; i++)
	{
		const uint8_t *t = b->tex + 28 * i;
		if (strncmp((const char *)t, name, 16) == 0 && (fw == 0 || rd16(t + 22) == fw))
		{
			p.bank = b;
			p.first = (int)rd32(t + 16);
			p.count = rd16(t + 20);
			p.fw = rd16(t + 22);
			p.fh = rd16(t + 24);
			return p;
		}
	}
	return p;
}

/* atan(n / d) in whole degrees, -90..90 (Hurrican's atanf, not atan2) */
static int b4_atan(int n, int d)
{
	if (d == 0) return n < 0 ? -90 : 90;
	const int an = n < 0 ? -n : n, ad = d < 0 ? -d : d;
	int r;
	if (an <= ad)
	{
		const int t = an * 256 / ad;
		r = (t * 45 + t * (256 - t) * 156 / 2560) / 256;
	}
	else
	{
		const int t = ad * 256 / an;
		r = 90 - (t * 45 + t * (256 - t) * 156 / 2560) / 256;
	}
	return (n < 0) != (d < 0) ? -r : r;
}

/* GegnerClass::Run without walls, for kinds that move themselves (step returns 1) */
static void b4_move(Ent *e)
{
	e->xo = e->x; e->yo = e->y;
	e->xs += SYNC(e->xa);
	e->ys += SYNC(e->ya);
	e->x += SYNC(e->xs);
	e->y += SYNC(e->ys);
	if (e->hit) e->hit--;
}

/* ScrolltoPlayeAfterBoss: the screen back to the player, the stage music again */
static void b4_after_boss(void)
{
	boss_unlock();
	music_stage();
}

/* an enemy of another group spawned as Hurrican's PushGegner does: its own init, active at once,
 * facing the player (what world_activate does for the map's enemies) */
static Ent *b4_spawn(int type, int x, int y, int v1, int v2)
{
	Ent *s = ent_new(type, x, y);
	if (!s) return 0;
	s->v1 = (int16_t)v1;
	s->v2 = (int16_t)v2;
	switch (type)
	{
	case E_FLEDERMAUS: FLEDERMAUS_init(s); break;
	case E_SCHLEIMBOLLER: SCHLEIMBOLLER_init(s); break;
	case E_STACHELBEERE: STACHELBEERE_init(s); break;
	case E_FIESERWALKER2: FIESERWALKER2_init(s); break;
	case E_AUGE: s->xs = FX(12); s->ya = FX(4); break;
	case E_ONEUP: s->ya = FX(2); break;
	default: break;
	}
	s->active = 1;
	s->dir = pcx() > ecx(s);
	if (!s->dir) { s->xs = (int16_t)-s->xs; s->xa = (int16_t)-s->xa; }
	return s;
}

/* a flame (flame.png) as a shot, top-left (x, y): FEUERFALLE / WALKERFIRE */
static void b4_flame_shot(int x, int y, fx xs, fx ys, int dmg, int life)
{
	Shot *s = enemy_shot_pic(&b4_flame, x, y, xs, ys, 0, dmg, 1);
	if (s) s->life = (int16_t)life;
}

/* the resident colour (1-96, the same in every stage) nearest to r, g, b (0-31) */
static int b4_colour(int r, int g, int b)
{
	int best = 1, bd = 1 << 30;
	for (int i = 1; i < 97; i++)
	{
		const int c = palette[i];
		const int dr = (c & 31) - r, dg = (c >> 5 & 31) - g, db = (c >> 10 & 31) - b;
		const int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
		if (d < bd) { bd = d; best = i; }
	}
	return best;
}

/* a filled box in the world (full-size pixels), clipped to the screen */
static void b4_box_world(int wx, int wy, int w, int h, int colour)
{
	int sx = (wx >> 1) - cam_x, sy = (wy >> 1) - cam_y, sw = w >> 1, sh = h >> 1;
	if (sx < 0) { sw += sx; sx = 0; }
	if (sy < HUD_H) { sh -= HUD_H - sy; sy = HUD_H; }
	if (sx + sw > SCREEN_W) sw = SCREEN_W - sx;
	if (sy + sh > VIEW_H) sh = VIEW_H - sy;
	if (sw > 0 && sh > 0) hud_box(sx, sy, sw, sh, colour);
}

/* draw_frame with a left / right clip (full-size x0 .. x1): Hurrican's SetRect on the doors */
static void b4_draw_clip(const Pic *p, int frame, int32_t fx_, int32_t fy_, int x0, int x1)
{
	if (!p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	if (!f.data) return;
	const int bx = (int)(fx_ >> 9) + f.x0, by = (int)(fy_ >> 9) + f.y0;
	int vx0 = view_x, vx1 = view_x + VIEW_W;
	if (vx0 < x0 >> 1) vx0 = x0 >> 1;
	if (vx1 > x1 >> 1) vx1 = x1 >> 1;
	const int vy0 = view_y, vy1 = view_y + PLAY_H;
	if (vx0 >= vx1 || bx >= vx1 || by >= vy1 || bx + f.w <= vx0 || by + f.h <= vy0) return;
	const int r0 = by < vy0 ? vy0 - by : 0;
	const int r1 = by + f.h > vy1 ? vy1 - by : f.h;
	for (int r = r0; r < r1; r++)
	{
		const uint8_t *q = f.data + rd32(f.data + 4 * r);
		int runs = rd16(q);
		q += 2;
		int col = 0;
		while (runs--)
		{
			const int skip = rd16(q), len = rd16(q + 2);
			const uint8_t *s = q + 4;
			q = s + len + (len & 1);
			col += skip;
			int a = bx + col, e = a + len;
			col += len;
			if (e <= vx0 || a >= vx1) continue;
			if (a < vx0) { s += vx0 - a; a = vx0; }
			if (e > vx1) e = vx1;
			fb_put(lin_of(a, by + r), s, e - a, 0);
			px_sprites += (uint32_t)(e - a);
		}
	}
	const int a = bx < vx0 ? vx0 : bx, e = bx + f.w > vx1 ? vx1 : bx + f.w;
	rect_add(a, by, e, by + f.h);
	if (over_list_n < MAX_RECTS) over_list[over_list_n++] = (Rect){ (int16_t)a, (int16_t)by, (int16_t)e, (int16_t)(by + f.h) };
}

/* ================================================================ 128 GEGNERSTURM
 * T.R. Schmidt's enemy storm in the caves (Boss_SchmidtOrgie.cpp): an invisible boss. When the
 * player comes near (within 600, not left of it by more than 80) the screen locks on the arena
 * (value1, value2) and an autofire extra drops; once there the boss music plays and enemies hop
 * in from both sides: every (1.2 + energy / 600) tenths at 1.5 a tenth a bat or a small slime
 * ball, each costing the storm 10 of its 4500; at every 1000 a pair of eyes, at every 500 a
 * spiked ball, a big slime ball or a fire walker. At 100 it is over: a one-up drops and the
 * screen goes back to the player. The bar shows the storm's energy; it cannot be shot.
 * A blinking WARNING (Hurrican's Trigger_Warning: 3 x 12.5 tenths) opens the storm.
 * For the frame rate a spawn waits while 8 of the storm's enemies are still alive (Hurrican has
 * no limit; the storm keeps its length in enemies, not in time).
 * state 0 waiting, 1 locking, 2 storm, 3 over; t1 the spawn counter, t2 the warning. */
#define B4_STORM 8
static int16_t b4_storm[B4_STORM];      /* the storm's enemies: index + 1 */
static uint8_t b4_storm_t[B4_STORM];    /* and their kinds */

static void b4_storm_add(const Ent *s)
{
	if (!s) return;
	for (int k = 0; k < B4_STORM; k++)
		if (!b4_storm[k] || ents[b4_storm[k] - 1].type != b4_storm_t[k])
		{
			b4_storm[k] = (int16_t)(s - ents + 1);
			b4_storm_t[k] = s->type;
			return;
		}
}
static int b4_storm_room(void)
{
	for (int k = 0; k < B4_STORM; k++)
		if (!b4_storm[k] || ents[b4_storm[k] - 1].type != b4_storm_t[k]) return 1;
	return 0;
}

static void GEGNERSTURM_load(void)
{
	/* the group's extra pictures */
	b4_kiefer = b4_pic_in(&bank_stage, "metalhead_kiefer.png", 51);
	b4_kiefer2 = b4_pic_in(&bank_stage, "metalhead_kiefer2.png", 65);
	b4_turbine = b4_pic_in(&bank_res, "metalhead_turbine.png", 0);
	b4_hals = pic("metalhead_halsteil.png");
	b4_flame = pic("flame.png");
	b4_fireball = pic("fireball.png");
	b4_skflame = pic("skeletor_flame.png");
	b4_such2 = pic("suchschuss2.png");
	b4_spider2 = pic("spidershot2.png");
	b4_tuer = pic("endbosstuer.png");
	b4_kringel2 = pic("endbosskringel2.png");
	b4_hirni = pic("endbosshirni.png");
	b4_rahmen = pic("endbossrahmen.png");
	b4_vorne = pic("endbossmaschine_vorne.png");
	b4_col_laser = b4_colour(30, 0, 31);
	b4_col_core = b4_colour(31, 26, 31);
}
static void GEGNERSTURM_init(Ent *e)
{
	e->state = 0;
	e->t1 = 4;                                   /* ActionCounter 1.0 */
	e->energy = (int16_t)b4_hp(4500);
	memset(b4_storm, 0, sizeof b4_storm);
}
static int GEGNERSTURM_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int V1 = e->v1, V2 = e->v2;
	b4_trace("STURM", e, e->t1, e->t2);
	switch (e->state)
	{
	case 0:                                      /* waiting for the player */
		if (player_distance(e) < 600 && (pl.x >> 8) + 80 > x) e->state = 1;
		break;
	case 1:                                      /* the screen goes to the arena */
		if (!boss_lock_on)
		{
			boss_lock(V1, V2, 10);
			spawn_extra(x, y - 40, 10);          /* autofire */
		}
		if (boss_locked_there())
		{
			music_boss();
			e->state = 2;
			e->t2 = 225;
		}
		break;
	case 2:                                      /* the storm */
		boss_bar(e->energy, 4500);
		if (e->t2 > 0) e->t2--;
		if (e->t1 > 0) { e->t1--; break; }
		if (!b4_storm_room()) break;
		{
			const int sx = V1 + (rnd(2) ? 620 : -40);
			e->t1 = (int16_t)(5 + e->energy / 150);
			if (rnd(2) == 0) { if (bats_awake() < MAX_BATS) b4_storm_add(b4_spawn(E_FLEDERMAUS, sx, V2 + 50 + rnd(200), 0, 1)); }
			else b4_storm_add(b4_spawn(E_SCHLEIMBOLLER, sx, V2 + 200 + rnd(50), 24, 0));
			e->energy -= 10;
			if (e->energy % 1000 == 0)
			{
				b4_storm_add(b4_spawn(E_AUGE, V1 - 20, V2 + 130, rnd(10), 0));
				b4_storm_add(b4_spawn(E_AUGE, V1 + 620, V2 + 200, rnd(10), 0));
			}
			if (e->energy % 500 == 0)
			{
				switch (rnd(3))
				{
				case 0: b4_storm_add(b4_spawn(E_STACHELBEERE, sx, V2 + 100, 0, 0)); break;
				case 1: b4_storm_add(b4_spawn(E_SCHLEIMBOLLER, sx, V2 + 100, 46, 0)); break;
				default: b4_storm_add(b4_spawn(E_FIESERWALKER2, sx, V2 + 250, 0, 0)); break;
				}
			}
			if (e->energy <= 100) e->state = 3;
		}
		break;
	default:                                     /* over */
		e->energy = 0;
		break;
	}
	return 1;
}
static int GEGNERSTURM_explode(Ent *e)
{
	b4_spawn(E_ONEUP, e->x >> 8, (e->y >> 8) - 250, 10, 0);
	b4_after_boss();
	return 1;
}
static int GEGNERSTURM_draw(const Ent *e, uint32_t t)
{
	if (e->state == 2 && e->t2 > 0 && e->t2 % 75 > 25) hud_text(SCREEN_W / 2 - 28, 70, "WARNING", col_red);
	return 1;
}

/* ================================================================ 129 METALHEAD
 * The robot skull on its long neck in the caves (Boss_MetalHead.cpp). The screen locks on
 * (value1, value2); once there the boss music plays and the head rises out of the scrap heap
 * (after its eye has lit up, 25 tenths), opens its jaw and turbine and fires: one big fire ball
 * (30) 4-7 times, aimed (the gun turns -20..50 degrees towards the player); a fan of three fire
 * balls (15) 15 times while it sinks to the left; a fire bomb (10) that bursts into flames.
 * Then it closes and picks another of these (or, below 2000, crashes down onto the floor, a
 * slime ball dropping from the ceiling) - never the same twice. The player under it (left of
 * its right side) makes it crash down at once (500 if he is still under it). Only the open
 * turbine can be hurt; below 2000 the turbine is shot off, the eye is the target from then on
 * and it spits flames (and plant shots) instead. Touching the skull costs 4 a tenth.
 * At 100 the head flies off and bursts, the 24 vertebrae blow off one by one, 12500 points,
 * the screen goes back to the player.
 * Drawn: the jaw (upper), 9 of the 24 vertebrae (upright, not turned), the head, the turbine,
 * the lower jaw. Left out: the eye's corona, the turbine's glow, the smoke and sparks. */
enum { MH_NOTVISIBLE, MH_INIT, MH_EINFLIEGEN, MH_SCHIESSEN, MH_CRUSHEN, MH_CRUSHEN2, MH_ERHOLEN, MH_EXPLODIEREN };
enum { AKT_OEFFNEN, AKT_SCHIESSEN, AKT_SCHLIESSEN };
static struct {
	int16_t akt, art, cnt, delay;      /* Akt, ShotArt, ShotCount, ShotDelay (steps) */
	int16_t sc, gun;                   /* SinCount, GunWinkel: 1/16 degrees */
	int16_t eye, ende, count;          /* Eye_Alpha (x 6); exploding: vertebrae left, steps to the next */
	int16_t hx, hy;                    /* the head where it blew up (the neck stays) */
	uint8_t moving, turbine;
	int32_t kiefer, toff;              /* KieferPos, TurbineOff: 8.8 pixels */
	int32_t nx, ny;                    /* MoveToNewPoint (8.8) */
} b4_mh;

static void b4_mh_moveto(Ent *e, int x, int y, int state)
{
	b4_mh.moving = 1;
	b4_mh.nx = FX(x);
	b4_mh.ny = FX(y);
	e->xs = e->ys = 0;
	if (e->x < b4_mh.nx) e->xs = FX(10);
	if (e->x > b4_mh.nx) e->xs = FX(-10);
	if (e->y < b4_mh.ny) e->ys = FX(10);
	if (e->y > b4_mh.ny) e->ys = FX(-10);
	e->state = (uint8_t)state;
}

/* the vertebra i (0..24) of the neck from the screen's left edge to the head at (hx, hy) */
static void b4_mh_neck(const Ent *e, int hx, int hy, int i, int *nx, int *ny)
{
	const int dx = hx - (e->v1 + 40), dy = hy - (e->v2 + 250);
	*nx = e->v1 - 40 + i * dx / 24;
	*ny = e->v2 + 400 + dy / 2 - cos_d(i * 180 / 25) * dy / 512;
}

static void METALHEAD_load(void) { }
static void METALHEAD_init(Ent *e)
{
	memset(&b4_mh, 0, sizeof b4_mh);
	b4_mh.turbine = 1;
	e->state = MH_NOTVISIBLE;
}
static int METALHEAD_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int V1 = e->v1, V2 = e->v2;
	const int st = e->state;
	b4_trace("METALHEAD", e, b4_mh.akt * 10 + b4_mh.art, b4_mh.turbine * 10 + kinds[E_METALHEAD].shootable);
	if (!b4_mh.turbine) kinds[E_METALHEAD].box = (Box){ 140, 90, 180, 130 };

	/* the player under the head: it crashes down */
	if (st != MH_ERHOLEN && st != MH_EINFLIEGEN && st != MH_EXPLODIEREN && st != MH_NOTVISIBLE &&
	    st != MH_CRUSHEN && st != MH_CRUSHEN2 && y < V2 + 230 && pcx() < x + 200)
	{
		e->ys = FX(50);
		e->ya = FX(10);
		e->state = MH_CRUSHEN;
		b4_mh.moving = 0;
		sfx(SFX_LAND);
	}
	if (e->state != MH_NOTVISIBLE && e->state != MH_EXPLODIEREN) boss_bar(e->energy, 4000);
	if (!boss_lock_on && e->state != MH_EXPLODIEREN) boss_lock(V1, V2, 10);

	/* the turbine shot off */
	if (e->hit && b4_mh.turbine && e->energy < 2000)
	{
		b4_mh.turbine = 0;
		fx_add(&P_explbig, x + 85 + (int)(b4_mh.toff >> 8) + 49, y + 226, 2);
		fx_add(&P_expl, x + 60, y + 140, 2);
		sfx(SFX_BIGEXPLODE);
	}

	/* no energy left: the head flies off */
	if (e->energy <= 100 && e->state != MH_EXPLODIEREN)
	{
		kinds[E_METALHEAD].shootable = 0;
		e->energy = 100;
		e->state = MH_EXPLODIEREN;
		b4_mh.ende = 24;
		b4_mh.count = 48;
		b4_mh.hx = (int16_t)x; b4_mh.hy = (int16_t)y;
		e->xs = FX(20); e->ys = FX(-30); e->xa = 0; e->ya = FX(5);
		b4_mh.moving = 0;
		fx_add(&P_explbig, x + 60, y + 100, 2);
		fx_add(&P_explbig, x + 20, y + 140, 3);
		sfx(SFX_BIGEXPLODE);
	}

	if (b4_mh.moving)
	{
		/* DoMove: slows down once past the point */
		const fx d = SYNC(FX(1));
		if (e->xs < 0 && b4_mh.nx > e->x) { e->xs += d; if (e->xs > 0) e->xs = 0; }
		if (e->xs > 0 && b4_mh.nx < e->x) { e->xs -= d; if (e->xs < 0) e->xs = 0; }
		if (e->ys < 0 && b4_mh.ny > e->y) { e->ys += d; if (e->ys > 0) e->ys = 0; }
		if (e->ys > 0 && b4_mh.ny < e->y) { e->ys -= d; if (e->ys < 0) e->ys = 0; }
		if (e->xs == 0 && e->ys == 0) b4_mh.moving = 0;
	}
	else switch (e->state)
	{
	case MH_CRUSHEN:
	case MH_CRUSHEN2:
		b4_mh.toff = 0;
		if (b4_mh.gun > 0) b4_mh.gun -= 133;
		if (b4_mh.gun < 0) b4_mh.gun += 133;
		if (b4_mh.kiefer > 0) b4_mh.kiefer -= SYNC(FX(50));
		else b4_mh.kiefer = 0;
		if (y > V2 + 200)
		{
			e->xs = e->ys = e->xa = 0;
			e->ya = FX(-1);
			sfx(SFX_BIGEXPLODE);
			if (e->state == MH_CRUSHEN2)
			{
				Ent *s = b4_spawn(E_SCHLEIMBOLLER, V1 + 50 + rnd(550), V2 - 40, 40, 0);   /* size 40: medium */
				(void)s;
			}
			e->state = MH_ERHOLEN;
			fx_add(&P_smoke, x + 40, y + 210, 3);
			fx_add(&P_smoke, x + 140, y + 210, 3);
			if (pcx() < x + 200) player_damage(DMG(500));
		}
		break;

	case MH_ERHOLEN:                             /* back up */
		if (e->ys < 0 && y < V2 + 50)
		{
			e->ys = 0; e->ya = 0;
			b4_mh_moveto(e, V1 + 130, V2, MH_SCHIESSEN);
			b4_mh.art = 1;
		}
		break;

	case MH_NOTVISIBLE:
		e->state = MH_INIT;
		break;

	case MH_INIT:                                /* wait for the screen */
		if (boss_locked_there())
		{
			music_boss();
			e->state = MH_EINFLIEGEN;
		}
		break;

	case MH_EINFLIEGEN:                          /* the eye lights up, the head rises */
		e->energy = (int16_t)b4_hp(4000);
		if (++b4_mh.eye > 153)
		{
			b4_mh_moveto(e, V1 + 70, V2 + 130, MH_SCHIESSEN);
			b4_mh.art = 0;
			b4_mh.akt = AKT_OEFFNEN;
			b4_mh.sc = 0;
		}
		break;

	case MH_SCHIESSEN:
		if (b4_mh.art == 2)
		{
			if (b4_mh.gun > 0) b4_mh.gun -= 133;
			if (b4_mh.gun < 0) b4_mh.gun += 133;
		}
		else
		{
			/* WinkelToPlayer */
			int neww = 0;
			if (b4_mh.akt == AKT_SCHIESSEN)
			{
				neww = b4_atan((pl.y >> 8) + 40 - (y + 200), (pl.x >> 8) + 35 - (x + 100));
				if (neww < -20) neww = -20;
				if (neww > 50) neww = 50;
				neww *= 16;
			}
			if (b4_mh.gun < neww) b4_mh.gun += 21;
			if (b4_mh.gun > neww) b4_mh.gun -= 21;
		}
		if (b4_mh.art == 1) { e->y += SYNC(FX(1.5)); e->x -= SYNC(FX(1)); }
		b4_mh.kiefer = sin_d(b4_mh.sc / 16) * 50;
		switch (b4_mh.akt)
		{
		case AKT_OEFFNEN:
			b4_mh.toff = sin_d(b4_mh.sc / 16) * 30;
			b4_mh.sc += 31;
			if (b4_mh.sc >= 1440)
			{
				kinds[E_METALHEAD].shootable = 1;
				if (b4_mh.turbine)
				{
					const int l = 85 + (int)(b4_mh.toff >> 8);
					kinds[E_METALHEAD].box = (Box){ (int16_t)l, 210, (int16_t)(l + 98), 210 + 49 + 20 };
				}
				b4_mh.akt = AKT_SCHIESSEN;
				if (b4_mh.turbine)
				{
					b4_mh.cnt = (int16_t)(b4_mh.art == 0 ? rnd(4) + 4 : 15);
					b4_mh.delay = 30;
				}
				else
				{
					b4_mh.cnt = 100;
					b4_mh.delay = 6;
				}
			}
			break;

		case AKT_SCHIESSEN:
			if (b4_mh.toff < FX(30)) b4_mh.toff += SYNC(FX(5));
			else b4_mh.toff = FX(30);
			if (--b4_mh.delay > 0) break;
			if (b4_mh.cnt <= 0)
			{
				b4_mh.akt = AKT_SCHLIESSEN;
				kinds[E_METALHEAD].shootable = !b4_mh.turbine;
				break;
			}
			if (b4_mh.turbine)
			{
				const int g = b4_mh.gun / 16;
				b4_mh.delay = 54;
				if (b4_mh.art == 0)               /* FIREBALL_BIG along the gun */
					enemy_shot_pic(&P_MHBOMB, x + 110 + cos_d(g) * 70 / 256, y + 195 + sin_d(g) * 70 / 256,
					               cos_d(g) * 50, sin_d(g) * 50, 0, DMG(30), 0);
				if (b4_mh.art == 1)               /* three FIREBALLs */
				{
					for (int a = -40; a <= 40; a += 40)
						enemy_shot_pic(&b4_fireball, x + 130 + cos_d(g) * 70 / 256, y + 200 + sin_d(g) * 70 / 256,
						               cos_d(g + a) * 50, sin_d(g + a) * 50, 0, DMG(15), a != 0);
					if (y > V2 + 240) b4_mh.cnt = 0;
				}
				if (b4_mh.art == 2)               /* FIREBALL_BOMB at the player */
				{
					Ent *b = ent_new(E_MHBOMB, x + 180, y + 195);
					if (b)
					{
						b->active = 1;
						b->xs = (int16_t)(FX((pl.x >> 8) + 35 - (x + 180 + 50)) / 10);
						b->ys = (int16_t)(FX((pl.y >> 8) + 40 - (y + 195 + 180)) / 10);
						b->ya = FX(3);
						sfx(SFX_ESHOT);
					}
				}
				b4_mh.toff = FX(15);
			}
			else
			{
				b4_mh.delay = 3;
				if (b4_mh.art == 1)               /* WALKERFIRE */
					b4_flame_shot(x + 95, y + 185 + rnd(5), FX(30), FX(15), DMG(8), 66);
				else if (b4_mh.cnt % 12 == 0)     /* PFLANZESHOT */
				{
					const int sx = x + 100, sy = y + 213 + rnd(5);
					enemy_shot_pic(&P_pflshot, sx, sy, FX((pl.x >> 8) + 35 - (sx + 20)) / 10,
					               FX((pl.y >> 8) + 40 - (sy + 20) - 160) / 10, FX(3.25) / 6, DMG(10), 0);
				}
			}
			b4_mh.cnt--;
			break;

		default:                                 /* closing */
			b4_mh.toff = sin_d(b4_mh.sc / 16) * 30;
			b4_mh.sc -= 31;
			if (b4_mh.sc <= 0)
			{
				const int n = e->energy < 2000 ? 4 : 3;
				int j = rnd(n);
				while (j == b4_mh.art) j = rnd(n);
				b4_mh.art = (int16_t)j;
				b4_mh.akt = AKT_OEFFNEN;
				if (j == 2) b4_mh_moveto(e, V1 + 10, V2, MH_SCHIESSEN);
				else if (j == 1) b4_mh_moveto(e, V1 + 130, V2, MH_SCHIESSEN);
				else if (j == 0) b4_mh_moveto(e, V1 + 70, V2 + 80, MH_SCHIESSEN);
				else
				{
					e->ys = FX(50);
					e->ya = FX(10);
					e->state = MH_CRUSHEN2;
					sfx(SFX_LAND);
					b4_mh.moving = 0;
					b4_mh.sc = 0;
				}
			}
			break;
		}
		break;

	case MH_EXPLODIEREN:
		e->energy = 100;
		if (--b4_mh.count < 0 && b4_mh.ende > 0)
		{
			/* a vertebra blows off */
			int nx, ny;
			b4_mh_neck(e, b4_mh.hx, b4_mh.hy, b4_mh.ende, &nx, &ny);
			fx_add(&P_expl, nx + 32, ny + 8, 2);
			sfx(SFX_EXPLODE);
			b4_mh.count = 15;
			b4_mh.ende--;
			if (b4_mh.ende == 20)
			{
				/* the flying head bursts */
				fx_add(&P_explbig, x + 60, y + 60, 2);
				fx_add(&P_explbig, x + 140, y + 120, 3);
				fx_add(&P_explbig, x + 80, y + 170, 2);
				sfx(SFX_BIGEXPLODE);
				e->xs = e->ys = e->xa = e->ya = 0;
			}
			if (b4_mh.ende <= 0) e->energy = 0;
		}
		break;

	default:
		break;
	}

	/* touching the skull */
	{
		static const Box r = { 0, 0, 110, 140 };
		if (e->state != MH_EXPLODIEREN && pl.state != PS_DEAD &&
		    boxes_meet(x + 50, y + 130, &r, pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box))
			player_damage(DMG_T(4));
	}
	b4_move(e);
	return 1;
}
static int METALHEAD_explode(Ent *e)
{
	sfx(SFX_BIGEXPLODE);
	b4_after_boss();
	return 1;
}
static int METALHEAD_draw(const Ent *e, uint32_t t)
{
	int nx, ny;
	if (e->state == MH_EXPLODIEREN)
	{
		if (b4_mh.ende > 20) draw_pic(&P_METALHEAD, 0, e->x, e->y, 0);
		for (int i = 0; i < b4_mh.ende; i += 3)
		{
			b4_mh_neck(e, b4_mh.hx, b4_mh.hy, i, &nx, &ny);
			draw_pic(&b4_hals, 0, FX(nx), FX(ny), 0);
		}
		return 1;
	}
	const int hx = e->x >> 8, hy = e->y >> 8;
	draw_pic(&b4_kiefer, 0, e->x + FX(87), e->y + FX(107) + b4_mh.kiefer, 0);
	for (int i = 0; i < 24; i += i < 21 ? 3 : 2)
	{
		b4_mh_neck(e, hx, hy, i, &nx, &ny);
		draw_pic(&b4_hals, 0, FX(nx), FX(ny), 0);
	}
	draw_pic(&P_METALHEAD, 0, e->x, e->y, 0);
	if (b4_mh.turbine) draw_pic(&b4_turbine, 0, e->x + FX(85) + b4_mh.toff, e->y + FX(202), 0);
	draw_pic(&b4_kiefer2, 0, e->x + FX(78), e->y + FX(180) + b4_mh.kiefer, 0);
	return 1;
}

/* the metal head's fire bomb (FIREBALL_BOMB): flies at the player, falls (3); at a wall or on
 * him (10) it bursts into flames (Hurrican's 8 FEUERFALLE: 4 rising flames, 5 each) */
static void MHBOMB_load(void) { }
static void MHBOMB_init(Ent *e) { }
static int MHBOMB_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (++e->t1 > 300 || ((bl | br | bo | bu) & B_WALL)) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(10)); e->energy = 0; return 1; }
	return 0;
}
static int MHBOMB_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_expl, x + 30, y + 27, 2);
	for (int i = 0; i < 4; i++)
		b4_flame_shot(x - 15 + rnd(30), y - 40 + rnd(30), FX(rnd(20) - 10) / 8, FX(-20), DMG(5), 90);
	sfx(SFX_EXPLODE);
	return 1;
}
static int MHBOMB_draw(const Ent *e, uint32_t t) { return 0; }

/* ================================================================ 138 SKELETOR
 * The robot skeleton, the end stage's first boss (Boss_Skeletor.cpp). When it comes on the
 * screen it is put above the screen and the screen locks on (value1, value2); there it drops
 * in (80), lands, stands up and laughs, and the boss music plays. Then, never the same twice:
 * the gatling (41 shots at the player, 4 each, the gun turned 50..120 degrees), grenades (21,
 * thrown in arcs; each lies 2 s and bursts into a column of flames), a jump over the arena
 * (25 / -60, gravity 6) with a fan of five big fire balls (30) at its top, a run to the other
 * side (accelerating 2, at most 31) firing 5 homing rockets (50) - after each it laughs (2 s).
 * Touching it costs 10 a tenth. At 0 it bursts (12500 points), leaves its skull (which flies
 * round the player from then on) and the screen goes back to the player 2 s later.
 * Left out: the after-images of the run, the empty cartridges, the smoke; the muzzle flame is
 * drawn upright. Its gatling shots are drawn with the engine's bullet.
 * state: see SK_*; t1 / t2 unused (b4_sk). */
enum { SK_NOTVISIBLE, SK_EINFLIEGEN, SK_ERHOLEN, SK_STEHEN, SK_SCHIESSEN, SK_BOMBARDIEREN, SK_SPRINGEN, SK_LACHEN, SK_RENNEN, SK_EXPLODIEREN };
static struct {
	int16_t cnt, aoff, last, delay, acount, gun;
	uint8_t laughed, moved;
} b4_sk;
static int b4_skull = -1;               /* the skull's entity (Skeletor's or the wall's) */
static int b4_wall = -1;                /* the wall's entity (THEWALL_init) */

static void b4_sk_laugh(Ent *e)
{
	e->anim = 2;
	e->xs = e->ys = e->xa = e->ya = 0;
	e->state = SK_LACHEN;
	b4_sk.acount = 120;
}
/* the frame while it jumps or runs over the arena */
static void b4_sk_runframe(Ent *e, int x)
{
	const int sx = cam_xoff();
	int a;
	if (x + 60 < e->v1 + 320) { e->dir = 1; a = 10 + (x - sx) / 56; }
	else { e->dir = 0; a = 15 - (x - sx - 260) / 56; }
	e->anim = (int16_t)(a < 0 ? 0 : a > 15 ? 15 : a);
}

static void SKELETOR_load(void) { }
static void SKELETOR_init(Ent *e)
{
	memset(&b4_sk, 0, sizeof b4_sk);
	b4_sk.last = -1;
	e->anim = 10;
	e->dir = 1;
	e->state = SK_NOTVISIBLE;
	e->energy = (int16_t)b4_hp(7000);
}
static int SKELETOR_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int V1 = e->v1, V2 = e->v2;
	b4_trace("SKELETOR", e, b4_sk.cnt, kinds[E_SKELETOR].shootable);
	/* CalcGunWinkel: 90 + atan, 50..120 */
	{
		int g = 90 + b4_atan((pl.y >> 8) + 40 - (y + 100), (pl.x >> 8) + 35 - (x + 60));
		b4_sk.gun = (int16_t)(g < 50 ? 50 : g > 120 ? 120 : g);
	}
	if (e->state != SK_NOTVISIBLE && e->state != SK_EXPLODIEREN) boss_bar(e->energy, 7000);

	/* on the screen: put above it, the screen goes to the arena */
	if (!b4_sk.moved)
	{
		b4_sk.moved = 1;
		e->x = e->xo = FX(cam_xoff() + 100);
		e->y = e->yo = FX(cam_yoff() - 250);
		x = e->x >> 8; y = e->y >> 8;
		boss_lock(V1, V2, 10);
	}

	switch (e->state)
	{
	case SK_LACHEN:                              /* after the laugh */
		if (--b4_sk.acount < 0) e->state = SK_STEHEN;
		break;

	case SK_ERHOLEN:                             /* stands up */
		b4_sk.acount++;
		if (b4_sk.acount > 60 && e->anim == 0) e->anim = 1;
		if (b4_sk.acount > 120 && e->anim == 1) e->anim = 2;
		if (b4_sk.acount > 150)
		{
			e->state = SK_STEHEN;
			if (!b4_sk.laughed) { b4_sk.laughed = 1; music_boss(); }
		}
		break;

	case SK_STEHEN:                              /* the next attack */
	{
		int j = rnd(4);
		while (j == b4_sk.last) j = rnd(4);
		b4_sk.last = (int16_t)j;
		switch (j)
		{
		case 0: b4_sk.delay = 0; b4_sk.cnt = 40; e->state = SK_SCHIESSEN; b4_sk.aoff = 0; break;
		case 1: b4_sk.delay = 0; b4_sk.cnt = 20; e->state = SK_BOMBARDIEREN; b4_sk.aoff = 0; break;
		case 2: b4_sk.delay = 18; b4_sk.cnt = 1; e->state = SK_SPRINGEN; e->xs = FX(25); e->ys = FX(-60); e->ya = FX(6); break;
		default: e->state = SK_RENNEN; e->xs = 0; e->xa = FX(2); b4_sk.delay = 54; b4_sk.cnt = 5; break;
		}
		if (x > V1 + 320) { e->xs = (int16_t)-e->xs; e->xa = (int16_t)-e->xa; }
		break;
	}

	case SK_RENNEN:                              /* runs over the arena firing rockets */
		if (b4_sk.cnt > 0) b4_sk.delay--;
		if (b4_sk.delay < 0)
		{
			Ent *r = ent_new(E_B4ORB, x + 50, y + 80);
			if (r)
			{
				r->active = 1;
				r->size = 0;                     /* TURRIEXTRAWURST */
				r->t2 = (int16_t)((2 * b4_sk.cnt + 8) * 6);
				r->ys = FX(-16);
				r->ya = FX(1);
			}
			sfx(SFX_ESHOT);
			b4_sk.delay = 18;
			b4_sk.cnt--;
		}
		if ((e->xs > FX(31) && e->xa > 0) || (e->xs < FX(-31) && e->xa < 0)) e->xa = (int16_t)-e->xa;
		if ((e->xa > 0 && e->xs < 0 && e->xs > FX(-2)) || (e->xa < 0 && e->xs > 0 && e->xs < FX(2))) e->xa = 0;
		b4_sk_runframe(e, x);
		if (x < V1 + 20 || x > V1 + 500)
		{
			e->x = FX(x < V1 + 50 ? V1 + 20 : V1 + 500);
			b4_sk_laugh(e);
		}
		break;

	case SK_SPRINGEN:                            /* the jump, the fan of fire balls at its top */
		b4_sk_runframe(e, x);
		if (b4_sk.cnt == 1 && x + 62 > V1 + 300 && x + 62 < V1 + 340)
		{
			b4_sk.cnt = 0;
			for (int i = 0; i < 5; i++)
			{
				const int w = 40 + i * 25;
				enemy_shot_pic(&P_MHBOMB, x + 50, y + 70, cos_d(w) * 50, sin_d(w) * 50, 0, DMG(30), i != 2);
			}
		}
		if (y > V2 + 250)
		{
			e->y = FX(V2 + 250);
			e->xs = e->ys = e->xa = e->ya = 0;
			e->anim = 0;
			e->state = SK_ERHOLEN;
			b4_sk.acount = 0;
			fx_add(&P_smoke, x + 40, y + 180, 3);
			sfx(SFX_LAND);
		}
		break;

	case SK_BOMBARDIEREN:                        /* grenades */
		if (--b4_sk.acount <= 0)
		{
			b4_sk.aoff++;
			if (b4_sk.aoff % 2 == 0) b4_sk.acount = 12;
			else
			{
				const int off = e->dir ? 0 : -95;
				int w = 40 - e->anim * 4 - rnd(8);
				b4_sk.acount = 9;
				if (!e->dir) w += 1;
				if (w < 0) w = 0;
				if (!e->dir) w = -w;
				Ent *g = ent_new(E_SKGRANATE, x + 100 + off, y + 80);
				if (g)
				{
					g->active = 1;
					g->xs = (int16_t)(FX(w) * 18 / 10);
					g->ys = FX(-18);
					g->ya = FX(6.2);
				}
				fx_add(&P_flare, x + 60 + off, y + 45, 2);
				sfx(SFX_ESHOT);
				b4_sk.cnt--;
			}
			{
				const int a = b4_sk.aoff % 14;
				e->anim = (int16_t)(a < 8 ? a + 2 : 16 - a);
			}
			if (b4_sk.aoff > 42) b4_sk.aoff = 0;
			if (b4_sk.cnt < 0) b4_sk_laugh(e);
		}
		break;

	case SK_SCHIESSEN:                           /* the gatling */
		b4_sk.delay--;
		e->anim = b4_sk.delay > 6 ? 3 : 2;
		if (b4_sk.delay <= 0)
		{
			b4_sk.cnt--;
			b4_sk.delay = 12;
			int w = b4_sk.gun + rnd(4) - 2 - 90;
			if (!e->dir) w += 180;
			enemy_shot(SH_BULLET, x + (e->dir ? 100 : 20), y + 75, cos_d(w) * 100, sin_d(w) * 100, DMG(4));
			if (b4_sk.cnt < 0) b4_sk_laugh(e);
		}
		break;

	case SK_NOTVISIBLE:                          /* wait for the screen, then drop in */
		if (boss_locked_there())
		{
			sfx(SFX_LAND);
			e->state = SK_EINFLIEGEN;
			e->xs = 0;
			e->ys = FX(80);
			kinds[E_SKELETOR].shootable = 1;
		}
		break;

	case SK_EINFLIEGEN:
		e->dir = 1;
		if (y > V2 + 250)
		{
			e->y = FX(V2 + 250);
			e->anim = 0;
			e->ys = 0;
			e->state = SK_ERHOLEN;
			b4_sk.acount = 0;
			fx_add(&P_smoke, x + 30, y + 170, 3);
			fx_add(&P_smoke, x + 90, y + 170, 3);
			sfx(SFX_LAND);
		}
		break;

	case SK_EXPLODIEREN:
		if ((b4_sk.delay & 15) == 0) fx_add(&P_explbig, x + rnd(160) - 20, y + rnd(200), 2);
		if (--b4_sk.delay < 0) e->energy = 0;
		break;

	default:
		break;
	}

	/* no energy left: it bursts and leaves its skull */
	if (e->energy <= 0 && e->state != SK_EXPLODIEREN)
	{
		e->energy = 1;
		kinds[E_SKELETOR].shootable = 0;
		e->state = SK_EXPLODIEREN;
		b4_sk.delay = 120;
		Ent *s = ent_new(E_SKULL, x + 50, y + 50);
		if (s)
		{
			s->active = 1;
			s->v2 = 99;
			SKULL_init(s);
			b4_skull = (int)(s - ents);
		}
		fx_add(&P_explbig, x + 62, y + 100, 2);
		fx_add(&P_explbig, x + 20, y + 40, 3);
		fx_add(&P_explbig, x + 100, y + 160, 3);
		sfx(SFX_BIGEXPLODE);
	}

	if (touching) player_damage(DMG_T(10));
	b4_move(e);
	return 1;
}
static int SKELETOR_explode(Ent *e)
{
	b4_after_boss();
	return 1;
}
static int SKELETOR_draw(const Ent *e, uint32_t t)
{
	if (e->state == SK_EXPLODIEREN) return 1;
	draw_pic(&P_SKELETOR, e->anim, e->x, e->y + (e->anim >= 10 ? FX(10) : 0), e->dir);
	if (e->state == SK_SCHIESSEN && b4_sk.delay > 6)
		draw_pic(&b4_skflame, b4_sk.cnt & 1, e->x + FX(e->dir ? 87 : -120), e->y + FX(60), !e->dir);
	return 1;
}

/* Skeletor's grenade (SKELETORGRANATE): thrown in an arc (gravity 6.2), stops on the ground,
 * bursts 2 s after the throw into a column of flames (Hurrican's 15 FEUERFALLE: 3 rising
 * flames here, 5 each). It does not hurt by itself. */
static void SKGRANATE_load(void) { }
static void SKGRANATE_init(Ent *e) { }
static int SKGRANATE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 9, 3);
	if (bu & B_WALL) e->xs = e->ys = e->xa = e->ya = 0;
	if (++e->t1 >= 120) { e->energy = 0; return 1; }
	return 0;
}
static int SKGRANATE_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_expl, x + 10, y + 10, 2);
	for (int k = 0; k < 3; k++)
		b4_flame_shot(x - 32, y - 40 - k * 30, FX(rnd(20) - 10) / 8, FX(-20), DMG(5), 100);
	sfx(SFX_EXPLODE);
	return 1;
}
static int SKGRANATE_draw(const Ent *e, uint32_t t) { return 0; }

/* the green orb (golemschuss.png), two of Hurrican's shots:
 * size 0 - Skeletor's rocket (TURRIEXTRAWURST): rises (-16, slowing by 1) and hovers for t2
 *          steps, then flies straight at the player (35);
 * size 1 - the wall's golem shot (GOLEMSCHUSS): flies left in a wave (its y speed swings
 *          between -40 and 40 at 10).
 * Bursts at walls or on the player (50). */
static void B4ORB_load(void) { }
static void B4ORB_init(Ent *e) { }
static int B4ORB_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 24, 3);
	if (e->size == 0)
	{
		if (e->t2 > 0)
		{
			e->t2--;
			if (e->ys >= 0) e->ys = 0;
		}
		else if (e->ya)
		{
			fx xs, ys;
			e->ya = 0;
			aim_at(x, y, 35, &xs, &ys);
			e->xs = (int16_t)xs;
			e->ys = (int16_t)ys;
		}
	}
	else if ((e->ya > 0 && e->ys > FX(40)) || (e->ya < 0 && e->ys < FX(-40))) e->ya = (int16_t)-e->ya;
	if (++e->t1 > 600 || ((bl | br | bo | bu) & B_WALL)) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(50)); e->energy = 0; return 1; }
	return 0;
}
static int B4ORB_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int B4ORB_draw(const Ent *e, uint32_t t) { return 0; }

/* ================================================================ the skull (Boss_Skull.cpp)
 * Skeletor's skull, which goes on to the wall. It cannot be hurt (its energy is set again every
 * step); touching it costs 1 a tenth. state 0: waits t1 (Skeletor's: 2 s), then flies round the
 * player (acceleration 2 towards him, at most 25 / 15, kept on the screen); 1: called back by
 * the wall - it slows down and flies (10) to the wall's socket, where it docks (the wall's
 * value1 = 1; the boss music starts the first time); 2: the wall is beaten - it flies to the
 * screen's middle exploding and bursts after 2 s. The wall's skull (value1 99) shoots blue
 * shots (12) at the player every 15 tenths once the wall is below 1500.
 * Left out: the sparks, the slow motion, the riding flying sack it leaves (the game ends here). */
static int b4_wall_music;

static Ent *b4_wall_ent(void)
{
	return b4_wall >= 0 && b4_wall < nents && ents[b4_wall].type == E_THEWALL ? &ents[b4_wall] : 0;
}
static Ent *b4_skull_ent(void)
{
	return b4_skull >= 0 && b4_skull < nents && ents[b4_skull].type == E_SKULL ? &ents[b4_skull] : 0;
}

static void SKULL_load(void) { }
static void SKULL_init(Ent *e)
{
	e->state = 0;
	e->t1 = e->v2 == 99 ? 120 : 0;
	e->t2 = 60;
}
static int SKULL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	Ent *w = b4_wall_ent();
	e->energy = 30000;
	e->dir = 0;
	{
		int a = (pcx() - (x + 16)) / 30;
		a = (a < -5 ? -5 : a > 9 ? 9 : a) + 5;
		if (e->xs < 0) a = 20 - a;
		e->anim = (int16_t)(a < 0 ? 0 : a > 19 ? 19 : a);
	}
	b4_move(e);
	x = e->x >> 8; y = e->y >> 8;
	switch (e->state)
	{
	case 0:                                      /* round the player */
		if (e->t1 > 0) { e->t1--; break; }
		{
			const int sx = cam_xoff(), sy = cam_yoff();
			e->xa = x + 16 > pcx() ? FX(-2) : FX(2);
			e->ya = y + 20 > (pl.y >> 8) + 10 ? FX(-2) : FX(2);
			if (e->xs > FX(25)) e->xs = FX(25);
			if (e->xs < FX(-25)) e->xs = FX(-25);
			if (e->ys > FX(15)) e->ys = FX(15);
			if (e->ys < FX(-15)) e->ys = FX(-15);
			if ((e->xs < 0 && x < sx) || (e->xs > 0 && x > sx + 640)) e->xs = (int16_t)-e->xs;
			if ((e->ys < 0 && y < sy) || (e->ys > 0 && y > sy + 640)) e->ys = (int16_t)-e->ys;
		}
		break;

	case 1:                                      /* back to the machine */
	{
		const fx d = SYNC(FX(1)), v = SYNC(FX(10));
		e->xa = e->ya = 0;
		if (e->xs < 0) { e->xs += d; if (e->xs > 0) e->xs = 0; }
		if (e->xs > 0) { e->xs -= d; if (e->xs < 0) e->xs = 0; }
		if (e->ys < 0) { e->ys += d; if (e->ys > 0) e->ys = 0; }
		if (e->ys > 0) { e->ys -= d; if (e->ys < 0) e->ys = 0; }
		if (!w) break;
		const fx mx = w->x + FX(5), my = w->y + FX(229);
		if (e->x < mx) { e->x += v; if (e->x > mx) e->x = mx; }
		if (e->x > mx) { e->x -= v; if (e->x < mx) e->x = mx; }
		if (e->y < my) { e->y += v; if (e->y > my) e->y = my; }
		if (e->y > my) { e->y -= v; if (e->y < my) e->y = my; }
		if (e->x == mx && e->y == my)
		{
			w->v1 = 1;
			sfx(SFX_HIT);
			if (!b4_wall_music) { b4_wall_music = 1; music_boss(); }
			e->type = E_NONE;
			return 1;
		}
		break;
	}

	default:                                     /* the wall is beaten: to the middle, bursting */
		if (--e->t1 <= 0)
		{
			e->t1 = 5;
			fx_add(&P_expl, x + rnd(30), y + rnd(30), 2);
			sfx(SFX_EXPLODE);
		}
		e->xa = x + 16 > cam_xoff() + 320 ? FX(-4) : FX(4);
		e->ya = y + 20 > cam_yoff() + 200 ? FX(-4) : FX(4);
		if (e->xs > FX(25)) e->xs = FX(25);
		if (e->xs < FX(-25)) e->xs = FX(-25);
		if (e->ys > FX(15)) e->ys = FX(15);
		if (e->ys < FX(-15)) e->ys = FX(-15);
		if (--e->t2 < 0)
		{
			fx_add(&P_explbig, x + 16, y + 20, 2);
			fx_add(&P_explbig, x - 30, y - 20, 3);
			fx_add(&P_explbig, x + 60, y + 50, 3);
			sfx(SFX_BIGEXPLODE);
			e->energy = 0;
			return 1;
		}
		return 1;
	}

	/* the wall's skull shoots at the player when the wall is low */
	if (e->v1 == 99 && w && w->energy < 1500 && --e->t2 < 0)
	{
		fx xs, ys;
		e->t2 = 90;
		aim_at(x - 10 + 20, y + 20, 20, &xs, &ys);
		enemy_shot_pic(&b4_such2, x - 10, y, xs, ys, 0, DMG(12), 0);
	}
	if (touching) player_damage(DMG_T(1));
	return 1;
}
static int SKULL_explode(Ent *e) { return 1; }
static int SKULL_draw(const Ent *e, uint32_t t) { return 0; }

/* ================================================================ 139 THEWALL
 * The end boss's wall, the last fight (Boss_TheWall.cpp): the machine at the right, the tunnel
 * with the heart behind two doors at the left. When the player comes near, the screen locks
 * (the machine's x - 355) and the wall calls Skeletor's skull back; docked on the machine it
 * takes the hits (250) while the machine attacks (after a laugh, 1 s), never the same twice:
 * 20 golem shots in waves (50), 16 purple balls (10), and - the player on the boxes - the big
 * laser (500 a tenth after it has charged), else an electric shot that sets the floor alight
 * (20 a tenth for 2 s). When the skull has no energy left, the next hit opens the doors: the
 * skull flies off (shooting at the player once the wall is below 1500) and the heart can be
 * hit for 5 s (4000); then the doors close and the skull is called back again. At 100 the
 * heart's glass breaks, the skull bursts in the middle of the screen, the screen goes back to
 * the player - and the game is won (stage_clear(), 4 s later; Hurrican waits for its outro).
 * Drawn: the machine, its front (recoils when it fires), the docked skull (skull.png's frame,
 * Hurrican cuts it from endbosshirni.png), the doors (cut where they slide in) and their frame;
 * with the doors open the turning tunnel and the heart. Left out: the covering ring and the
 * tube holder over the heart, the red soup, the doors' shadow and shading, the smoke, sparks
 * and glass splinters, the sounds of doors and heart. The laser is drawn as two filled boxes.
 * state: see W_*; v1 the skull: -1 away, -2 called, 1 docked. */
enum { W_INIT, W_STEHEN, W_GOLEM, W_LILA, W_ELEKTRO, W_LASER, W_OEFFNEN, W_SCHLIESSEN, W_EXPLODIEREN };
#define B4_TOX (-320)                   /* TunnelOffx */
#define B4_TOY 140                      /* TunnelOffY */
static struct {
	int16_t cnt, delay, acount, countopen, oldenergy, skullenergy;
	int16_t heartanim, heartbeat, door, kringel, laughing, dummy, skullshot, laser, target;
	uint8_t last, done;
	int32_t vornex;                     /* 8.8 pixels */
} b4_w;

/* ReturnSkull: the skull flies back to the machine (one is made when it is gone or far off) */
static void b4_wall_return_skull(Ent *w)
{
	Ent *s = b4_skull_ent();
	const int sx = cam_xoff() + 320, sy = cam_yoff() + 40;
	if (!s)
	{
		s = ent_new(E_SKULL, sx, sy);
		if (!s) { w->v1 = 1; return; }
		s->active = 1;
		SKULL_init(s);
		b4_skull = (int)(s - ents);
	}
	else if (abs((s->x >> 8) - (w->x >> 8)) > 900 || abs((s->y >> 8) - (w->y >> 8)) > 700)
	{
		s->x = s->xo = FX(sx);
		s->y = s->yo = FX(sy);
	}
	s->state = 1;
}

/* NeueAktion */
static void b4_wall_action(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	if (b4_w.countopen <= 0)
	{
		/* the doors open, the skull flies off */
		b4_w.countopen = 1;
		b4_w.acount = 300;
		e->state = W_OEFFNEN;
		if (b4_w.laser >= 0 && b4_w.laser < nents && ents[b4_w.laser].type == E_WLASER) ents[b4_w.laser].type = E_NONE;
		e->v1 = -1;
		Ent *s = ent_new(E_SKULL, x + 3, y + 228);
		if (s)
		{
			s->active = 1;
			s->v1 = 99;
			SKULL_init(s);
			b4_skull = (int)(s - ents);
		}
	}
	else
	{
		int i = rnd(3);
		while (i == b4_w.last) i = rnd(3);
		/* the player on the boxes: the laser, else the electric floor */
		const int laser = (pl.y >> 8) < cam_yoff() + 480 - 160;
		if (laser) i = 2;
		b4_w.last = (uint8_t)i;
		switch (i)
		{
		case 0: b4_w.cnt = 20; b4_w.delay = 30; e->state = W_GOLEM; b4_w.dummy = (int16_t)(rnd(10) + 15); break;
		case 1: b4_w.cnt = 16; b4_w.delay = 30; e->state = W_LILA; break;
		default: b4_w.cnt = 2; b4_w.delay = 120; e->state = laser ? W_LASER : W_ELEKTRO; break;
		}
	}
	b4_w.laughing = 60;
}

static void THEWALL_load(void) { }
static void THEWALL_init(Ent *e)
{
	memset(&b4_w, 0, sizeof b4_w);
	e->energy = (int16_t)b4_hp(4000);
	b4_w.countopen = 3;
	b4_w.skullenergy = 250;
	b4_w.oldenergy = e->energy;
	b4_w.acount = 60;
	b4_w.laser = -1;
	b4_w.target = -1;
	b4_w.last = 9;
	e->v1 = -1;
	e->state = W_INIT;
	b4_wall = (int)(e - ents);
	b4_wall_music = 0;
}
static int THEWALL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	b4_trace("WALL", e, e->v1, b4_w.skullenergy);
	/* the hits on its target (the skull or the heart) */
	{
		Ent *tg = b4_w.target >= 0 && b4_w.target < nents && ents[b4_w.target].type == E_WTARGET ? &ents[b4_w.target] : 0;
		if (!tg && (tg = ent_new(E_WTARGET, x, y)) != 0) { tg->active = 1; b4_w.target = (int16_t)(tg - ents); }
		if (tg)
		{
			e->energy = (int16_t)(e->energy - (30000 - tg->energy));
			tg->energy = 30000;
			tg->x = tg->xo = e->x;
			tg->y = tg->yo = e->y;
		}
	}
	if (e->state != W_INIT) boss_bar(e->energy, 4000);
	/* the docked skull shoots when the wall is low */
	if (e->energy < 1500 && e->v1 == 1 && --b4_w.skullshot < 0)
	{
		fx xs, ys;
		b4_w.skullshot = 90;
		aim_at(x + 20, y + 260, 20, &xs, &ys);
		enemy_shot_pic(&b4_such2, x, y + 240, xs, ys, 0, DMG(12), 0);
	}
	if (b4_w.laughing > 0) { b4_w.laughing--; return 1; }

	const int open = e->state == W_OEFFNEN || e->state == W_SCHLIESSEN;
	if (e->state != W_EXPLODIEREN)
	{
		if (b4_w.vornex > 0) b4_w.vornex -= SYNC(FX(1));
		else b4_w.vornex = 0;
		/* hit: the skull takes it while it has energy, then the doors open */
		if (e->energy < b4_w.oldenergy)
		{
			if (b4_w.skullenergy > 0)
			{
				b4_w.skullenergy -= b4_w.oldenergy - e->energy;
				e->energy = b4_w.oldenergy;
			}
			else if (!open)
			{
				b4_w.countopen = 0;
				b4_wall_action(e);
			}
		}
		/* what can be hit: the docked skull, or the heart */
		kinds[E_WTARGET].shootable = 0;
		if (!open && e->v1 == 1)
		{
			kinds[E_WTARGET].box = (Box){ 5, 232, 25, 260 };
			kinds[E_WTARGET].shootable = 1;
		}
		if (open)
		{
			kinds[E_WTARGET].box = (Box){ B4_TOX + 116, B4_TOY + 59, B4_TOX + 116 + 38, B4_TOY + 59 + 42 };
			kinds[E_WTARGET].shootable = 1;
		}
		/* the turning tunnel, the heart beating the faster the lower it is */
		b4_w.kringel = (int16_t)((b4_w.kringel + 10) % 2160);
		{
			static const int16_t lv[9] = { 3500, 3000, 2500, 2000, 1500, 1000, 500, 250, 150 };
			int beat = 1;
			for (int k = 1; k < 9; k++) if (e->energy < lv[k]) beat = k + 1;
			b4_w.heartbeat = (int16_t)(b4_w.heartbeat + beat);
			if (b4_w.heartbeat > 60)
			{
				if (b4_w.heartanim == 0) { b4_w.heartanim = 1; b4_w.heartbeat = 30; }
				else { b4_w.heartanim = 0; b4_w.heartbeat = 0; }
			}
		}
		/* the screen locks when the player comes near */
		if (!boss_lock_on && !b4_w.done)
		{
			const int dx = pcx() - (x + 142), dy = pcy() - (y + 240);
			if (dx * dx + dy * dy < 800 * 800 && abs(pcx() - (x - 357)) < 400) boss_lock(x - 355, y, 10);
		}
		/* beaten */
		if (e->energy < 100)
		{
			e->energy = 100;
			kinds[E_WTARGET].shootable = 0;
			boss_unlock();
			b4_w.cnt = 1;
			b4_w.done = 1;
			e->state = W_EXPLODIEREN;
			b4_w.laughing = 0;
			/* all its shots gone (Projectiles.ClearAll): once, at the end */
			for (int i = 0; i < MAX_SHOTS; i++) if (shots[i].enemy) shots[i].on = 0;
			for (int i = nents_map; i < nents; i++)
			{
				const int t = ents[i].type;
				if (t == E_B4ORB || t == E_WELEKTRO || t == E_WPAMPE || t == E_WLASER) ents[i].type = E_NONE;
			}
			b4_w.acount = 240;
			fx_add(&P_explbig, x + B4_TOX + 135, y + B4_TOY + 80, 2);
			fx_add(&P_expl, x + B4_TOX + 120, y + B4_TOY + 60, 3);
			sfx(SFX_BIGEXPLODE);
			Ent *s = b4_skull_ent();
			if (s) { s->v1 = 0; s->t1 = 3; s->t2 = 120; s->state = 2; }
		}
	}

	switch (e->state)
	{
	case W_INIT:
		if (abs(pcx() - (x - 357)) < 400 && abs((pl.y >> 8) + 45 - (y + 240)) < 400)
		{
			const int dx = pcx() - (x + 142), dy = pcy() - (y + 240);
			if (dx * dx + dy * dy < 600 * 600)
			{
				if (e->v1 == -1) { e->v1 = -2; b4_wall_return_skull(e); }
				if (e->v1 == 1 && --b4_w.acount <= 0) b4_wall_action(e);
			}
		}
		break;

	case W_GOLEM:                                /* golem shots in waves */
		if (--b4_w.delay < 0)
		{
			Ent *g = ent_new(E_B4ORB, x, y + 332);
			if (g)
			{
				g->active = 1;
				g->size = 1;
				g->xs = (int16_t)FX(-b4_w.dummy);
				g->ys = FX(-40);
				g->ya = FX(10);
				if (b4_w.dummy % 2 == 0) { g->ys = FX(40); g->ya = FX(-10); }
			}
			fx_add(&P_flare, x - 15, y + 310, 2);
			sfx(SFX_ESHOT);
			b4_w.delay = 7;
			b4_w.vornex = FX(5);
			if (--b4_w.cnt <= 0) b4_wall_action(e);
		}
		break;

	case W_LILA:                                 /* purple balls */
		if (--b4_w.delay < 0)
		{
			fx ys = -FX(rnd(12) + 5), ya = FX(rnd(10) + 5) / 30;
			if (rnd(2) == 0) { ys = -ys; ya = -ya; }
			enemy_shot_pic(&b4_spider2, x, y + 332, FX(-20), ys, ya, DMG(10), 0);
			b4_w.delay = 30;
			b4_w.vornex = FX(5);
			if (--b4_w.cnt <= 0) b4_wall_action(e);
		}
		break;

	case W_ELEKTRO:                              /* the electric shot */
		if (--b4_w.delay < 0)
		{
			if (b4_w.cnt > 1)
			{
				Ent *s = ent_new(E_WELEKTRO, x + 220, y + 210);
				if (s) { s->active = 1; s->ys = FX(25); s->xs = FX(-3); }
				sfx(SFX_ESHOT);
			}
			b4_w.delay = 180;
			if (--b4_w.cnt <= 0) b4_wall_action(e);
		}
		break;

	case W_LASER:                                /* the big laser */
		if (--b4_w.delay < 0)
		{
			if (b4_w.cnt > 1)
			{
				Ent *s = ent_new(E_WLASER, cam_xoff() - 40, y + 240);
				if (s) { s->active = 1; b4_w.laser = (int16_t)(s - ents); }
			}
			b4_w.delay = 360;
			if (--b4_w.cnt <= 0) b4_wall_action(e);
		}
		break;

	case W_STEHEN:
		if (e->v1 == 1) b4_wall_action(e);
		break;

	case W_OEFFNEN:                              /* the doors open, then wait */
		if (b4_w.door < 1440) b4_w.door += 11;
		else
		{
			b4_w.door = 1440;
			if (--b4_w.acount < 0) e->state = W_SCHLIESSEN;
		}
		break;

	case W_SCHLIESSEN:
		b4_w.door += 19;
		if (b4_w.door > 2880)
		{
			b4_w.door = 0;
			sfx(SFX_LAND);
			e->state = W_STEHEN;
			b4_w.acount = 120;
			b4_w.oldenergy = e->energy;
			b4_w.skullenergy = 250;
			b4_wall_return_skull(e);
		}
		break;

	case W_EXPLODIEREN:
		if (--b4_w.acount < 0 && b4_w.cnt > 0)
		{
			b4_w.cnt = 0;
			stage_clear();
		}
		break;

	default:
		break;
	}
	return 1;
}
static int THEWALL_explode(Ent *e) { return 1; }
static int THEWALL_draw(const Ent *e, uint32_t t)
{
	const int x = e->x >> 8, y = e->y >> 8;
	const int toff = sin_d(b4_w.door / 16) * 100 / 256;
	/* the machine, its front, the docked skull */
	draw_pic(&P_THEWALL, 0, e->x, e->y, 0);
	draw_pic(&b4_vorne, 0, e->x + b4_w.vornex + FX(3), e->y + FX(148), 0);
	if (e->v1 == 1) draw_pic(&P_SKULL, 0, e->x + b4_w.vornex + FX(3), e->y + FX(225), 0);
	/* behind the doors: the turning tunnel, the heart */
	if (toff > 0 || e->state == W_EXPLODIEREN)
	{
		draw_pic(&b4_kringel2, b4_w.kringel / 18 % 5, FX(x + B4_TOX + 38), FX(y + B4_TOY - 14), 0);
		if (e->state != W_EXPLODIEREN)
			draw_pic(&b4_hirni, b4_w.heartanim, FX(x + B4_TOX + 116), FX(y + B4_TOY + 59), 0);
	}
	/* the doors: the left one slides in to the left, the right one to the right */
	{
		const int lx = x + B4_TOX + 10, rx = x + B4_TOX + 128 + toff, dy = y + B4_TOY - 10;
		b4_draw_clip(&b4_tuer, 0, FX(lx - toff), FX(dy), lx, lx + 138 - toff);
		b4_draw_clip(&b4_tuer, 0, FX(rx - 138), FX(dy), rx, rx + 128 - toff);
	}
	draw_pic(&b4_rahmen, 0, FX(x + B4_TOX - 8), FX(y + B4_TOY - 26), 0);
	return 1;
}

/* the wall's target: an invisible kind at the wall's place whose box is the docked skull or
 * the heart (the wall takes the energy it loses) - so a hit does not make the whole wall flicker */
static void WTARGET_load(void) { }
static void WTARGET_init(Ent *e) { }
static int WTARGET_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching) { return 1; }
static int WTARGET_explode(Ent *e) { return 1; }
static int WTARGET_draw(const Ent *e, uint32_t t) { return 1; }

/* the wall's electric shot (ELEKTROSCHUSS): falls (25), swings left and up, then down; touching
 * it costs 1000 a tenth; after 69 steps it bursts and the floor is alight */
static void WELEKTRO_load(void) { }
static void WELEKTRO_init(Ent *e) { }
static int WELEKTRO_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int n = ++e->t1;
	if (n >= 16)
	{
		e->xa = FX(-6); e->ya = FX(-5);
		if (e->xs < FX(-25)) e->xs = FX(-25);
		if (e->ys < FX(4)) e->ys = FX(4);
	}
	if (n >= 40) { e->ya = FX(7); e->xa = FX(1); }
	if (n >= 69) { e->energy = 0; return 1; }
	if (touching) player_damage(DMG_T(1000));
	b4_move(e);
	return 1;
}
static int WELEKTRO_explode(Ent *e)
{
	Ent *p = ent_new(E_WPAMPE, cam_xoff() - 60, cam_yoff() + 480 - 128);
	if (p) p->active = 1;
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int WELEKTRO_draw(const Ent *e, uint32_t t) { return 0; }

/* the electric floor (ELEKTROPAMPE): the screen's lowest 128 lines, 20 a tenth for 128 steps */
static void WPAMPE_load(void) { }
static void WPAMPE_init(Ent *e) { }
static int WPAMPE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 4, 3);
	if (++e->t1 > 128) { e->type = E_NONE; return 1; }
	if (touching) player_damage(DMG_T(20));
	return 1;
}
static int WPAMPE_explode(Ent *e) { return 1; }
static int WPAMPE_draw(const Ent *e, uint32_t t) { return 0; }

/* the big laser (SPIDERLASER) over the boxes: charges 108 steps (a thin line), then the beam
 * (500 x 200 from the screen's left) costs 500 a tenth until step 336 */
static void WLASER_load(void) { }
static void WLASER_init(Ent *e) { }
static int WLASER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (++e->t1 >= 336) { e->type = E_NONE; return 1; }
	if (e->t1 >= 108 && touching) player_damage(DMG_T(500));
	return 1;
}
static int WLASER_explode(Ent *e) { return 1; }
static int WLASER_draw(const Ent *e, uint32_t t)
{
	const int x = e->x >> 8, y = e->y >> 8;
	if (e->t1 < 108)
	{
		if (e->t1 * 2 > (int)(t & 127)) b4_box_world(x - 10, y - 4, 480, 8, b4_col_laser);
	}
	else
	{
		const int s = 210 + sin_d(e->t1 * 25) * 10 / 256;
		b4_box_world(x - 10, y - s / 2, 560, s, b4_col_laser);
		b4_box_world(x - 10, y - s / 6, 560, s / 3, b4_col_core);
	}
	return 1;
}
