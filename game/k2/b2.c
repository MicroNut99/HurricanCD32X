/*
 * Group b2: three stage bosses (src/bosses/Boss_Golem.cpp, Boss_Spinnenmaschine.cpp,
 * Boss_RiesenSpinne.cpp) and what they throw.
 *
 *  GOLEM (132, stage 4)            the lava golem: walks to and fro in its cave, throws boulders
 *                                  (temple.c's BOULDER), beats the wall so lava balls come up
 *                                  (caves.c's LAVABALL), spits fire balls, beats the lava so a
 *                                  fire column (B2SAEULE) runs along it. Leaves a platform.
 *  SPINNENMASCHINE (133, stage 5)  the spider machine: opens its lid and lets out climbing
 *                                  spiders (factory.c), drones or spider bombs (by its screen),
 *                                  then lifts its head and fires homing lasers (B2PHARAO); only
 *                                  the eye under the lifted head can be hit. Its broken lower
 *                                  part stays (B2WRACK) and a 1-up drops.
 *  RIESENSPINNE (122, stage 5)     the giant spider: walks to and fro, fires rockets (B2ROCKET),
 *                                  purple shots, a beam across the screen (B2LASER), lowers
 *                                  itself, charges, drops spider bombs; barrels (factory.c's
 *                                  LAFASSFALL, 8000 each) hurt it most. Its 100000 energy does
 *                                  not fit Ent.energy: e->energy is a buffer of 30000 that is
 *                                  read and refilled each step (b2s.energy holds the real one).
 *
 * Common leaving-outs: particles, smoke, glows (the light ray, the purple flares), screen shaking,
 * rotated drawing (the golem's body and arms, the spider's head and legs are drawn upright where
 * Hurrican's rotations / skewed quads put them), the red hit tint. Hurrican fades the music: here
 * the stage music stops when the screen locks, the boss song starts when it is in place, the
 * boss song stops when the boss starts to explode and the stage music comes back at the end.
 * Each boss's own state is in a static struct (one of each per stage). The boxes that Hurrican
 * changes as the boss moves are set in kinds[] (p1.c resets them when a stage loads).
 */

/* ------------------------------------------------------------------ shared */
#define B2_PI     804                     /* 8.8 radians */
#define B2_TWOPI  1608
#define B2_R(v)   ((int32_t)((v) * 65536))   /* 16.16 (the golem's angles: finer steps) */

/* sin / cos of 8.8 radians, x 256 */
static int b2_sin8(int rad) { return sin_d(rad * 229 / 1024); }
static int b2_cos8(int rad) { return cos_d(rad * 229 / 1024); }
/* degrees of 16.16 radians */
static int b2_deg16(int32_t rad) { return (int)(rad / 16 * 573 / 40960); }

/* the n-th picture whose name starts with the 16 letters of name (pic() finds only the first:
 * spinnenmaschine_oben/_states/_topf/_unten/_unten2 share their first 16 letters) */
static Pic b2_pic_nth(const char *name, int n)
{
	Pic p = { 0, 0, 0, 0, 0 };
	const Bank *banks[2] = { &bank_stage, &bank_res };
	for (int k = 0; k < 2; k++)
		for (int i = 0; i < banks[k]->ntex; i++)
		{
			const uint8_t *t = banks[k]->tex + 28 * i;
			if (strncmp((const char *)t, name, 16) != 0) continue;
			if (n-- > 0) continue;
			p.bank = banks[k];
			p.first = (int)rd32(t + 16);
			p.count = rd16(t + 20);
			p.fw = rd16(t + 22);
			p.fh = rd16(t + 24);
			return p;
		}
	return p;
}

static Pic b2_fireball, b2_golem_arm[2];
static Pic b2_m_states, b2_m_topf, b2_m_unten[2];
static Pic b2_s_head, b2_s_foot, b2_s_leg1, b2_s_slow, b2_s_shot, b2_s_shot2;

/* a helper thrown far off the screen is gone (only entities near the view are run) */
static int b2_far(int x, int y)
{
	const int sx = cam_x * 2, sy = cam_y * 2;
	return x < sx - 500 || x > sx + 1140 || y < sy - 500 || y > sy + 900;
}

/* the player's box meets a box at (x, y) */
static int b2_meets(int x, int y, int l, int t, int r, int b)
{
	if (pl.state == PS_DEAD) return 0;
	const Box bx = { (int16_t)l, (int16_t)t, (int16_t)r, (int16_t)b };
	return boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box, x, y, &bx);
}

static void b2_stop_music(void) { music(0); }

/* ================================================================== GOLEM (132) */
/*
 * Handlung in e->state; t1 = the screen lock asked for (Hurrican: Active and SCROLLBAR, at once),
 * t2 the explosion's effect timer. ScrolltoX / ScrolltoY = Value1 / Value2. Angles (Wackel,
 * rotarm1, rotarm2) in 16.16 radians, yoff / ShotDelay / count in 16.16 too.
 */
enum { G_NOTVIS, G_EIN, G_LAUFEN, G_LAUFEN2, G_LAUFEN3, G_LAUFEN4, G_SPECIAL2, G_SPECIAL, G_SPUCKEN, G_WARTEN,
       G_CRUSHEN, G_BOMB, G_EXPL };
enum { GA_HEBEN, GA_SENKEN, GA_SENKEN2, GA_UEBERKOPF, GA_WERFEN };
static struct { int32_t wackel, wdir, rot1, rot2, yoff, shot, count; int8_t state2, stones, last; } b2g;

static void GOLEM_load(void)
{
	b2_golem_arm[0] = pic("golemarm_hinten.png");
	b2_golem_arm[1] = pic("golemarm_vorne.png");
	b2_fireball = pic("fireball_big.png");
}
static void GOLEM_init(Ent *e)
{
	memset(&b2g, 0, sizeof b2g);
	b2g.wdir = 1;
	e->state = G_NOTVIS;
}

static void b2g_wackeln(const Ent *e)
{
	b2g.yoff = (b2g.wackel > 0 ? b2g.wackel : -b2g.wackel) * 8;
	if ((b2g.wdir > 0 && b2g.wackel > B2_R(0.1)) || (b2g.wdir < 0 && b2g.wackel < -B2_R(0.1)))
	{
		b2g.wdir = -b2g.wdir;
		b2g.wackel = -b2g.wdir * B2_R(0.1);
		if (e->state >= G_EIN && e->state <= G_LAUFEN4) sfx(SFX_LAND);    /* a step (DOORSTOP) */
	}
}
static int b2g_zero(int32_t *v, int32_t speed)
{
	int done = 1;
	if (*v > 0) { done = 0; *v -= SYNC(speed); if (*v < 0) *v = 0; }
	if (*v < 0) { done = 0; *v += SYNC(speed); if (*v > 0) *v = 0; }
	return done;
}
static void b2g_walk(Ent *e, int dx)
{
	e->x += SYNC(FX(dx));
	b2g.wackel += SYNC(b2g.wdir * B2_R(0.025));
	b2g.rot1 -= SYNC(b2g.wdir * B2_R(0.2));
	b2g.rot2 += SYNC(b2g.wdir * B2_R(0.2));
	b2g_wackeln(e);
}
/* a lava ball out of the lava at the screen's bottom (Gegner_LavaBall: speed Value1) */
static void b2g_lavaball(int x)
{
	const int y = cam_yoff() + 480;
	Ent *b = ent_new(E_LAVABALL, x, y);
	if (!b) return;
	b->active = 1;
	b->ys = (int16_t)FX(-(50 + rnd(10)));
	b->ya = FX(4);
	b->v1 = (int16_t)y;
}
static void b2g_boulder(int x, int y, int v1, int v2)
{
	Ent *b = ent_new(E_BOULDER, x, y);
	if (!b) return;
	b->active = 1;
	b->v1 = (int16_t)v1;
	b->v2 = (int16_t)v2;
	BOULDER_init(b);
}

static int GOLEM_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int sx = e->v1;
	const int32_t ex = e->x;
	e->hit = 0;
	if (e->state != G_NOTVIS && e->state != G_EXPL) boss_bar(e->energy, 8000);

	/* seen: hidden 400 to the right, the screen goes to the cave, the stage music stops */
	if (!e->t1)
	{
		e->t1 = 1;
		e->x += FX(400);
		boss_lock(e->v1, e->v2, 10);
		b2_stop_music();
	}

	if (e->energy <= 100 && e->state != G_EXPL)
	{
		e->state = G_EXPL;
		e->t2 = 0;
		b2g.shot = B2_R(30);
		b2_stop_music();
	}

	switch (e->state)
	{
	case G_NOTVIS:                      /* waits until the screen is in place */
		if (boss_locked_there())
		{
			music_boss();
			e->state = G_EIN;
			sfx(SFX_BIGEXPLODE);            /* its roar (MUTANT) */
		}
		break;

	case G_EIN:                         /* walks in slowly to the left */
		e->energy = 8000;
		/* fall through */
	case G_LAUFEN:                      /* to the middle */
		b2g_walk(e, -10);
		if (e->x <= FX(sx + 380)) { e->x = FX(sx + 380); e->state = G_SPECIAL; }
		break;

	case G_LAUFEN2:                     /* to the right */
		b2g_walk(e, 10);
		if (e->x >= FX(sx + 540)) { e->x = FX(sx + 540); e->state = G_SPECIAL; }
		break;

	case G_LAUFEN3:                     /* all the way left to the wall */
		b2g_walk(e, -10);
		if (e->x <= FX(sx + 100))
		{
			e->x = FX(sx + 100);
			e->state = G_SPECIAL2;
			b2g.last = G_SPECIAL2;
			b2g.state2 = GA_SENKEN;
			b2g.stones = (int8_t)(3 + rnd(2));
		}
		break;

	case G_LAUFEN4:                     /* from the left to the middle */
		b2g_walk(e, 10);
		if (e->x >= FX(sx + 380)) { e->x = FX(sx + 380); e->state = G_SPECIAL; }
		break;

	case G_SPECIAL2:                    /* beats the wall: lava balls come up */
		if (b2g.wackel > -B2_R(0.2)) b2g.wackel -= SYNC(B2_R(0.03)); else b2g.wackel = -B2_R(0.2);
		if (b2g.state2 == GA_SENKEN)
		{
			b2g.rot1 -= SYNC(B2_R(0.25));
			b2g.rot2 += SYNC(B2_R(0.25));
			if (b2g.rot2 > B2_R(1))
			{
				b2g.state2 = GA_HEBEN;
				sfx(SFX_LAND);
				b2g_lavaball(cam_xoff() + 100 + rnd(540));
			}
		}
		else
		{
			b2g.rot1 += SYNC(B2_R(0.25));
			b2g.rot2 -= SYNC(B2_R(0.25));
			if (b2g.rot1 > B2_R(1))
			{
				b2g.state2 = GA_SENKEN;
				sfx(SFX_LAND);
				b2g_lavaball((pl.x >> 8) - 20 + rnd(70));
				if (--b2g.stones <= 0) e->state = G_SPECIAL;
			}
		}
		break;

	case G_SPECIAL:                     /* back to the start position, then the next action */
	{
		b2g_wackeln(e);
		const int f0 = b2g_zero(&b2g.wackel, B2_R(0.1));
		const int f1 = b2g_zero(&b2g.rot1, B2_R(0.1));
		const int f2 = b2g_zero(&b2g.rot2, B2_R(0.1));
		if (f0 && f1 && f2)
		{
			if (e->x > FX(sx + 400))                     /* on the right */
			{
				if (rnd(2) == 0)                            /* throws boulders */
				{
					e->state = G_CRUSHEN;
					b2g.last = G_CRUSHEN;
					b2g.state2 = GA_SENKEN;
					b2g.stones = (int8_t)(rnd(2) + 2);
				}
				else                                        /* or beats the lava */
				{
					e->state = G_BOMB;
					b2g.state2 = GA_HEBEN;
					b2g.count = B2_R(rnd(3) + 2);
				}
			}
			else if (e->x > FX(sx + 120))                /* in the middle */
			{
				if (rnd(3) > 0) e->state = b2g.last == G_SPECIAL2 ? G_LAUFEN2 : G_LAUFEN3;
				else
				{
					e->state = G_SPUCKEN;
					b2g.shot = B2_R(80);
					b2g.count = B2_R(4 + rnd(4));
					b2g.wackel = 0;
				}
			}
			else e->state = G_LAUFEN4;                  /* on the left: back to the middle */
		}
	}
	break;

	case G_SPUCKEN:                     /* fire balls at the player */
		if (b2g.shot > B2_R(10))
		{
			b2g.wackel += SYNC(B2_R(0.03));
			b2g.rot1 -= SYNC(B2_R(0.054));
			b2g.rot2 -= SYNC(B2_R(0.054));
		}
		else
		{
			b2g.wackel -= SYNC(B2_R(0.2));
			b2g.rot1 += SYNC(B2_R(0.4));
			b2g.rot2 += SYNC(B2_R(0.4));
		}
		b2g.shot -= SYNC(B2_R(10));
		if (b2g.shot < 0)
		{
			/* FIREBALL_BIG at WinkelToPlayer() + 180, speed 50: towards the player when he is to
			 * the left (atan's angle), away from him when he is to the right, as in Hurrican */
			const int yo = b2g.yoff >> 16;
			fx xs, ys;
			aim_at(x + 50, y + yo + 50, 50, &xs, &ys);
			if (pcx() > x + 50) { xs = -xs; ys = -ys; }
			enemy_shot_pic(&b2_fireball, x + 35, y + yo + 35, xs, ys, 0, DMG(30), 0);
			b2g.shot = B2_R(80);
			b2g.count -= B2_R(1);
			if (b2g.count <= 0) { e->state = G_WARTEN; b2g.shot = B2_R(10); }
			b2g.wackel = b2g.rot1 = b2g.rot2 = 0;
		}
		break;

	case G_WARTEN:                      /* a short wait after shooting */
		b2g.shot -= SYNC(B2_R(1));
		if (b2g.shot <= 0) e->state = G_SPECIAL;
		break;

	case G_CRUSHEN:                     /* fetches a boulder and throws it */
		switch (b2g.state2)
		{
		case GA_SENKEN:                     /* down for a boulder */
			b2g.wackel -= SYNC(B2_R(0.05));
			b2g.rot1 += SYNC(B2_R(0.1));
			b2g.yoff += SYNC(B2_R(5));
			if (b2g.wackel < -B2_R(0.5)) b2g.state2 = GA_HEBEN;
			break;
		case GA_HEBEN:                      /* up again */
			b2g.yoff -= SYNC(B2_R(5));
			b2g.wackel += SYNC(B2_R(0.05));
			b2g.rot1 -= SYNC(B2_R(0.1));
			if (b2g.wackel >= 0) { b2g.state2 = GA_UEBERKOPF; b2g.wackel = b2g.rot1 = b2g.yoff = 0; }
			break;
		case GA_UEBERKOPF:                  /* the boulder back over the head */
			b2g.wackel += SYNC(B2_R(0.04));
			b2g.rot1 += SYNC(B2_R(0.4));
			if (b2g.wackel >= B2_R(0.35)) b2g.state2 = GA_WERFEN;
			break;
		case GA_WERFEN:                     /* throws */
			if (b2g.wackel > 0) b2g.wackel -= SYNC(B2_R(0.1)); else b2g.wackel = 0;
			b2g.rot1 -= SYNC(B2_R(0.6));
			if (b2g.rot1 <= B2_R(2))
			{
				b2g_boulder(x - 40, y + 30, -(rnd(80) + 10), -(rnd(20) + 10));
				b2g_boulder(x - 40, y + 30, -(rnd(40) + 10), -(rnd(10) + 10));
				sfx(SFX_EXPLODE);               /* STONEFALL */
				b2g.state2 = GA_SENKEN2;
			}
			break;
		default:                            /* back to the throwing position */
		{
			b2g_wackeln(e);
			const int f0 = b2g_zero(&b2g.wackel, B2_R(0.5));
			const int f1 = b2g_zero(&b2g.rot1, B2_R(0.5));
			const int f2 = b2g_zero(&b2g.rot2, B2_R(0.5));
			if (f0 && f1 && f2)
			{
				if (--b2g.stones == 0) e->state = G_LAUFEN;
				else b2g.state2 = GA_SENKEN;
			}
		}
		break;
		}
		break;

	case G_BOMB:                        /* beats the lava: a fire column runs along it */
		if (b2g.state2 == GA_HEBEN)
		{
			b2g.rot1 += SYNC(B2_R(0.3));
			b2g.rot2 += SYNC(B2_R(0.2));
			if (b2g.rot1 > B2_R(3)) { b2g.rot1 = B2_R(3); b2g.state2 = GA_SENKEN; }
		}
		else if (b2g.state2 == GA_SENKEN)
		{
			b2g.rot1 -= SYNC(B2_R(0.8));
			b2g.rot2 -= SYNC(B2_R(0.7));
			if (b2g.rot1 < B2_R(0.7))
			{
				b2g.rot1 = b2g.rot2 = B2_R(0.7);
				b2g.state2 = GA_SENKEN2;
				sfx(SFX_LAND);                  /* WATERIN */
				Ent *s = ent_new(E_B2SAEULE, x - 90, y + 300);
				if (s) { s->active = 1; s->xs = (int16_t)FX(-(rnd(10) + 5)); }
			}
		}
		else
		{
			b2g.rot1 += SYNC(B2_R(0.25));
			b2g.rot2 += SYNC(B2_R(0.25));
			if (b2g.rot1 > B2_R(1.5))
			{
				b2g.rot1 = b2g.rot2 = B2_R(1.5);
				b2g.count -= B2_R(1);
				if (b2g.count < 0) e->state = G_SPECIAL;
				else b2g.state2 = GA_HEBEN;
			}
		}
		break;

	case G_EXPL:                        /* bursts for 3 seconds, then explodes */
		e->energy = 100;
		if (++e->t2 >= 8)
		{
			e->t2 = 0;
			fx_add(&P_expl, x + rnd(160), y + rnd(300) + (b2g.yoff >> 16), 2);
			sfx(SFX_EXPLODE);
		}
		b2g.shot -= SYNC(B2_R(1));
		if (b2g.shot <= 0) e->energy = 0;
		break;
	}

	/* the box (Hurrican sets it as it draws): wider as it bends */
	kinds[E_GOLEM].box.r = (int16_t)(138 + (b2g.yoff >> 16));
	if (e->state != G_EXPL && e->state != G_NOTVIS && touching) player_damage(DMG_T(5));
	e->xo = ex;
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu;
	return 1;
}
static int GOLEM_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 40, y + 80, 2);
	fx_add(&P_explbig, x + 100, y + 160, 3);
	fx_add(&P_explbig, x + 50, y + 240, 2);
	fx_add(&P_expl, x + 120, y + 60, 3);
	sfx(SFX_BIGEXPLODE);
	/* a platform, so the player gets on */
	Ent *p = ent_new(E_PLATTFORM, 6760, 2500);
	if (p) { p->active = 1; p->v1 = 6; p->xs = FX(6); }
	boss_unlock();
	music_stage();
	return 1;
}

/* p's (8.8 full-size) centre (cx, cy) turned by deg around (px, py) and moved to (qx, qy):
 * returns the move against the same at deg0 (the picture is drawn upright, moved by that) */
static void b2g_turn(int cx, int cy, int px, int py, int qx, int qy, int deg, int deg0, int *dx, int *dy)
{
	const int ax = cx - px, ay = cy - py;
	const int c = cos_d(deg), s = sin_d(deg), c0 = cos_d(deg0), s0 = sin_d(deg0);
	*dx = (ax * c - ay * s - (ax * c0 - ay * s0)) / 256;
	*dy = (ax * s + ay * c - (ax * s0 + ay * c0)) / 256;
	(void)qx; (void)qy;
}
static int GOLEM_draw(const Ent *e, uint32_t t)
{
	if (e->state == G_NOTVIS) return 1;
	const int x = (e->x >> 8) + (e->state == G_EXPL ? (int)(t & 1) * 2 : 0);
	const int y = (e->y >> 8) + (b2g.yoff >> 16);
	const int w = b2_deg16(b2g.wackel);
	int dx, dy, bx, by;
	/* the whole turns by Wackel around the body's middle (x + 48, y + 215) */
	b2g_turn(92, 145, 48, 215, 0, 0, w, 0, &bx, &by);
	/* the arm behind: turned by rotarm2 - 0.5 around its shoulder (150, 30) */
	b2g_turn(112, 120, 150, 30, 0, 0, b2_deg16(b2g.rot2 - B2_R(0.5)), -29, &dx, &dy);
	draw_pic(&b2_golem_arm[0], 0, FX(x + dx + bx), FX(y + dy + by), 0);
	draw_pic(&P_GOLEM, 0, FX(x + bx), FX(y + by), 0);
	/* the arm in front: turned by rotarm1 - 0.3 around its shoulder (125, 70) */
	b2g_turn(102, 133, 125, 70, 0, 0, b2_deg16(b2g.rot1 - B2_R(0.3)), -17, &dx, &dy);
	draw_pic(&b2_golem_arm[1], 0, FX(x + dx + bx), FX(y + dy + by), 0);
	return 1;
}

/* ------------------------------------------------------------------ B2SAEULE: the golem's fire column (GOLEMSAEULE) */
/*
 * Runs left along the lava (speed 5-14), burning (1 a unit); at once and every 13 units it sends
 * up a column of 25 flames (FEUERFALLE: 5 apart, rising at 20, about 100 steps): here one column
 * box rising from where it was sent (v1 its x, t1 the steps since), 1 a unit for each of the
 * flames the player is in (about 20), drawn as 3 flames. Gone off the screen.
 */
static void B2SAEULE_load(void) { }
static void B2SAEULE_init(Ent *e) { (void)e; }
static int B2SAEULE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (--e->t2 <= 0)
	{
		e->t2 = 78;
		e->t1 = 0;
		e->v1 = (int16_t)x;
		sfx(SFX_ESHOT);                  /* FEUERFALLE */
	}
	if (e->t1 < 100)
	{
		e->t1++;
		const int rise = e->t1 * 20 / 6;
		if (b2_meets(e->v1 + 5, y - 40 - rise, 30, 30, 54, 190)) player_damage(DMG_T(20));
	}
	if (touching) player_damage(DMG_T(1));
	if (b2_far(x, y) || x + 84 < cam_x * 2) { e->type = E_NONE; return 1; }
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu;
	return 0;
}
static int B2SAEULE_explode(Ent *e) { (void)e; return 1; }
static int B2SAEULE_draw(const Ent *e, uint32_t t)
{
	draw_pic(&P_B2SAEULE, (int)(t / 3 % 6), e->x, e->y, 0);
	if (e->t1 < 100)
	{
		const int rise = e->t1 * 20 / 6, f = e->t1 * 29 / 100;
		for (int i = 0; i < 3; i++)
			draw_pic(&P_B2SAEULE, f, FX(e->v1 + 5), e->y - FX(40 + rise - i * 60), 0);
	}
	return 1;
}

/* ================================================================== SPINNENMASCHINE (133) */
/*
 * Handlung in e->state; t1 = the screen lock asked for. The lid (DoDeckel) and the head (DoHoch)
 * each go shut - opening - open - closing; their counters here in steps (Hurrican's units x 6).
 * DeckelOffset = 20 - 20 cos(DeckelCount): dcount 0..94 steps for 0..PI. The whole is drawn
 * mirrored, as Hurrican does. The player cannot get past x + 250.
 */
enum { M_INIT, M_EIN, M_STEHEN, M_LAUFEN, M_EXPL };
enum { MD_ZU, MD_OFFEN, MD_OEFFNEN, MD_SCHLIESSEN };
static struct { int16_t disp, olddisp, phase, open_c, hoch_c, animc, dcount, shot, spawn, death; uint8_t deckel, hoch, fertig; } b2m;

static int b2m_off(void) { return 20 - 20 * cos_d(b2m.dcount * 180 / 94) / 256; }

static void SPINNENMASCHINE_load(void)
{
	b2_m_states = b2_pic_nth("spinnenmaschine_", 1);
	b2_m_topf = b2_pic_nth("spinnenmaschine_", 2);
	b2_m_unten[0] = b2_pic_nth("spinnenmaschine_", 3);
	b2_m_unten[1] = b2_pic_nth("spinnenmaschine_", 4);
}
static void SPINNENMASCHINE_init(Ent *e)
{
	memset(&b2m, 0, sizeof b2m);
	b2m.open_c = 60;
	b2m.hoch_c = 180;
	b2m.shot = 30;
	b2m.spawn = 48;
	e->state = M_INIT;
}

static void b2m_spawn(int x, int y)
{
	const int off = b2m_off();
	Ent *n;
	switch (b2m.disp)
	{
	case 1:                              /* climbing spiders */
		b2m.spawn = 36;
		n = ent_new(E_CLIMBSPIDER, x + 100 + rnd(60), y + 190 - off);
		if (n) { n->active = 1; n->v1 = 99; CLIMBSPIDER_init(n); }
		break;
	case 2:                              /* drones */
		b2m.spawn = 120;
		n = ent_new(E_DRONE, x + 135, y + 190 - off);
		if (n) n->active = 1;
		break;
	case 3:                              /* spider bombs */
		b2m.spawn = 90;
		n = ent_new(E_SPIDERBOMB, x + 100 + rnd(80), y + 180 - off);
		if (n) { n->active = 1; n->ya = FX(5); }
		break;
	}
}
static void b2m_deckel(int x, int y)
{
	switch (b2m.deckel)
	{
	case MD_ZU:
		if (--b2m.open_c < 0) { b2m.open_c = 0; b2m.deckel = MD_OEFFNEN; b2m.animc = 0; }
		break;
	case MD_OEFFNEN:
		if (++b2m.animc > 4) { b2m.animc = 0; b2m.phase++; }
		if (b2m.phase > 10) { b2m.phase = 10; b2m.deckel = MD_OFFEN; b2m.open_c = 240; }
		break;
	case MD_OFFEN:
		if (--b2m.open_c < 0) { b2m.open_c = 0; b2m.deckel = MD_SCHLIESSEN; b2m.animc = 0; }
		if (--b2m.spawn < 0) b2m_spawn(x, y);
		break;
	default:
		if (++b2m.animc > 4) { b2m.animc = 0; b2m.phase--; }
		if (b2m.phase < 0) { b2m.phase = 0; b2m.deckel = MD_ZU; b2m.open_c = 60; b2m.fertig = 1; }
		break;
	}
}
static void b2m_hoch(int x, int y)
{
	switch (b2m.hoch)
	{
	case MD_ZU:
		if (--b2m.hoch_c < 0) { b2m.hoch_c = 0; b2m.hoch = MD_OEFFNEN; }
		break;
	case MD_OEFFNEN:
		if (++b2m.dcount >= 94) { b2m.dcount = 94; b2m.hoch = MD_OFFEN; b2m.hoch_c = 360; b2m.shot = 60; sfx(SFX_LAND); }
		break;
	case MD_OFFEN:
		if (--b2m.hoch_c < 0) { b2m.hoch_c = 0; b2m.hoch = MD_SCHLIESSEN; }
		if (--b2m.shot <= 0)
		{
			/* PHARAOLASER at the player */
			b2m.shot = 90;
			Ent *s = ent_new(E_B2PHARAO, x + 230, y + 310);
			if (s)
			{
				fx xs, ys;
				s->active = 1;
				aim_at(x + 254, y + 334, 40, &xs, &ys);
				s->xs = (int16_t)xs; s->ys = (int16_t)ys;
				sfx(SFX_ESHOT);
			}
		}
		break;
	default:
		if (--b2m.dcount <= 0) { b2m.dcount = 0; b2m.hoch = MD_ZU; b2m.hoch_c = 180; b2m.fertig = 1; sfx(SFX_LAND); }
		break;
	}
}

static int SPINNENMASCHINE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->hit = 0;
	if (e->state != M_INIT && e->state != M_EXPL) boss_bar(e->energy, 4000);
	if (!e->t1 && abs(pcx() - x) < 450)
	{
		e->t1 = 1;
		boss_lock(e->v1, e->v2, 10);
		b2_stop_music();
	}
	if (e->energy <= 100 && e->state != M_EXPL)
	{
		e->state = M_EXPL;
		b2m.death = 240;
		Ent *o = ent_new(E_ONEUP, 140, 820);
		if (o) { o->active = 1; o->ya = FX(2); }
		b2_stop_music();
	}
	switch (e->state)
	{
	case M_INIT:
		if (boss_locked_there()) { music_boss(); e->state = M_EIN; }
		break;
	case M_EIN:
		e->energy = 4000;
		b2m.disp = b2m.olddisp = (int16_t)(rnd(3) + 1);
		e->state = M_LAUFEN;
		break;
	case M_STEHEN:                       /* the head up and down */
		b2m_hoch(x, y);
		if (b2m.fertig)
		{
			b2m.fertig = 0;
			e->state = M_LAUFEN;
			do b2m.disp = (int16_t)(rnd(3) + 1); while (b2m.disp == b2m.olddisp);
			b2m.olddisp = b2m.disp;
		}
		break;
	case M_LAUFEN:                       /* the lid open and shut */
		b2m_deckel(x, y);
		if (b2m.fertig) { b2m.fertig = 0; e->state = M_STEHEN; b2m.disp = 0; }
		break;
	default:                             /* bursts for 4 seconds; the lower part stays */
		e->energy = 100;
		if (b2m.death % 8 == 0)
		{
			fx_add(&P_expl, x + rnd(300), y + rnd(400), 2);
			if (rnd(3) == 0) sfx(SFX_EXPLODE);
		}
		if (--b2m.death < 0)
		{
			Ent *w = ent_new(E_B2WRACK, x + 45, y + 352);
			if (w) w->active = 1;
			e->energy = 0;
		}
		break;
	}
	/* only the eye under the lifted head can be hit */
	{
		Box *b = &kinds[E_SPINNENMASCHINE].box;
		if (b2m.hoch == MD_ZU) { b->l = b->t = b->r = b->b = 0; }
		else { b->l = 204; b->t = (int16_t)(350 - b2m_off()); b->r = 251; b->b = 350; }
	}
	/* the player cannot get past it */
	if (pl.x < e->x + FX(250)) pl.x = e->x + FX(250);
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	return 1;
}
static int SPINNENMASCHINE_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 100, y + 200, 2);
	fx_add(&P_explbig, x + 220, y + 260, 3);
	fx_add(&P_explbig, x + 150, y + 330, 2);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int SPINNENMASCHINE_draw(const Ent *e, uint32_t t)
{
	const int32_t x = e->x, y = e->y - FX(b2m_off());
	draw_pic(&P_SPINNENMASCHINE, 0, x, y, 1);
	draw_pic(&b2_m_states, b2m.disp, x + FX(133), y + FX(263), 1);
	draw_pic(&b2_m_topf, b2m.phase, x + FX(75), y + FX(159), 1);
	draw_pic(&b2_m_unten[0], 0, e->x + FX(45), e->y + FX(352), 1);
	(void)t;
	return 1;
}

/* ------------------------------------------------------------------ B2WRACK: the machine's broken lower part */
static void B2WRACK_load(void) { }
static void B2WRACK_init(Ent *e) { (void)e; }
static int B2WRACK_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)e; (void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	return 1;
}
static int B2WRACK_explode(Ent *e) { (void)e; return 1; }
static int B2WRACK_draw(const Ent *e, uint32_t t) { (void)t; draw_pic(&b2_m_unten[1], 0, e->x, e->y, 1); return 1; }

/* ------------------------------------------------------------------ B2PHARAO: the machine's laser ball (PHARAOLASER) */
/*
 * Flies at the player (40), follows his height (5, at most 20), bounces off the floor, bursts on
 * walls; hitting him costs 40. Left out: the smoke trail.
 */
static void B2PHARAO_load(void) { }
static void B2PHARAO_init(Ent *e) { (void)e; }
static int B2PHARAO_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 3, 3);
	if (e->ys > FX(20)) e->ys = FX(20);
	if (e->ys < FX(-20)) e->ys = FX(-20);
	e->ya = (pl.y >> 8) + 40 < y + 24 ? FX(-5) : FX(5);
	if ((bu & B_WALL) && e->ys > 0) e->ys = (int16_t)-e->ys;
	if ((bl | br | bo) & B_WALL) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(40)); e->energy = 0; return 1; }
	if (++e->t1 > 600 || b2_far(x, y)) { e->type = E_NONE; return 1; }
	(void)idx;
	return 0;
}
static int B2PHARAO_explode(Ent *e) { fx_add(&P_expl, ecx(e), ecy(e), 2); sfx(SFX_EXPLODE); return 1; }
static int B2PHARAO_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================== RIESENSPINNE (122) */
/*
 * Handlung in e->state; t1 = the screen lock asked for, t2 = the beam's entity + 1.
 * In b2s (8.8 fixed): HeadWinkel (degrees), HeadXOffset, WalkCount, DamageWackel (radians),
 * WalkDir (its x speed, units), ShotDelay, AnimCount (units), yBody (pixels).
 * Drawing: the body, the head (not turned), the four legs in front of the body (upper part and
 * foot, upright in the middle of Hurrican's skewed quads); the legs behind and the lower leg
 * parts (mostly hidden) are left out.
 */
enum { S_NOTVIS, S_EIN, S_LINKS, S_RECHTS, S_CRUSHEN, S_EXPL, S_SCHIESSEN, S_SPECIAL, S_SPECIAL2,
       S_AB, S_AB2, S_AUF };
#define B2S_BUF 30000
static struct { int32_t energy, animc; int16_t count, mode, headw, headx, walkdir, walkc, dwack, shot, ybody, last, headdir, nbomb, bombs[8]; } b2s;

static void RIESENSPINNE_load(void)
{
	b2_s_head = pic("spiderboss_kopf.png");
	b2_s_foot = pic("spiderboss_foot.png");
	b2_s_leg1 = pic("spiderboss_leg1.png");
	b2_s_slow = pic("spiderslow.png");
	b2_s_shot = pic("spidershot.png");
	b2_s_shot2 = pic("spidershot2.png");
}
static void RIESENSPINNE_init(Ent *e)
{
	memset(&b2s, 0, sizeof b2s);
	b2s.energy = 100000;
	b2s.walkdir = FX(-5);
	b2s.last = -1;
	b2s.headdir = FX(1);
	b2s.headx = B2_PI;
	e->state = S_NOTVIS;
	e->energy = B2S_BUF;
}

/* the spider bombs it drops are kept in a ring of 8: P1's spider bomb (p1.c) only crawls while
 * the player is within 400 (Hurrican's always runs at him), so bombs left behind would pile up;
 * the 9th one back is taken away when it is still there, out of the player's reach */
static void b2s_bomb_slot(void)
{
	const int k = b2s.bombs[b2s.nbomb];
	if (k)
	{
		Ent *o = &ents[k - 1];
		if (o->type == E_SPIDERBOMB && player_distance(o) >= 400) o->type = E_NONE;
	}
}
static void b2s_links(Ent *e)
{
	e->state = S_LINKS;
	b2s.mode = (int16_t)rnd(2);
	b2s.shot = FX(5);
}
static void b2s_shot2(int x, int y)
{
	int ys = -(rnd(12) + 5), ya = FX(rnd(10) + 5) / 30;
	if (rnd(2) == 0) { ys = -ys; ya = -ya; }
	enemy_shot_pic(&b2_s_shot2, x, y, FX(-20), FX(ys), ya, DMG(10), 0);
}
/* RandomHandlung: a new action (not the last one) when it stands on the right */
static void b2s_random(Ent *e, int y)
{
	int j;
	b2s.shot = FX(4);
	do j = rnd(6); while (j == b2s.last);
	b2s.last = (int16_t)j;
	switch (j)
	{
	case 0: b2s.count = (int16_t)(10 + rnd(10)); e->state = S_SCHIESSEN; break;
	case 1: e->state = S_AB; break;
	case 2: b2s.count = (int16_t)(20 + rnd(10)); e->state = S_SPECIAL; break;
	case 3: e->state = S_AB2; break;
	case 4: e->state = S_LINKS; b2s.shot = FX(1); b2s.mode = (int16_t)rnd(2); break;
	default:
	{
		e->state = S_SPECIAL2;
		Ent *l = ent_new(E_B2LASER, e->v1, y + (b2s.ybody >> 8) + 110);
		if (l) { l->active = 1; e->t2 = (int16_t)(l - ents + 1); }
		sfx(SFX_ESHOT);
		b2s.animc = FX(140);
		b2s.shot = 0;
	}
	break;
	}
}

static int RIESENSPINNE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int v1 = e->v1;
	e->hit = 0;
	/* the damage since the last step (shots, barrels) from the buffer */
	const int dmg = B2S_BUF - e->energy;
	e->energy = B2S_BUF;
	b2s.energy -= dmg;
	if (e->state != S_NOTVIS && e->state != S_EXPL) boss_bar((int)(b2s.energy > 0 ? b2s.energy : 0), 100000);

	if (!e->t1)
	{
		e->t1 = 1;
		boss_lock(e->v1, e->v2, 10);
		b2_stop_music();
	}

	switch (e->state)
	{
	case S_NOTVIS:
		if (boss_locked_there()) { music_boss(); e->state = S_EIN; }
		break;
	case S_EIN:                          /* steps into the level */
		e->y -= FX(55);
		b2s.energy = 100000;
		sfx(SFX_BIGEXPLODE);                 /* SPIDERSCREAM */
		e->state = S_LINKS;
		b2s.shot = FX(1);
		b2s.mode = (int16_t)rnd(2);
		break;

	case S_CRUSHEN:
	case S_LINKS:
	case S_RECHTS:
	case S_EXPL:
		if (b2s.headw > 0) b2s.headw -= SYNC(FX(1));
		if (b2s.headw < 0) b2s.headw += SYNC(FX(1));
		e->x += SYNC(b2s.walkdir);
		b2s.walkc -= SYNC(b2s.walkdir / 10);
		if (b2s.walkc > B2_TWOPI) b2s.walkc -= B2_TWOPI;
		if (b2s.walkc < 0) b2s.walkc += B2_TWOPI;

		if (e->state == S_CRUSHEN)
		{
			if (b2s.headw < FX(45)) b2s.headw += SYNC(FX(1));
			b2s.walkdir -= SYNC(FX(10));
			if (b2s.walkdir < FX(-35)) b2s.walkdir = FX(-35);
			if (e->x < FX(v1 - 50)) { e->state = S_RECHTS; b2s.shot = FX(15); }
			/* pushes the player away (and hurts: 50 a unit) */
			const int yb = b2s.ybody >> 8;
			if (b2_meets(x, y, 50, 40 + yb, 120, 140 + yb))
			{
				const int px = pl.x >> 8, py = pl.y >> 8;
				player_damage(DMG_T(50));
				if (px + 35 < x + 85) { if (!(blk_left(px, py, px, &player_box, 0) & B_WALL)) pl.x -= SYNC(FX(21)); }
				else if (!(blk_right(px, py, px, &player_box, 0) & B_WALL)) pl.x += SYNC(FX(21));
			}
		}
		if (e->state == S_LINKS)
		{
			b2s.walkdir -= SYNC(FX(1));
			if (b2s.walkdir < FX(-8)) b2s.walkdir = FX(-8);
			if (e->x < FX(v1 + 100)) { e->state = S_RECHTS; b2s.shot = FX(15); }
			if (b2s.energy < 75000)
			{
				b2s.shot -= SYNC(FX(1));
				if (b2s.dwack > 0) b2s.shot = FX(15);
				if (b2s.shot < 0)
				{
					const int sy = y + (b2s.ybody >> 8) + 85;
					b2s.shot = FX(12);
					if (b2s.mode == 0)
					{
						fx xs, ys;
						aim_at(x + 50, sy + 20, 20, &xs, &ys);
						enemy_shot_pic(&b2_s_slow, x + 30, sy, xs, ys, 0, DMG(30), 0);
					}
					else
						for (int i = 0; i < 5; i++)
						{
							const int w = 212 - i * 15 - 90;
							enemy_shot_pic(&b2_s_shot, x, sy, -sin_d(w) * 25, cos_d(w) * 25, 0, DMG(10), i > 0);
						}
				}
			}
		}
		if (e->state == S_RECHTS || e->state == S_EXPL)
		{
			b2s.walkdir += SYNC(FX(1));
			if (b2s.ybody > 0) b2s.ybody -= SYNC(FX(1));
			if (e->state == S_RECHTS)
			{
				b2s.shot -= SYNC(FX(1));
				if (b2s.shot < 0)
				{
					/* spider bombs out of its back */
					b2s.shot = FX(15);
					b2s_bomb_slot();
					Ent *b = ent_new(E_SPIDERBOMB, x + 400, y + 100 + (b2s.ybody >> 8));
					if (b) { b->active = 1; b->ya = FX(5); b2s.bombs[b2s.nbomb] = (int16_t)(b - ents + 1); b2s.nbomb = (int16_t)((b2s.nbomb + 1) & 7); }
				}
			}
			if (e->x < FX(v1 - 550)) b2s.walkdir = FX(30);
			else if (e->state == S_RECHTS) { if (b2s.walkdir > FX(8)) b2s.walkdir = FX(8); }
			else if (b2s.walkdir > FX(4)) b2s.walkdir = FX(4);
			if (e->state == S_RECHTS && e->x > FX(v1 + 400)) b2s_random(e, y);
		}
		if (e->state == S_EXPL)
		{
			b2s.animc -= SYNC(FX(1));
			if (b2s.animc < 0)
			{
				b2s.animc = FX(1.5);
				fx_add(rnd(2) ? &P_expl : &P_explbig, x + rnd(400), y + rnd(100), 2);
				if (rnd(3) == 0) sfx(SFX_EXPLODE);
			}
		}
		break;

	case S_SCHIESSEN:                    /* rockets */
		b2s.shot -= SYNC(FX(1));
		if (b2s.shot < 0)
		{
			b2s.shot = FX(4);
			Ent *r = ent_new(E_B2ROCKET, x + 35 - 25, y + 95 - 25);
			if (r)
			{
				const int w = rnd(60) - 130;          /* degrees, Hurrican's Winkel */
				r->active = 1;
				r->v1 = (int16_t)w;
				r->xs = (int16_t)(sin_d(w) * 35);
				r->ys = (int16_t)(-cos_d(w) * 35);
				r->anim = (int16_t)(((-w + 22) / 45) & 7);
				sfx(SFX_ESHOT);
			}
			b2s.headx = B2_TWOPI;
			if (--b2s.count == 0 || (pl.x >> 8) > x - 50) b2s_links(e);
		}
		break;

	case S_SPECIAL:                      /* turns the head and fires */
		b2s.headw += SYNC(b2s.headdir);
		if (b2s.headw > FX(15)) b2s.headdir = FX(-1);
		if (b2s.headw < FX(-10)) b2s.headdir = FX(1);
		b2s.shot -= SYNC(FX(1));
		if (b2s.shot < 0)
		{
			const int hw = b2s.headw >> 8, w = 180 - hw * 3 / 2 - 90;
			b2s.shot = FX(3.5);
			enemy_shot_pic(&b2_s_shot, x - cos_d(hw) * 10 / 256, y + sin_d(hw) * 140 / 256 + 90 + (b2s.ybody >> 8),
			               -sin_d(w) * 50, cos_d(w) * 50, 0, DMG(15), 0);
			b2s.headx = B2_TWOPI;
			if (--b2s.count == 0) b2s_links(e);
		}
		break;

	case S_SPECIAL2:                     /* the beam (B2LASER) */
		b2s.animc -= SYNC(FX(2.5));
		if (b2s.animc < 0) b2s_links(e);
		break;

	case S_AB:                           /* lowers itself, firing */
	case S_AUF:                          /* up again, firing */
		if (e->state == S_AB) { b2s.ybody += SYNC(FX(1)); if (b2s.ybody > FX(50)) e->state = S_AUF; }
		else
		{
			b2s.ybody -= SYNC(FX(1));
			if (b2s.ybody <= 0) { b2s.ybody = 0; b2s_links(e); }
		}
		b2s.shot -= SYNC(FX(1));
		if (b2s.shot < 0)
		{
			b2s.shot = FX(10);
			b2s_shot2(x + 20, y + 80 + (b2s.ybody >> 8));
			b2s_shot2(x + 20, y + 100 + (b2s.ybody >> 8));
			b2s.headx = B2_PI * 3;
		}
		break;

	case S_AB2:                          /* down fast, then charges */
		b2s.ybody += SYNC(FX(8));
		if (b2s.ybody > FX(50)) { e->state = S_CRUSHEN; sfx(SFX_BIGEXPLODE); }
		break;
	}

	/* the head springs back */
	if (b2s.headx > B2_PI) b2s.headx -= SYNC(FX(0.8)); else b2s.headx = B2_PI;

	/* hit by a barrel: it rears up and stops what it is doing */
	if (dmg > 5000 && e->state != S_EXPL)
	{
		if (e->state != S_SPECIAL2) b2s.dwack = B2_PI;
		sfx(SFX_BIGEXPLODE);
		if (e->state == S_SCHIESSEN || e->state == S_SPECIAL || e->state == S_AB || e->state == S_AUF) e->state = S_LINKS;
	}
	if (b2s.dwack > 0) b2s.dwack -= SYNC(FX(0.2)); else b2s.dwack = 0;

	{
		Box *b = &kinds[E_RIESENSPINNE].box;
		b->t = (int16_t)(40 + (b2s.ybody >> 8));
		b->b = (int16_t)(140 + (b2s.ybody >> 8));
	}
	if (touching) player_damage(DMG_T(4));

	if (b2s.energy <= 100 && e->state != S_EXPL)
	{
		e->state = S_EXPL;
		b2s.animc = FX(1);
		if (e->t2 && ents[e->t2 - 1].type == E_B2LASER) ents[e->t2 - 1].type = E_NONE;
		e->t2 = 0;
		b2_stop_music();
	}
	if (e->state == S_EXPL)
	{
		b2s.energy = 100;
		if (e->x > FX(v1 + 640)) e->energy = 0;      /* walked off the screen: gone */
	}
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu;
	return 1;
}
static int RIESENSPINNE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int RIESENSPINNE_draw(const Ent *e, uint32_t t)
{
	static const int16_t top1[4] = { 40, 24, -10, -20 }, bot1[4] = { 120, 40, -40, -120 };
	static const int8_t order[4] = { 0, 1, 3, 2 };
	const int x = e->x >> 8, y = e->y >> 8, yb = b2s.ybody >> 8;
	const int td = sin_d(b2s.dwack * 360 / 256) * 10 / 256;
	const int bob = b2_sin8(b2s.walkc) * 2 / 256;
	const int yoff = b2_sin8(b2s.walkc * 2) / 256;
	draw_pic(&P_RIESENSPINNE, 0, FX(x), FX(y + bob + td + yb), 0);
	draw_pic(&b2_s_head, 0, FX(x + 30 + b2_sin8(b2s.headx) * 10 / 256), FX(y + 60 + bob + td + yb), 0);
	for (int i = 0; i < 4; i++)
	{
		const int a = order[i];
		int w = b2s.walkc + B2_PI / 3 * a;
		while (w > B2_TWOPI) w -= B2_TWOPI;
		const int lx = x - 70 + a * 110 + 120, ly = y + 130;
		int xp, yp;
		if (w < B2_PI) { xp = lx + b2_cos8(w) * 18 / 256; yp = ly - b2_sin8(w) * 30 / 256; }
		else { xp = lx + (w - B2_PI) * 10 / 256 - 16; yp = ly; }
		/* the upper part: between (temp + x + top1, o) and (x + bot1, u) */
		const int temp = b2_sin8(w) * 10 / 256, yoff2 = b2_sin8(w - B2_PI - 128) * 5 / 256;
		const int o = ly + 55 - 154 + yoff2 + yb + yoff, u = ly + 30 + yoff + td + yb;
		const int mx = (temp + lx + top1[a] + lx + bot1[a]) / 2;
		draw_pic(&b2_s_leg1, a * 3, FX(mx), FX((o + u) / 2 - 77), 0);
		draw_pic(&b2_s_foot, a * 3, FX(xp), FX(yp), 0);
	}
	(void)t;
	return 1;
}

/* ------------------------------------------------------------------ B2ROCKET: the spider's rocket (Gegner_FetteRakete.cpp) */
/*
 * Flies at 35 in the direction Value1 (degrees) it was given; bursts on walls or on the player
 * (25). Shootable (50, 50 points). Hurrican turns its picture: here rocket.png's 8 directions
 * (e->anim, set when fired). Left out: the smoke trail.
 */
static void B2ROCKET_load(void) { }
static void B2ROCKET_init(Ent *e) { (void)e; }
static int B2ROCKET_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if ((bl | br | bo | bu) & B_WALL) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(25)); e->energy = 0; return 1; }
	if (b2_far(x, y)) { e->type = E_NONE; return 1; }
	(void)idx;
	return 0;
}
static int B2ROCKET_explode(Ent *e) { fx_add(&P_expl, ecx(e), ecy(e), 2); sfx(SFX_EXPLODE); return 1; }
static int B2ROCKET_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ B2LASER: the spider's beam (SPIDERLASER) */
/*
 * From the screen's left edge: 108 steps a thin warning line (drawn as a row of laser pieces),
 * then for 228 steps the beam, 560 long and 200 high (fetterspinnenlaser.png in a row of 9):
 * in it the player loses 500 a unit. t1 = Hurrican's AnimCount x 6 / 2.5.
 */
static void B2LASER_load(void) { }
static void B2LASER_init(Ent *e) { (void)e; }
static int B2LASER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (++e->t1 > 336) { e->type = E_NONE; return 1; }
	if (e->t1 > 108 && b2_meets(x, y, 0, -100, 500, 100)) player_damage(DMG_T(500));
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	return 1;
}
static int B2LASER_explode(Ent *e) { (void)e; return 1; }
static int B2LASER_draw(const Ent *e, uint32_t t)
{
	if (e->t1 <= 108)
	{
		if (e->t1 > 30 || (t & 1))
			for (int i = 0; i < 15; i++) draw_pic(&P_laser, 0, e->x + FX(i * 32 - 10), e->y - FX(1), 0);
	}
	else
		for (int i = 0; i < 9; i++) draw_pic(&P_B2LASER, 0, e->x + FX(i * 64 - 10), e->y - FX(64), 0);
	return 1;
}
