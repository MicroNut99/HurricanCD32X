/*
 * Group b3: the bosses of stages 6 and 7 and their helpers.
 *
 *   134 DRACHE        the steel dragon (bosses/Boss_Drache.cpp): stage 6's last boss (value2 0),
 *                     and stage 7's opening ride on its back (value2 != 0; value1 is not used then)
 *   124 UFO           stage 6's UFO (bosses/Boss_Ufo.cpp), value1 / value2 = its arena
 *   136 SCHNEEKOENIG  stage 7's snow king (bosses/Boss_Schneekoenig.cpp), value1 = which of its
 *                     three appearances (0, 1, 2)
 *   135 ROLLMOPS      stage 7's rolling snow ball (bosses/Boss_Rollmops.cpp), value1 / value2 =
 *                     its arena
 * helpers (spawned): B3_KLEIN the small dragon flying past behind (CDragonHack), B3_MINIDRAGON
 * the mini dragons the dragon drops (Gegner_Minidragon.cpp, value2 99), B3_RAKETE the homing
 * rockets of the dragon and the UFO (Gegner_FetteRakete.cpp, value2 99), B3_KOPPE the king's
 * snow balls (Gegner_Schneekoppe.cpp), B3_KETTE the snow ball's chain (Gegner_Kettenglied.cpp).
 *
 * Hurrican's float counters are kept as 8.8 numbers: speeds / accelerations in its units
 * (FX(10) = 10.0, one step adds SYNC), timers as T(v) = 6 x v units in steps 8.8 (one step
 * takes FX(1)), angles in radians as Q12 (4096 = 1). Hurrican's Destroyable is the kind's
 * shootable flag, switched by the boss while it changes (one of each boss at a time); its
 * GegnerRect changes (the dragon's head, the king's legs) go into the kind's box the same way.
 * Not drawn: glows (lava flares), smoke, sparks, snow dust, shaking; pictures Hurrican turns
 * or scales are drawn upright and at their size.
 */

#define B3_T(v)      FX(6 * (v))            /* a Hurrican timer of v units, in steps 8.8 */
#define B3_PI        12868                  /* Q12 radians */
#define B3_HALFPI    6434
#define B3_TWOPI     25736

static Pic b3_arm, b3_leg, b3_head, b3_jaw, b3_tail, b3_tip, b3_arrow, b3_fireball;
static Pic b3_such, b3_ufolaser, b3_knarre, b3_bluebomb, b3_ball, b3_auf, b3_gun, b3_eis, b3_snowsmall;

/* sin of r radians (Q12) x 256, between whole degrees interpolated */
static int b3_sin12(int32_t r)
{
	const int32_t d = r % B3_TWOPI * 917 / 256;   /* degrees x 256 */
	const int di = (int)(d >> 8), f = (int)(d & 255);
	const int a = sin_d(di), b = sin_d(di + 1);
	return a + ((b - a) * f >> 8);
}

/* atan(n / d) in degrees x 16 (-1440..1440), d != 0 */
static int b3_atan16(int n, int d)
{
	const int an = n < 0 ? -n : n, ad = d < 0 ? -d : d;
	int r;
	if (an <= ad) { const int t = an * 256 / ad; r = (t * 720 + t * (256 - t) * 250 / 256) / 256; }
	else { const int t = ad * 256 / an; r = 1440 - (t * 720 + t * (256 - t) * 250 / 256) / 256; }
	return ((n < 0) != (d < 0)) ? -r : r;
}

/* the player's box meets the kind's box where it is now */
static int b3_touch(const Ent *e)
{
	return pl.state != PS_DEAD && boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box,
	                                         e->x >> 8, e->y >> 8, ent_box(e));
}

/* the player pushed sideways by dx (8.8), not into a wall */
static void b3_push(int dx)
{
	const int px = pl.x >> 8, py = pl.y >> 8;
	if ((dx > 0 && !(blk_right(px, py, px, &player_box, 0) & B_WALL)) || (dx < 0 && !(blk_left(px, py, px, &player_box, 0) & B_WALL)))
		pl.x += dx;
}

/* the player kept on the held screen (Hurrican's Player.cpp: xpos within XOffset .. + 580
 * while the screen is not free to scroll); called by the bosses that hold it */
static void b3_keep(void)
{
	if (!boss_lock_on) return;
	const int32_t l = FX(cam_xoff()), r = FX(cam_xoff() + 580);
	if (pl.x < l) pl.x = l;
	if (pl.x > r) pl.x = r;
}

/* a helper kind spawned active at x, y (0 = none free) */
static Ent *b3_spawn(int type, int x, int y)
{
	Ent *e = ent_new(type, x, y);
	if (e) e->active = 1;
	return e;
}

/* FETTERAKETE dropped by a boss (value2 99): rises and turns, then flies at the player */
static void b3_rakete(int x, int y)
{
	Ent *r = b3_spawn(E_B3_RAKETE, x, y);
	if (r) { r->ys = FX(-10); r->ya = FX(2); r->t1 = 72; }
}

/* EISZAPFENSHOT: an icicle at 40 towards angle w (0 = up, clockwise), damage 12 */
static void b3_icicle(int x, int y, int w)
{
	enemy_shot_pic(&b3_eis, x, y, sin_d(w) * 40, -cos_d(w) * 40, 0, DMG(12), 1);
}

#ifndef OJ32X_MARS
/* PC check: HB_B3_HP=<n> - a boss starts its fight with at most n energy */
static void b3_test_hp(Ent *e)
{
	const char *s = getenv("HB_B3_HP");
	if (s && e->energy > atoi(s)) e->energy = (int16_t)atoi(s);
}
/* PC check: HB_B3LOG=1 prints a boss's state twice a second */
static void b3_log(const Ent *e, int a, int b)
{
	static uint32_t n;
	if (getenv("HB_B3LOG") && n++ % (getenv("HB_B3LOGN") ? atoi(getenv("HB_B3LOGN")) : 30) == 0)
		fprintf(stderr, "b3 %d: state %d/%d pos %d,%d speed %d,%d energy %d cam %d,%d\n", e->type, e->state, a,
		        (int)(e->x >> 8), (int)(e->y >> 8), e->xs >> 8, e->ys >> 8, e->energy, cam_xoff(), cam_yoff());
	(void)b;
}
#else
#define b3_test_hp(e) ((void)0)
#define b3_log(e, a, b) ((void)0)
#endif

/* ================================================================ 134 DRACHE
 * value2 0 - the boss: when it comes into view the screen locks on value1 (its arena) and the
 * stage music stops; once the screen is there the boss song starts and the small dragon flies
 * past behind; then, over and over at random: it flies over the player spitting fireballs (30)
 * or dropping mini dragons; it races past dropping homing rockets while the wind pushes the
 * player; or it comes in at the side and shoots its head out on its neck at the player 2-3
 * times (500 when it bites; only the head can be hit then). 7000 energy, touching it costs 5 a
 * tenth. At 100 it is beaten (10000 points): it rises burning, the mini dragons burst, it falls
 * down and lies there; when the player stands on it it flies off with him: the stage is won.
 * value2 != 0 - stage 7's start: the player rides on its back (held there) while it flies left
 * burning (its energy runs down), crashes, slides and lies still; then the stage music starts
 * and the player can walk off.
 * Drawn: the body, the head and jaw, the front leg and arm, seven tail pieces closed up and
 * the tip, the neck every 30 pixels (at most ten); Hurrican: also the back leg and arm and the
 * knob on the neck, twenty tail pieces, the neck every 10, all turned and scaled. Left out:
 * the jet smoke and flares, the leaves in the wind, the screen shake. */
enum { DH_FIRST, DH_NOTVISIBLE, DH_EINFLIEGEN2, DH_AUSWAHL, DH_LAUFEN, DH_LAUFEN2, DH_LAUFEN3,
       DH_EXPLODIEREN, DH_WARTEN, DH_AUSFAHREN, DH_EINFLIEGEN, DH_DONE };
enum { DA_LAUFEN, DA_LAUFEN2, DA_LAUFEN3, DA_STEHEN, DA_SCHIESSEN, DA_BOMBARDIEREN, DA_EINFLIEGEN,
       DA_VERFOLGEN, DA_SCHLIESSEN, DA_EINFLIEGEN2 };
static struct {
	int16_t attack, shots, pos, locked, destroy, left, hack, past, nmini, startposy, arrow, mini[8];
	int32_t hw;                  /* HeadWinkel, degrees x 16 */
	int32_t kiefer;              /* KieferWinkel, Q12 */
	int32_t hx, hy, hxs, hys;    /* the head's offset (8.8 pixels) and speed */
	int32_t anim, delay, dyo, off, tail, ride;
} b3_dr;

static void DRACHE_load(void)
{
	b3_arm = pic("drache_arm.png");        b3_leg = pic("drache_bein.png");
	b3_head = pic("drache_head.png");      b3_jaw = pic("drache_head2.png");
	b3_tail = pic("drache_tail.png");      b3_tip = pic("drache_spitze.png");
	b3_arrow = pic("hudarrow.png");        b3_fireball = pic("fireball_big.png");
}
static void DRACHE_init(Ent *e)
{
	memset(&b3_dr, 0, sizeof b3_dr);
	b3_dr.pos = -1; b3_dr.left = 1; b3_dr.hack = -1;
	b3_dr.destroy = e->v2 == 0;
	b3_dr.locked = e->v2 != 0;
	kinds[E_DRACHE].box = e->v2 ? (Box){ 50, 15, 150, 135 } : (Box){ 40, 0, 201, 135 };
	kinds[E_DRACHE].shootable = (uint8_t)b3_dr.destroy;
}

/* ComputeHeadWinkel: the head turned to the player */
static void b3_dr_aim(const Ent *e)
{
	if (b3_dr.locked) return;
	const int x = e->x >> 8, y = e->y >> 8;
	int yd = (pl.y >> 8) + 40 - y, w;
	if (!yd) yd = 1;
	if (!b3_dr.left) w = b3_atan16((pl.x >> 8) + 35 - (x + 210), yd) - 90 * 16;
	else w = -b3_atan16((pl.x >> 8) + 35 - (x - 80), yd) - 82 * 16;
	b3_dr.hw = w < -1440 ? -1440 : w > 1440 ? 1440 : w;
}

/* how it faces, its box (the head while it attacks from the side, the back to stand on) */
static void b3_dr_state(const Ent *e)
{
	Box *b = &kinds[E_DRACHE].box;
	int left = e->xs < 0 ? 1 : e->xs > 0 ? 0 : b3_dr.pos == -1;
	if (!b3_dr.destroy) { b3_dr.pos = -1; left = 1; }
	b->l = left ? -50 : 40; b->r = left ? 161 : 250;
	if (e->state == DH_LAUFEN3)
	{
		const int hx = b3_dr.hx >> 8, hy = b3_dr.hy >> 8;
		b->t = (int16_t)(20 + hy); b->b = (int16_t)(130 + hy);
		if (b3_dr.pos == -1) { b->l = (int16_t)(hx - 60); b->r = (int16_t)(hx + 20); }
		else { b->l = (int16_t)(180 + hx); b->r = (int16_t)(270 + hx); }
	}
	else if (e->state == DH_WARTEN || e->state == DH_AUSFAHREN || e->state == DH_EINFLIEGEN) *b = (Box){ 50, 15, 150, 135 };
	else { b->t = 0; b->b = 135; }
	b3_dr.left = (int16_t)left;
	kinds[E_DRACHE].shootable = (uint8_t)b3_dr.destroy;
}

/* the player held on its back (DoFesteAktion), at dx, dy from it after this step's move */
static void b3_dr_hold(const Ent *e, int idx, int32_t dx, int32_t dy)
{
	if (pl.state == PS_DEAD) return;
	pl.x = e->x + dx + SYNC(e->xs + SYNC(e->xa));
	pl.y = e->y + dy + SYNC(e->ys + SYNC(e->ya));
	pl.xs = pl.ys = 0; pl.state = PS_STAND; pl.dir = -1; pl.platform = idx + 1;
}

static int DRACHE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	b3_keep();
	b3_log(e, b3_dr.attack, 0);
	(void)bl; (void)br; (void)bo; (void)bu;
	const int v1 = e->v1, mo = b3_dr.left ? -1 : 0;
	if (e->state == DH_FIRST)
	{
		if (e->v2 == 0)
		{
			boss_lock(v1, y, 10);                     /* ScrollLevel(Value1, yPos, SCROLLTOLOCK) */
			e->x = FX(v1 - 500); x = v1 - 500;
			music(0);                                 /* the stage music fades out */
			Ent *h = b3_spawn(E_B3_KLEIN, cam_xoff(), cam_yoff());
			if (h) { h->t1 = 30; b3_dr.hack = (int16_t)(h - ents); }
			e->state = DH_NOTVISIBLE;
		}
		else
		{
			e->state = DH_EINFLIEGEN; b3_dr.attack = DA_LAUFEN;
			e->xs = FX(-40);
			music(0);                                 /* StopStageMusicAtStart */
		}
	}
	if (e->v2 == 0 && e->state != DH_NOTVISIBLE && e->state != DH_EXPLODIEREN) boss_bar(e->energy, 7000);

	/* beaten */
	if (e->energy <= 100 && e->state != DH_EXPLODIEREN && b3_dr.destroy)
	{
		pl.score += 10000;
		e->state = DH_EXPLODIEREN; b3_dr.attack = DA_LAUFEN;
		e->ya = FX(-1);
		b3_dr.delay = B3_T(1);
		e->energy = 100;
		b3_dr.locked = 1;
		for (int k = 0; k < 8; k++)                       /* the mini dragons burst */
		{
			const int m = b3_dr.mini[k] - 1;
			if (m >= 0 && ents[m].type == E_B3_MINIDRAGON) ents[m].energy = 0;
		}
		music(0);
	}

	b3_dr_aim(e);
	switch (e->state)
	{
	case DH_WARTEN:                                      /* lies there: waits for the player to stand on it */
		b3_dr.hw = -50 * 16;
		b3_dr.arrow++;
		ent_platform(e, idx);
		if (pl.platform == idx + 1 && pl.state != PS_JUMP && pl.state != PS_DEAD)
		{
			sfx(SFX_BIGEXPLODE);
			e->state = DH_AUSFAHREN;
			e->xa = FX(-2); e->ya = FX(-0.2);
			b3_dr.ride = pl.x - e->x;
		}
		break;

	case DH_AUSFAHREN:                                   /* flies off with the player: the stage is won */
		b3_dr_hold(e, idx, b3_dr.ride, FX(-65));
		if (e->x < FX(v1 - 500))
		{
			e->x = FX(v1 - 500);
			e->xs = e->ys = e->xa = e->ya = 0;
			e->state = DH_DONE;
			stage_clear();
		}
		break;

	case DH_EINFLIEGEN:                                  /* stage 7: the player on its back */
		if (b3_dr.attack != DA_STEHEN)
		{
			b3_dr_hold(e, idx, FX(40), FX(-65) + b3_dr.dyo);
			if (--e->t1 < 0)                             /* now and then it burns */
			{
				e->t1 = 15;
				if (!rnd(4)) { fx_add(&P_expl, x - 120 + rnd(300) + 30, y + rnd(100) + 30, 2); sfx(SFX_EXPLODE); }
			}
		}
		switch (b3_dr.attack)
		{
		case DA_LAUFEN:                                  /* flying */
			b3_dr.off += 341;
			if (b3_dr.off > B3_TWOPI) b3_dr.off -= B3_TWOPI;
			b3_dr.dyo = b3_sin12(b3_dr.off) * 8;
			if (e->energy > 100) e->energy -= (int16_t)(14 + (++e->t2 % 6 == 0));   /* 85 a unit */
			else { e->ya = FX(2); b3_dr.attack = DA_LAUFEN2; }
			break;
		case DA_LAUFEN2:                                 /* crashing */
			b3_dr.dyo = b3_dr.dyo > 0 ? b3_dr.dyo - SYNC(FX(1)) : 0;
			if (e->ys > 0 && (blk_below(x, y, e->yo >> 8, ent_box(e), 0) & B_WALL))
			{
				e->ys = (int16_t)(e->ys * -77 / 256);
				sfx(SFX_LAND);
				if (e->ys > FX(-0.5))
				{
					e->ys = 0; e->ya = 0;
					b3_dr.attack = DA_LAUFEN3;
					e->xa = FX(2);
				}
			}
			break;
		case DA_LAUFEN3:                                 /* sliding */
			if (e->xs > 0)
			{
				e->xs = 0; e->xa = 0;
				b3_dr.attack = DA_STEHEN;
				pl.platform = idx + 1;
				music_stage();
			}
			break;
		default:                                         /* lies there: the player can stand on it */
			if (pl.platform == 0) ent_platform(e, idx);
			break;
		}
		break;

	case DH_EXPLODIEREN:                                 /* beaten: rises burning, falls down */
		e->energy = 100;
		b3_dr.delay -= FX(1);
		if (b3_dr.delay < 0)
		{
			b3_dr.delay = B3_T(1);                        /* (Hurrican: every half unit) */
			fx_add(rnd(3) ? &P_expl : &P_explbig, x - 100 + rnd(500), y + 20 + rnd(80) + 20, 2);
			sfx(SFX_EXPLODE);
		}
		if (b3_dr.attack == DA_LAUFEN)
		{
			if (e->xs < 0) e->xa = FX(1);
			if (e->xs > 0) e->xa = FX(-1);
			if (y < b3_dr.startposy - 400)
			{
				e->x = FX(v1 + 450);
				e->xa = e->ya = e->xs = 0;
				e->ys = FX(40);
				b3_dr.hx = b3_dr.hy = 0;
				b3_dr.hw = 75 * 16;
				b3_dr.destroy = 0;
				b3_dr.attack = DA_LAUFEN2;
			}
		}
		else if (y > b3_dr.startposy + 295 && e->ys > 0)
		{
			fx_add(&P_explbig, x + 30, y + 100, 2);
			sfx(SFX_BIGEXPLODE);
			e->xs = e->ys = e->xa = e->ya = 0;
			e->state = DH_WARTEN;
			e->y = FX(b3_dr.startposy + 295);
		}
		break;

	case DH_NOTVISIBLE:                                  /* waits for the screen */
		b3_dr.startposy = (int16_t)(y - 25);
		if (boss_locked_there()) { music_boss(); e->state = DH_EINFLIEGEN2; }
		break;

	case DH_EINFLIEGEN2:                                 /* the small dragon flies past behind first */
	{
		e->energy = 7000;
		Ent *h = b3_dr.hack >= 0 && ents[b3_dr.hack].type == E_B3_KLEIN ? &ents[b3_dr.hack] : 0;
		if (h && h->state == 1) b3_dr.past = 1;
		if (!h || (h->state == 0 && b3_dr.past))
		{
			if (h) h->type = E_NONE;
			b3_dr.hack = -1;
			e->state = DH_AUSWAHL;
		}
		break;
	}

	case DH_AUSWAHL:                                     /* the next move */
	{
		b3_dr.locked = 0;
		b3_test_hp(e);
		const int dir = x < v1 ? -1 : 1;
		switch (rnd(3))
		{
		case 0:                                          /* over the player: fire or mini dragons */
			e->state = DH_LAUFEN;
			e->x = FX(v1 + 420 + 800 * dir);
			e->y = FX(b3_dr.startposy - 80);
			e->xs = (int16_t)(FX(-15) * dir); e->ys = FX(5); e->ya = FX(-0.1);
			if (!rnd(2)) { b3_dr.attack = DA_SCHIESSEN; b3_dr.anim = 33; }
			else { b3_dr.attack = DA_BOMBARDIEREN; b3_dr.anim = B3_T(10); }
			break;
		case 1:                                          /* races past dropping rockets */
			e->state = DH_LAUFEN2;
			e->x = FX(v1 + 420 + 800 * dir);
			e->y = FX(b3_dr.startposy + 50);
			e->xs = (int16_t)(FX(-50) * dir); e->ys = 0; e->ya = 0;
			b3_dr.attack = DA_BOMBARDIEREN;
			b3_dr.anim = B3_T(2.14);
			b3_dr.delay = 0;
			break;
		default:                                         /* in from the side */
			e->state = DH_LAUFEN3;
			b3_dr.attack = DA_EINFLIEGEN;
			b3_dr.shots = (int16_t)(2 + rnd(2));
			b3_dr.pos = (int16_t)(x < v1 ? 1 : -1);
			e->y = FX(b3_dr.startposy + 50 + rnd(50));
			b3_dr.kiefer = B3_HALFPI;
			e->xs = e->ys = e->xa = e->ya = 0;
			b3_dr.anim = 0;
			break;
		}
		break;
	}

	case DH_LAUFEN:                                      /* flies over the player */
		if (b3_dr.attack == DA_SCHIESSEN)
		{
			if (--b3_dr.anim <= 0)                       /* a fireball when the jaw opens (2 pi / 0.75 units) */
			{
				const int hw = b3_dr.hw / 16;
				b3_dr.anim = 50;
				if (!b3_dr.left) enemy_shot_pic(&b3_fireball, x + 240 + hw / 2, y + 50 - hw / 2, cos_d(-hw) * 50, sin_d(-hw) * 50, 0, DMG(30), 0);
				else enemy_shot_pic(&b3_fireball, x - 110 - hw / 2, y + 50 - hw / 2, cos_d(180 + hw) * 50, sin_d(180 + hw) * 50, 0, DMG(30), 0);
			}
		}
		else
		{
			const int sx = x + 100 - mo;
			if (sx > cam_xoff() && sx < cam_xoff() + 640) b3_dr.anim -= FX(1);
			if (b3_dr.anim < 0)                          /* lays a mini dragon */
			{
				b3_dr.anim = B3_T(25);
				Ent *m = b3_spawn(E_B3_MINIDRAGON, sx, y + 80);
				if (m)
				{
					m->state = 4;
					m->ys = (int16_t)-FX(13 + rnd(4)); m->ya = FX(3); m->xs = (int16_t)(FX(rnd(10) - 5) / 2);
					m->anim = 5;
					b3_dr.mini[b3_dr.nmini++ & 7] = (int16_t)(m - ents + 1);
				}
			}
		}
		if ((e->xs > 0 && x > v1 + 1000) || (e->xs < 0 && x < v1 - 500)) e->state = DH_AUSWAHL;
		break;

	case DH_LAUFEN2:                                     /* races past: rockets, the wind */
	{
		const int sx = x + 100 - mo;
		if (sx > cam_xoff() + 50 && sx < cam_xoff() + 600) b3_dr.delay -= FX(1);
		if (b3_dr.delay < 0) { b3_dr.delay = B3_T(3); b3_rakete(sx, y + 80); }
		b3_dr.anim -= FX(1);
		if (b3_dr.anim < 0)
		{
			b3_dr.anim = B3_T(0.3);
			if (pl.state != PS_DEAD) b3_push(b3_dr.left ? -SYNC(FX(8)) : SYNC(FX(8)));
		}
		if ((e->xs > 0 && x > v1 + 1000) || (e->xs < 0 && x < v1 - 500)) e->state = DH_AUSWAHL;
		break;
	}

	case DH_LAUFEN3:                                     /* at the side: the head shoots out */
		switch (b3_dr.attack)
		{
		case DA_EINFLIEGEN:
		case DA_EINFLIEGEN2:
			e->x = b3_dr.pos == 1 ? FX(v1 - 300) + b3_sin12(b3_dr.anim) * 150 : FX(v1 + 740) - b3_sin12(b3_dr.anim) * 150;
			if (b3_dr.attack == DA_EINFLIEGEN2)          /* back out */
			{
				b3_dr.anim -= 51;
				if (b3_dr.anim <= -B3_HALFPI) e->state = DH_AUSWAHL;
			}
			else if (b3_dr.anim >= B3_HALFPI)
			{
				if (b3_dr.anim == B3_HALFPI) b3_dr.attack = DA_SCHIESSEN;
				else b3_dr.anim = B3_HALFPI;
			}
			else b3_dr.anim += 51;
			break;
		case DA_SCHIESSEN:                               /* the jaw opens */
			b3_dr.kiefer -= 341;
			if (b3_dr.kiefer < -B3_HALFPI)
			{
				const int hw = b3_dr.hw / 16;
				b3_dr.kiefer = -B3_HALFPI;
				b3_dr.attack = DA_VERFOLGEN;
				b3_dr.locked = 1;
				b3_dr.hxs = cos_d(hw) * (40 + 10 * skill) * b3_dr.pos;
				b3_dr.hys = -sin_d(hw) * (30 + 8 * skill);
				if (b3_dr.pos == -1 && b3_dr.hxs > FX(-0.1)) b3_dr.hxs = -6;
				if (b3_dr.pos == 1 && b3_dr.hxs < FX(0.1)) b3_dr.hxs = 6;
			}
			break;
		case DA_VERFOLGEN:                               /* the head flies at the player */
		{
			static const Box bite = { 40, 30, 50, 50 };
			int hit = 0;
			b3_dr.hx += SYNC(b3_dr.hxs);
			b3_dr.hy += SYNC(b3_dr.hys);
			if (pl.state != PS_DEAD && boxes_meet(x, y, ent_box(e), pl.x >> 8, pl.y >> 8, &bite))
			{
				b3_dr.shots = 0; hit = 1;
				player_damage(DMG(500));
			}
			const int hx = b3_dr.hx >> 8, hy = b3_dr.hy >> 8;
			if ((b3_dr.hys > 0 && y + 50 + hy > b3_dr.startposy + 380) || (b3_dr.hxs > 0 && x + hx > v1 + 400) ||
			    (b3_dr.hxs < 0 && x + hx < v1 + 100) || hit)
			{
				b3_dr.attack = DA_SCHLIESSEN;
				b3_dr.anim = B3_T(4);
				b3_dr.kiefer = B3_HALFPI;
				sfx(SFX_HIT);
				b3_dr.hys = -b3_dr.hys; b3_dr.hxs = -b3_dr.hxs;
				fx_add(&P_flare, x + hx + 230 + mo * 275, y + hy + 80, 3);
			}
			break;
		}
		case DA_SCHLIESSEN:                              /* a moment, then the head back */
			if (b3_dr.anim > 0) b3_dr.anim -= FX(1);
			else
			{
				b3_dr.anim = 0;
				b3_dr.hx += SYNC(b3_dr.hxs);
				b3_dr.hy += SYNC(b3_dr.hys);
				if (b3_dr.hy < 0 && ((b3_dr.hxs < 0 && b3_dr.hx <= 0) || (b3_dr.hxs > 0 && b3_dr.hx >= 0)))
				{
					b3_dr.locked = 0;
					if (--b3_dr.shots > 0) b3_dr.attack = DA_SCHIESSEN;
					else { b3_dr.anim = B3_HALFPI; b3_dr.attack = DA_EINFLIEGEN2; }
				}
			}
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	if (b3_dr.destroy && touching) player_damage(DMG_T(5));
	if (!(e->state == DH_EINFLIEGEN && b3_dr.attack == DA_STEHEN))
	{
		b3_dr.tail += 683;
		if (b3_dr.tail > B3_TWOPI) b3_dr.tail -= B3_TWOPI;
	}
	b3_dr_state(e);
	return 0;
}
static int DRACHE_explode(Ent *e) { (void)e; return 1; }
static int DRACHE_draw(const Ent *e, uint32_t t)
{
	(void)t;
	if (e->state == DH_FIRST || e->state == DH_NOTVISIBLE) return 1;
	const int left = b3_dr.left, mo = left ? -1 : 0;
	const int32_t x = e->x, y = e->y + b3_dr.dyo, hx = b3_dr.hx, hy = b3_dr.hy;
	/* the neck */
	if ((e->state == DH_LAUFEN3 || e->state == DH_EXPLODIEREN) && b3_dr.attack != DA_EINFLIEGEN && b3_dr.attack != DA_SCHIESSEN)
	{
		const int hxi = hx >> 8, hyi = hy >> 8, dist = isqrt(hxi * hxi + hyi * hyi);
		for (int k = 1; k <= dist / 30 && k <= 10; k++)
			draw_pic(&b3_tail, 0, x + FX(200 + mo * 220) + hx * k * 30 / dist, y + FX(60) + hy * k * 30 / dist, 0);
	}
	draw_pic(&P_DRACHE, 0, x, y, left);
	/* the tail: every third piece, closed up */
	int32_t tx = x + FX(10 - mo * 165);
	int i;
	for (i = 0; i < 20; i += 3)
	{
		draw_pic(&b3_tail, 0, tx, y + FX(62) + b3_sin12(b3_dr.tail + i * 585) * i, 0);
		tx += left ? FX(12 - i / 4) : -FX(12 - i / 4);
	}
	draw_pic(&b3_tip, 0, tx - FX(37 + mo * 37), y + FX(53) + b3_sin12(b3_dr.tail + i * 585) * i, left);
	draw_pic(&b3_jaw, 0, x + FX(190 + mo * 276) + hx, y + FX(50) + hy, left);
	draw_pic(&b3_head, 0, x + FX(180 + mo * 276) + hx, y + FX(15) + hy, left);
	draw_pic(&b3_leg, 0, x - FX(140 + mo * 270), y + FX(85), left);
	draw_pic(&b3_arm, 0, x + FX(140 + mo * 210), y + FX(90), left);
	if (e->state == DH_WARTEN && b3_dr.arrow % 60 >= 30) draw_pic(&b3_arrow, 0, x + FX(50), e->y - FX(30), 0);
	return 1;
}

/* ================================================================ (-1) B3_KLEIN
 * (CDragonHack) the small dragon flying past in the background while the dragon boss waits
 * to come: after a while it flies across the screen from one side at 10 and is away again.
 * Hurrican flies it the whole stage; here only before the fight (the boss waits for it). */
static void B3_KLEIN_load(void) { }
static void B3_KLEIN_init(Ent *e) { (void)e; }
static int B3_KLEIN_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	e->animt = (int16_t)((e->animt + 1) & 7);
	e->anim = (int16_t)(e->animt / 2);
	const int cx = cam_xoff(), cy = cam_yoff();
	if (e->state == 0)
	{
		if (--e->t1 <= 0)
		{
			e->state = 1;
			if (!rnd(2)) { e->x = FX(cx - 160); e->xs = FX(10); }
			else { e->x = FX(cx + 640 + 160); e->xs = FX(-10); }
			e->y = FX(cy + 200);
			e->ys = (int16_t)(FX(rnd(9) - 4) / 5);
			e->dir = e->xs > 0;
		}
	}
	else
	{
		e->x += SYNC(e->xs);
		e->y += SYNC(e->ys);
		if (x + 260 < cx || x - 260 > cx + 640 || y + 240 < cy || y - 240 > cy + 480) { e->state = 0; e->t1 = (int16_t)((rnd(100) + 50) * 6); }
	}
	return 1;
}
static int B3_KLEIN_explode(Ent *e) { (void)e; return 1; }
static int B3_KLEIN_draw(const Ent *e, uint32_t t) { (void)t; return e->state == 0; }

/* ================================================================ (-1) B3_MINIDRAGON
 * (Gegner_Minidragon.cpp, value2 99) dropped by the dragon: tumbles down (13-16 up, falls),
 * lands, then is a mini dragon like the ones in the walls: when the player comes within 300 it
 * darts out and sways towards him (at most 80 from where it landed), spitting fireballs (50,
 * 15); touching it costs 4 a tenth; shot down to 100 of its 200 it bursts piece by piece. Not
 * hurt while waiting. Kept: v1, v2 where it landed, t1 the shot delay (x 60), t2 the dart
 * timer, size the pieces left; state 4 tumbling, 0 waiting, 1 darting, 2 swaying, 3 bursting.
 * Drawn: the head and three of the six neck pieces. */
static void B3_MINIDRAGON_load(void) { }
static void B3_MINIDRAGON_init(Ent *e) { e->t1 = 240; e->size = 6; e->state = 4; }
static int B3_MINIDRAGON_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	if (!e->size) { e->t1 = 240; e->size = 6; }
	if ((e->ys < 0 && (bo & B_WALL)) || (e->ys > 0 && (bu & B_WALL) && e->state != 4)) e->ys = (int16_t)-e->ys;
	if ((e->xs < 0 && (bl & B_WALL)) || (e->xs > 0 && (br & B_WALL))) e->xs = (int16_t)-e->xs;
	if (e->state != 3 && e->state != 0 && e->state != 4 && e->energy < 100)
	{
		e->state = 3; e->energy = 1; e->t1 = 0;
		e->xs = e->ys = e->xa = e->ya = 0;
		fx_add(&P_explbig, x + 30, y + 23, 2);
		sfx(SFX_BIGEXPLODE);
	}
	if (e->state != 4) e->dir = (pl.x >> 8) + 35 >= x + 30;
	switch (e->state)
	{
	case 4:                                          /* tumbling down */
		anim_loop(e, 5, 14, 3);
		if (e->ys > FX(40)) e->ys = FX(40);
		if (bu & B_WALL)
		{
			e->xs = e->ys = e->xa = e->ya = 0;
			e->state = 0; e->anim = 0;
			fx_add(&P_expl, x + 30, y + 10, 2);
			sfx(SFX_EXPLODE);
		}
		break;
	case 0:                                          /* waiting: not hurt */
		e->energy = 200;
		e->v1 = (int16_t)x; e->v2 = (int16_t)y;
		if (abs(pcx() - (x + 30)) < 300 && abs((pl.y >> 8) + 45 - (y + 23)) < 300)
		{
			e->state = 1;
			if (pcx() < x + 30) { e->xs = FX(10); e->xa = FX(-2); }
			else { e->xs = FX(-10); e->xa = FX(2); }
			if ((pl.y >> 8) + 40 < y + 20) { e->ys = FX(10); e->ya = FX(-2); }
			else { e->ys = FX(-10); e->ya = FX(2); }
			e->t2 = 30;
		}
		break;
	case 1:                                          /* darting out */
		if (--e->t2 <= 0) e->state = 2;
		break;
	case 2:                                          /* swaying at the player's height */
	{
		if (player_distance(e) < 600) e->t1 -= 8;
		if (e->t1 < 0)
		{
			e->t1 = 540;
			enemy_shot_pic(&P_pflshot, x + (e->dir ? 14 : -6), y + 16, e->dir ? FX(50) : FX(-50), 0, 0, DMG(15), 0);
		}
		int xd = (pl.x >> 8) - e->v1, yd = (pl.y >> 8) + 35 - (e->v2 + 20);
		xd = xd < -80 ? -80 : xd > 80 ? 80 : xd;
		yd = yd < -80 ? -80 : yd > 80 ? 80 : yd;
		e->xa = x < e->v1 + xd ? FX(2) : FX(-2);
		e->ya = y < e->v2 + yd ? FX(2) : FX(-2);
		if (e->xs < FX(-8)) e->xs = FX(-8);
		if (e->xs > FX(8)) e->xs = FX(8);
		if (e->ys < FX(-8)) e->ys = FX(-8);
		if (e->ys > FX(8)) e->ys = FX(8);
		break;
	}
	default:                                         /* bursting piece by piece */
		e->energy = 100;
		if (--e->t1 < 0)
		{
			const int k = 6 - e->size;
			fx_add(&P_expl, x + 30 + (e->v1 - x) * k / 6, y + 23 + (e->v2 - y) * k / 6, 2);
			sfx(SFX_EXPLODE);
			e->t1 = 12;
			if (--e->size == 0) e->energy = 0;
		}
		return 0;
	}
	if (touching) player_damage(DMG_T(4));
	return 0;
}
static int B3_MINIDRAGON_explode(Ent *e) { (void)e; return 1; }
static int B3_MINIDRAGON_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const Pic *p = &P_B3_MINIDRAGON;
	if (e->state == 4) { draw_pic(p, e->anim, e->x - FX(15), e->y, e->xs > 0); return 1; }
	if (e->state)
	{
		const int32_t dx = (FX(e->v1) - e->x) / 6, dy = (FX(e->v2) - e->y) / 6;
		for (int i = 0; i < 6 && i < e->size; i += 2) draw_pic(p, 4, e->x + (6 - i) * dx, e->y + (6 - i) * dy, 0);
	}
	if (e->size == 6)
	{
		const int sd = e->t1 / 60;
		const int a = e->state == 2 ? (sd == 0 ? 1 : sd == 8 || sd == 7 ? 2 : sd == 6 ? 1 : 0) : 0;
		draw_pic(p, a, e->x + (e->dir ? FX(15) : FX(-15)), e->y, !e->dir);
	}
	return 1;
}

/* ================================================================ (-1) B3_RAKETE
 * (Gegner_FetteRakete.cpp, value2 99) dropped by the dragon and the UFO: rises (-10, falling
 * 2) for 1.2 s while it turns to the player, then flies at him (35); bursts on walls; hitting
 * the player costs 25 and bursts it. 50 energy, 50 points. Drawn upright (Hurrican turns it). */
static void B3_RAKETE_load(void) { }
static void B3_RAKETE_init(Ent *e) { (void)e; }
static int B3_RAKETE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	if (e->state == 0)
	{
		if (--e->t1 < 0)
		{
			fx xs, ys;
			e->state = 1;
			e->xa = e->ya = 0;
			aim_at(x + 12, y + 17, 35, &xs, &ys);
			e->xs = (int16_t)xs; e->ys = (int16_t)ys;
			sfx(SFX_ESHOT);
		}
	}
	else
	{
		anim_loop(e, 0, 9, 2);
		if ((bl | br | bo | bu) & B_WALL) e->energy = 0;
	}
	if (touching) { player_damage(DMG(25)); e->energy = 0; }
	return 0;
}
static int B3_RAKETE_explode(Ent *e) { (void)e; return 0; }
static int B3_RAKETE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 124 UFO
 * When it comes into view the screen locks on its arena (value1, value2), the stage music
 * stops; once the screen is there the boss song starts and the UFO comes down from above.
 * It sways over the middle of the arena (3 towards it), drops two homing rockets every 1.8 s
 * and every 4-8 s changes what it does (never the same twice): two aimed blue shots (20,
 * damage 12) every half second, a falling laser (20) every 0.4 s, or a dive down and up.
 * Touching it costs 20 a tenth. At 100 of its 4000 it falls burning; on the ground it bursts
 * (4000 points, an extra life drops), the screen goes back to the player and the stage music
 * plays again. Left out: the smoke, the flare, the laser flash. */
enum { UH_FIRST, UH_INIT, UH_EINFLIEGEN, UH_LAUFEN, UH_SCHIESSEN, UH_BOMBARDIEREN, UH_FALLEN, UH_EXPLODIEREN };
static struct { int32_t shot, action; int16_t last; } b3_uf;
static void UFO_load(void) { b3_such = pic("suchschuss2.png"); b3_ufolaser = pic("ufolaser.png"); }
static void UFO_init(Ent *e) { (void)e; b3_uf.shot = 0; b3_uf.action = B3_T(40); b3_uf.last = -1; }
static int UFO_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	b3_keep();
	b3_log(e, 0, 0);
	(void)idx; (void)bl; (void)br; (void)bo; (void)touching;
	const int v1 = e->v1, v2 = e->v2;
	if (e->state != UH_EXPLODIEREN) anim_loop(e, 0, 20, 6);
	if (e->state == UH_FIRST)
	{
		boss_lock(v1, v2, 10);
		e->y -= FX(300); y -= 300;
		e->state = UH_INIT;
		music(0);
	}
	if (e->state != UH_INIT && e->state != UH_EXPLODIEREN) boss_bar(e->energy, 4000);
	if (e->energy <= 100 && e->state != UH_EXPLODIEREN)
	{
		e->state = UH_EXPLODIEREN;
		e->xs = e->ys = e->xa = 0;
		e->ya = FX(0.5);
		e->t1 = 6;
		music(0);
	}
	if (e->state != UH_INIT && e->state != UH_EINFLIEGEN && e->state != UH_EXPLODIEREN)
		e->xa = x + 100 < v1 + 320 ? FX(3) : FX(-3);
	switch (e->state)
	{
	case UH_INIT:                                        /* waits for the screen */
		if (boss_locked_there()) { music_boss(); e->state = UH_EINFLIEGEN; }
		break;
	case UH_EINFLIEGEN:                                  /* comes down */
		e->energy = 4000;
		e->y += SYNC(FX(8));
		if (e->y >= FX(v2 + 100)) { e->y = FX(v2 + 100); e->state = UH_LAUFEN; e->xs = FX(-35); b3_test_hp(e); }
		break;
	case UH_LAUFEN:                                      /* sways, drops rockets */
		if (e->y < FX(v2 + 100)) e->y += SYNC(FX(5));
		if (e->y > FX(v2 + 100)) e->y -= SYNC(FX(5));
		b3_uf.shot -= FX(1);
		if (b3_uf.shot < 0)
		{
			b3_rakete(x + 45, y + 40);
			b3_rakete(x + 135, y + 40);
			b3_uf.shot = B3_T(18);
			sfx(SFX_ESHOT);
		}
		b3_uf.action -= FX(1);
		if (b3_uf.action < 0)
		{
			int j = rnd(3);
			while (j == b3_uf.last) j = rnd(3);
			b3_uf.last = (int16_t)j;
			if (j == 0) e->state = UH_SCHIESSEN;
			else if (j == 1) e->state = UH_BOMBARDIEREN;
			else { e->state = UH_FALLEN; e->ys = FX(28); e->ya = FX(-2); }
			b3_uf.shot = B3_T(10);
			b3_uf.action = B3_T(60);
		}
		break;
	case UH_SCHIESSEN:                                   /* aimed blue shots */
	case UH_BOMBARDIEREN:                                /* the laser */
		if (e->y > FX(v2 + 50)) e->y -= SYNC(FX(5));
		else e->y = FX(v2 + 50);
		b3_uf.action -= FX(1);
		if (b3_uf.action < 0) { b3_uf.action = e->state == UH_SCHIESSEN ? B3_T(80) : B3_T(60); e->state = UH_LAUFEN; }
		if (b3_uf.shot > 0) b3_uf.shot -= FX(1);
		if (b3_uf.shot <= 0)
		{
			if (e->state == UH_SCHIESSEN)
			{
				fx xs, ys;
				aim_at(x + 40, y + 60, 20, &xs, &ys);
				enemy_shot_pic(&b3_such, x + 20, y + 40, xs, ys, 0, DMG(12), 0);
				aim_at(x + 185, y + 60, 20, &xs, &ys);
				enemy_shot_pic(&b3_such, x + 165, y + 40, xs, ys, 0, DMG(12), 1);
				b3_uf.shot = B3_T(5);
			}
			else
			{
				enemy_shot_pic(&b3_ufolaser, x + 79, y + 60, 0, FX(20), SYNC(FX(10)), DMG(20), 0);
				b3_uf.shot = B3_T(4);
			}
		}
		break;
	case UH_FALLEN:                                      /* a dive */
		if (e->ys < FX(-28)) { e->ya = 0; e->ys = 0; b3_uf.action = B3_T(30); e->state = UH_LAUFEN; }
		break;
	case UH_EXPLODIEREN:                                 /* falls burning, bursts on the ground */
		e->energy = 100;
		if (--e->t1 < 0)
		{
			fx_add(rnd(4) ? &P_expl : &P_explbig, x + rnd(180) + 20, y + rnd(70) + 40, 2);
			sfx(SFX_EXPLODE);
			e->t1 = 12;
		}
		if (e->ys > FX(20)) e->ys = FX(20);
		if (bu & (B_WALL | B_PLATFORM)) { e->ya = e->ys = 0; e->energy = 0; }
		break;
	default:
		break;
	}
	return 0;
}
static int UFO_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 100, y + 60, 2);
	fx_add(&P_explbig, x + 40, y + 70, 3);
	fx_add(&P_explbig, x + 160, y + 70, 3);
	sfx(SFX_BIGEXPLODE);
	Ent *o = b3_spawn(E_ONEUP, x + 80, y + 20);
	if (o) o->ya = FX(2);
	boss_unlock();
	music_stage();
	return 1;
}
static int UFO_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 136 SCHNEEKOENIG
 * When it comes into view the screen holds where it is, the boss song starts and the king
 * drops into the middle of the screen; on landing the screen moves to it and it stands up.
 * Only its own snow balls hurt it (5000 of its 50000 each - kept as 50 of 500 here); the
 * player shoots them back at it (a ball hit from the side turns round). It aims its gun and
 * fires snow balls; the second time it also jumps at the player six times (500 when it lands
 * on him), the third time it also hops from side to side dropping blue bombs (20). The first
 * time it only shoots. Beaten, it flies off (the third time it blows up on the way): 2000 x
 * value1 points, the screen goes back to the player, the stage music plays again. The player
 * can stand on it and is pushed away from its sides. Drawn: the body and the gun (upright,
 * Hurrican turns it). Left out: the snow dust, the steam, the flames under it as it flies. */
enum { KH_FIRST, KH_INIT, KH_INIT2, KH_EINFLIEGEN, KH_SPRINGEN, KH_AUSFAHREN, KH_VERFOLGEN };
enum { KA_NONE, KA_EINFLIEGEN, KA_INIT2, KA_INIT3, KA_INIT4, KA_STEHEN, KA_SPRINGEN, KA_BOMBARDIEREN,
       KA_AUSSPUCKEN, KA_SCHLIESSEN, KA_AUSSPUCKENZWEI };
static struct { int16_t action, shots, yoff, ysave; int32_t kw, ky, delay, wackel, xoff, anim; } b3_kg;
static int16_t b3_kg_last;                          /* NextAction's LastAttack (kept for the game) */
static void SCHNEEKOENIG_load(void) { b3_knarre = pic("schneekoenig_kna"); b3_bluebomb = pic("blauebombe.png"); }
/* the king's own state (it comes three times: set again each time it comes) */
static void SCHNEEKOENIG_init(Ent *e)
{
	(void)e;
	memset(&b3_kg, 0, sizeof b3_kg);
	b3_kg.action = KA_NONE;
	b3_kg.ky = FX(60);
	kinds[E_SCHNEEKOENIG].box = (Box){ 60, 50, 145, 120 };
}
static void b3_kg_next(Ent *e, int n)
{
	e->state = KH_EINFLIEGEN;
	if (n < 0)
	{
		n = b3_kg_last + 1;
		if (n > 3 || (n == 2 && e->v1 == 1)) n = 0;
	}
	b3_kg.action = n == 1 ? KA_INIT3 : n == 3 ? KA_INIT4 : KA_INIT2;
	b3_kg_last = (int16_t)n;
	b3_kg.kw = 0;
}
static int SCHNEEKOENIG_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	b3_keep();
	b3_log(e, b3_kg.action, 0);
	(void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	static const int8_t hitanim[4] = { 1, 2, 1, 0 };
	Box *box = &kinds[E_SCHNEEKOENIG].box;
	if (e->state == KH_FIRST)
	{
		SCHNEEKOENIG_init(e);
		music_boss();
		b3_kg.ysave = (int16_t)y;
		x = cam_xoff() + 250; y = cam_yoff() - 300;
		e->x = FX(x); e->y = FX(y);
		e->ys = FX(50);
		boss_lock(cam_xoff(), cam_yoff(), 10);            /* held where it is */
		e->state = KH_INIT;
	}
	if (e->state != KH_INIT) boss_bar(e->energy, 500);
	switch (e->state)
	{
	case KH_INIT:                                        /* drops in */
		if (y > b3_kg.ysave)
		{
			e->y = FX(b3_kg.ysave);
			e->state = KH_INIT2;
			e->ys = 0;
			b3_kg.anim = B3_T(20);
			fx_add(&P_smoke, x + 60, y + 120, 3);
			sfx(SFX_LAND);
			boss_lock(cam_xoff(), b3_kg.ysave - 320, 10);
			b3_test_hp(e);
		}
		break;
	case KH_INIT2:                                       /* a moment while the screen moves */
		b3_kg.anim -= FX(1);
		if (b3_kg.anim < 0) { e->state = KH_EINFLIEGEN; b3_kg.action = KA_EINFLIEGEN; e->anim = 0; e->animt = 0; }
		break;
	case KH_EINFLIEGEN:
		if (b3_kg.action == KA_EINFLIEGEN)               /* the legs out */
		{
			if (++e->animt > 6) { e->animt = 0; if (++e->anim >= 12) e->anim = 0; }
			b3_kg.yoff = (int16_t)(e->anim * -8 < -40 ? -40 : e->anim * -8);
			if (e->anim >= 11)
			{
				b3_kg.yoff = 0;
				e->y -= FX(40);
				e->anim = 10;
				box->b = 150;
				b3_kg_next(e, 0);
			}
		}
		else                                             /* the gun out, then the next attack */
		{
			b3_kg.ky -= SYNC(FX(5));
			if (b3_kg.ky < 0)
			{
				b3_kg.ky = 0;
				if (b3_kg.action == KA_INIT2)
				{
					b3_kg.action = KA_NONE;
					e->state = KH_VERFOLGEN;
					b3_kg.delay = B3_T(1);
					b3_kg.shots = (int16_t)(10 + rnd(6));
				}
				else
				{
					b3_kg.delay = B3_T(1.5);
					b3_kg.action = b3_kg.action == KA_INIT3 ? KA_STEHEN : KA_BOMBARDIEREN;
					e->state = KH_SPRINGEN;
					e->anim = 10;
					b3_kg.anim = B3_T(1);
					b3_kg.shots = 6;
					if (pl.platform == idx + 1) pl.platform = 0;
				}
			}
		}
		break;
	case KH_SPRINGEN:
		if (b3_kg.ky < FX(60)) { b3_kg.ky += SYNC(FX(5)); break; }   /* the gun back in first */
		b3_kg.ky = FX(60);
		switch (b3_kg.action)
		{
		case KA_STEHEN:                                  /* crouches, jumps */
		case KA_BOMBARDIEREN:
			b3_kg.anim -= FX(4);
			if (b3_kg.anim <= 0)
			{
				b3_kg.anim = B3_T(1);
				if (--e->anim < 5)
				{
					if (b3_kg.action == KA_STEHEN)       /* at the player */
					{
						const int d = abs(pcx() - ecx(e));
						b3_kg.action = KA_SPRINGEN;
						e->ys = FX(-80); e->ya = FX(18);
						e->xs = (int16_t)(FX(d) / 8 * (x > (pl.x >> 8) ? -1 : 1));
					}
					else                                 /* to the other side (the last one to the middle) */
					{
						int target = x + 100 > cam_xoff() + 320 ? cam_xoff() + 100 : cam_xoff() + 540;
						if (b3_kg.shots == 1) target = cam_xoff() + 320;
						b3_kg.action = KA_AUSSPUCKEN;
						e->ys = FX(-110); e->ya = FX(16);
						e->xs = (int16_t)(FX(target - (x + 100)) / 12);
					}
				}
			}
			break;
		case KA_SCHLIESSEN:                              /* stands up again */
		case KA_AUSSPUCKENZWEI:
			b3_kg.anim -= FX(4);
			if (b3_kg.anim <= 0)
			{
				b3_kg.anim = B3_T(1);
				if (++e->anim > 10)
				{
					e->anim = 10;
					b3_kg.action = b3_kg.action == KA_SCHLIESSEN ? KA_STEHEN : KA_BOMBARDIEREN;
					if (--b3_kg.shots <= 0) b3_kg_next(e, -1);
				}
			}
			break;
		default:                                         /* in the air */
			if (b3_kg.action == KA_AUSSPUCKEN && b3_kg.shots > 1)
			{
				b3_kg.delay -= FX(1);
				if (b3_kg.delay <= 0)
				{
					b3_kg.delay = B3_T(2);
					enemy_shot_pic(&b3_bluebomb, x + 100, y + 140, 0, FX(10), SYNC(FX(10)), DMG(20), 0);
				}
			}
			if (e->ys > 0 && y > b3_kg.ysave - 40)       /* down */
			{
				e->y = FX(b3_kg.ysave - 40);
				e->xs = e->ys = e->xa = e->ya = 0;
				b3_kg.action = b3_kg.action == KA_SPRINGEN ? KA_SCHLIESSEN : KA_AUSSPUCKENZWEI;
				if (b3_touch(e)) player_damage(DMG(500));
				fx_add(&P_smoke, x + 100, y + 150, 3);
				sfx(SFX_LAND);
			}
			break;
		}
		break;
	case KH_AUSFAHREN:                                   /* beaten: flies off */
		if (b3_kg.kw < 0) b3_kg.kw += SYNC(FX(5));
		if (b3_kg.kw > 0) b3_kg.kw -= SYNC(FX(5));
		b3_kg.xoff = (b3_kg.xoff + 10240) % B3_TWOPI;      /* it shakes */
		if (b3_kg.ky < FX(60)) b3_kg.ky += SYNC(FX(5));
		else
		{
			b3_kg.ky = FX(60);
			b3_kg.anim -= FX(1);
			if (b3_kg.anim < 0 && e->ya == 0) { e->ys = FX(-5); e->ya = FX(-0.6); }
			if (y < cam_yoff() - 300) e->energy = 0;
			if (e->v1 >= 2 && y < cam_yoff() + 150) e->energy = 0;   /* the third time it blows up */
			if (e->v1 >= 2 && --e->t1 < 0)
			{
				e->t1 = 6;
				if (!rnd(2)) { fx_add(&P_expl, x + rnd(150) + 20, y + rnd(100) + 20, 2); sfx(SFX_EXPLODE); }
			}
			int a = (y - cam_yoff() - 200) / 10;
			e->anim = (int16_t)(a < 0 ? 0 : a > 10 ? 10 : a);
		}
		break;
	case KH_VERFOLGEN:                                   /* aims and fires snow balls */
	{
		const int32_t aim = FX(x + 100 - ((pl.x >> 8) + 35)) / 10;
		if (b3_kg.kw < aim) { b3_kg.kw += SYNC(FX(8)); if (b3_kg.kw > aim) b3_kg.kw = aim; }
		if (b3_kg.kw > aim) { b3_kg.kw -= SYNC(FX(8)); if (b3_kg.kw < aim) b3_kg.kw = aim; }
		if (abs(pcx() - ecx(e)) < 100 && e->v1 > 0) b3_kg.shots--;
		else if (abs((aim - b3_kg.kw) / 256) < 10) b3_kg.delay -= FX(1);
		if (b3_kg.delay <= 0)
		{
			const int kw = b3_kg.kw / 256, w = kw <= 0 ? 180 - kw : kw - 180, a = -w - 3 + rnd(7);
			b3_kg.delay = B3_T(8);
			Ent *k = b3_spawn(E_B3_KOPPE, x + sin_d(kw + 180) * 70 / 256 + 100,
			                  y + cos_d(kw + 180) * 60 / 256 + (b3_kg.ky >> 8) + 5 + b3_kg.yoff);
			if (k) { k->xs = (int16_t)(sin_d(a) * 40); k->ys = (int16_t)(cos_d(a) * 40); k->ya = FX(6); k->v2 = (int16_t)idx; }
			sfx(SFX_ESHOT);
			if (e->v1 > 0) b3_kg.shots--;
		}
		if (b3_kg.shots <= 0) b3_kg_next(e, -1);
		break;
	}
	default:
		break;
	}

	/* hit by a snow ball: shaken (DoDraw) */
	if (b3_kg.wackel > 0)
	{
		e->anim = (int16_t)(10 - hitanim[(b3_kg.wackel >> 8) % 4]);
		b3_kg.wackel -= SYNC(FX(2));
		if (b3_kg.wackel < 0) e->anim = 10;
	}
	box->t = (int16_t)(60 + b3_kg.yoff);
	if (e->state != KH_SPRINGEN && e->state != KH_AUSFAHREN && e->state >= KH_INIT2)
	{
		/* Wegschieben: the player pushed off its sides; PlattformTest: he can stand on it */
		if (pl.platform != idx + 1 && b3_touch(e))
			b3_push(pcx() < x + (box->l + box->r) / 2 ? -SYNC(FX(21)) : SYNC(FX(21)));
		ent_platform(e, idx);
	}
	if (e->energy < 1 && e->state != KH_AUSFAHREN)
	{
		if (pl.platform == idx + 1) pl.platform = 0;
		e->energy = 1;
		e->state = KH_AUSFAHREN;
		b3_kg.anim = B3_T(10);
		e->xs = e->ys = e->xa = e->ya = 0;
		sfx(SFX_BIGEXPLODE);
		music(0);
	}
	return 0;
}
static int SCHNEEKOENIG_explode(Ent *e)
{
	pl.score += 2000u * (uint32_t)(e->v1 > 0 ? e->v1 : 0);
	if (e->v1 >= 2)
	{
		fx_add(&P_explbig, ecx(e), ecy(e), 2);
		fx_add(&P_explbig, ecx(e) - 50, ecy(e) + 20, 3);
		fx_add(&P_explbig, ecx(e) + 50, ecy(e) - 10, 3);
		sfx(SFX_BIGEXPLODE);
	}
	boss_unlock();
	music_stage();
	return 1;
}
static int SCHNEEKOENIG_draw(const Ent *e, uint32_t t)
{
	(void)t;
	if (e->state == KH_FIRST) return 1;
	const int s = b3_sin12(b3_kg.xoff);
	draw_pic(&b3_knarre, 0, e->x + FX(95) + s * 3 / 2, e->y + FX(b3_kg.yoff - 36) + b3_kg.ky, 0);
	draw_pic(&P_SCHNEEKOENIG, e->anim, e->x + s * 5, e->y + FX(b3_kg.yoff), 0);
	return 1;
}

/* ================================================================ (-1) B3_KOPPE
 * (Gegner_Schneekoppe.cpp) the king's snow ball: flies under gravity (6); bursts on walls and
 * on the player (20). The player's shot turns it round once (x -0.75, a diagonal one also up 8)
 * - back at the king, which loses 5000 when one hits it. Kept: v2 the king. Drawn upright. */
static void B3_KOPPE_load(void) { }
static void B3_KOPPE_init(Ent *e) { (void)e; }
static int B3_KOPPE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	if (e->hit == 4 && !e->state)                    /* the player's shot: turned round */
	{
		e->state = 1;
		if (pl.aim != 2)
		{
			e->xs = (int16_t)(-e->xs * 3 / 4);
			if (pl.aim == 1) e->ys -= FX(8);
		}
	}
	if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM)) e->energy = 0;
	if (touching) { player_damage(DMG(20)); e->energy = 0; }
	Ent *k = &ents[e->v2];
	if (k->type == E_SCHNEEKOENIG && k->state != KH_AUSFAHREN && boxes_meet(k->x >> 8, k->y >> 8, ent_box(k), x, y, ent_box(e)))
	{
		e->energy = 0;
		k->energy -= 50;
		if (k->state != KH_SPRINGEN) b3_kg.wackel = FX(8);
		sfx(SFX_HIT);
	}
	return 0;
}
static int B3_KOPPE_explode(Ent *e) { (void)e; return 0; }
static int B3_KOPPE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 135 ROLLMOPS
 * When it is seen the screen locks on its arena (value1, value2) and the stage music stops;
 * once there the boss song starts and the snow ball bounces in from the right and rolls to a
 * stop. It throws snow lumps (15), shoots a hook up and swings on its chain over the arena,
 * throwing a ring of icicles (12) at the top of each swing; only the chain can be hurt then
 * (the damage goes to the boss). At 2000 of its 4000 the chain breaks: it drops and rolls
 * from side to side, now and then hopping; after five turns it opens and fires icicles from
 * its gun - only then it can be shot; after 15-30 shots or 100 damage it closes and rolls
 * again. Under 1000 it stops in the middle when it rolls and jumps, throwing icicle rings.
 * Touching it costs 20 a tenth. Beaten it bursts (5000 points), the screen goes back to the
 * player and the stage music plays again. Drawn: the ball, the body opening or with its gun
 * (upright, Hurrican turns it), ten chain links (Hurrican: twenty). Left out: the snow dust,
 * the sparks; the snow lumps do not bounce. */
enum { RH_FIRST, RH_EINFLIEGEN, RH_INIT, RH_STEHEN, RH_AUSSPUCKEN, RH_WARTEN, RH_LINKS, RH_RECHTS,
       RH_LINKS2, RH_RECHTS2, RH_OEFFNEN, RH_SCHLIESSEN, RH_VERFOLGEN, RH_SPECIAL2, RH_SPECIAL3, RH_EXPLODIEREN };
#define B3_LINKS 10
static struct {
	int32_t gw, delay, shotdelay, shotcount, hookx, hooky, sdir, schwung, animc, anims, hits;
	int16_t wc, shot, destroy, link[B3_LINKS];
} b3_rm;
static void ROLLMOPS_load(void)
{
	b3_ball = pic("snowbomb.png");          b3_auf = pic("rollmops_aufklap");
	b3_gun = pic("rollmops_gun.png");       b3_eis = pic("eiszapfenshot.pn");
	b3_snowsmall = pic("snowbombsmall.pn");
}
static void ROLLMOPS_init(Ent *e) { (void)e; memset(&b3_rm, 0, sizeof b3_rm); kinds[E_ROLLMOPS].shootable = 0; }

/* Abhopsen: bounces on the arena's floor (mul x 256) */
static void b3_rm_bounce(Ent *e, int mul)
{
	const int32_t floor = FX(e->v2 + 480 - 140);
	if (e->ys > 0 && e->y > floor)
	{
		e->y = floor;
		sfx(SFX_LAND);
		e->ys = (int16_t)(e->ys * mul / 256);
		if (e->ys > FX(-1)) { e->ys = 0; e->ya = 0; }
	}
}
/* RoundShot: a ring of icicles, or one more of the turning ring */
static void b3_rm_round(const Ent *e, int single)
{
	const int x = e->x >> 8, y = e->y >> 8;
	if (single)
	{
		b3_icicle(x + 50 + sin_d(b3_rm.wc) * 40 / 256, y + 50 - cos_d(b3_rm.wc) * 40 / 256, b3_rm.wc);
		b3_rm.wc += 30;
		if (b3_rm.wc >= 360) b3_rm.wc = 0;
	}
	else
		for (b3_rm.wc = (int16_t)rnd(30); b3_rm.wc < 360; b3_rm.wc += 30)
			b3_icicle(x + 50 + sin_d(b3_rm.wc) * 50 / 256, y + 50 - cos_d(b3_rm.wc) * 50 / 256, b3_rm.wc);
	sfx(SFX_ESHOT);
}
/* GunAusfahren: opens (to shoot) or closes */
static void b3_rm_gun(Ent *e, int open)
{
	e->xs = e->ys = e->xa = e->ya = 0;
	if (open) { e->state = RH_OEFFNEN; e->anim = 0; b3_rm.hits = e->energy; }
	else { e->state = RH_SCHLIESSEN; e->anim = 7; b3_rm.delay = 0; }
	b3_rm.animc = 0;
	b3_rm.anims = B3_T(1.2);
}
static int ROLLMOPS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	b3_keep();
	b3_log(e, 0, 0);
	(void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	const int v1 = e->v1, v2 = e->v2;
	if (e->state == RH_FIRST)
	{
		boss_lock(v1, v2, 10);
		e->x = FX(v1 + 640); x = v1 + 640;
		music(0);
		e->state = RH_EINFLIEGEN;
	}
	if (e->state != RH_INIT && e->state != RH_EXPLODIEREN) boss_bar(e->energy, 4000);
	if (b3_rm.delay > 0) b3_rm.delay -= FX(1);
	else switch (e->state)
	{
	case RH_EINFLIEGEN:                                  /* waits for the screen, then bounces in */
		if (boss_locked_there())
		{
			e->xs = FX(-10); e->ya = FX(4);
			e->state = RH_INIT;
			e->anim = 0;
			b3_rm.anims = B3_T(0.25);
			music_boss();
			b3_test_hp(e);
		}
		break;
	case RH_INIT:
		b3_rm.animc += FX(1);
		if (b3_rm.animc > b3_rm.anims) { b3_rm.animc = 0; if (++e->anim >= 19) e->anim = 0; }
		b3_rm.anims += FX(0.02);
		b3_rm_bounce(e, -154);
		if (e->ys == 0)                                  /* rolls out */
		{
			e->xa = FX(1);
			b3_rm.anims += FX(0.02);
			if (e->xs >= 0)
			{
				e->xa = 0;
				e->state = RH_STEHEN;
				b3_rm.shotdelay = B3_T(5);
			}
		}
		break;
	case RH_STEHEN:                                      /* snow lumps, the hook goes up */
		b3_rm.shotdelay -= FX(1);
		if (b3_rm.shotdelay <= 0)
		{
			b3_rm.shotdelay = 0;
			e->state = RH_AUSSPUCKEN;
			e->anim = 0;
			b3_rm.hookx = FX(x + 65 - 9); b3_rm.hooky = FX(y);
			b3_rm.schwung = b3_rm.sdir = 0;
			sfx(SFX_LAND);
			for (int i = 0; i < 8; i++)
				enemy_shot_pic(&b3_snowsmall, x + 10 + rnd(80), y + 10 + rnd(80), FX(rnd(80) - 40) / 5, -FX(rnd(40) + 80) / 5,
				               SYNC(FX(5)), DMG(15), 1);
			for (int i = 0; i < B3_LINKS; i++)
			{
				Ent *l = b3_spawn(E_B3_KETTE, x, y);
				b3_rm.link[i] = (int16_t)(l ? l - ents : -1);
				if (l) l->v2 = (int16_t)idx;
			}
			kinds[E_B3_KETTE].shootable = 1;
		}
		break;
	case RH_AUSSPUCKEN:                                  /* swings on the chain */
		b3_rm.hooky -= SYNC(FX(40));
		if (b3_rm.hooky < FX(v2 + 20))
		{
			if (y < v2 + 130 && !b3_rm.shot) { b3_rm_round(e, 0); b3_rm.shot = 1; }
			b3_rm.hooky = FX(v2 + 20);
			e->x = FX(v1 + 320 - 65) + b3_sin12(b3_rm.schwung) * 300;
			e->y = FX(v2 - 500) + b3_sin12(b3_rm.schwung * 3 / 4 + B3_HALFPI) * 830;
			b3_rm.sdir += 109;
			if (b3_rm.sdir > B3_TWOPI) b3_rm.sdir = 0;
			b3_rm.schwung = b3_sin12(b3_rm.sdir) * 16;
			int a = -((v1 + 640 - (int)(e->x >> 8)) / 200);
			e->anim = (int16_t)(a < 0 ? a + 10 : a);
			if (((b3_rm.sdir > B3_PI - 819 && b3_rm.sdir < B3_PI + 819) || b3_rm.sdir > B3_TWOPI - 819) && !rnd(2))
				b3_rm.shot = 0;
		}
		{                                                /* the chain from the hook to it */
			const int32_t dx = (e->x + FX(65 - 9) - b3_rm.hookx) / B3_LINKS, dy = (e->y + FX(65) - b3_rm.hooky) / B3_LINKS;
			for (int i = 0; i < B3_LINKS; i++)
			{
				const int k = b3_rm.link[i];
				if (k >= 0 && ents[k].type == E_B3_KETTE) { ents[k].x = b3_rm.hookx + i * dx; ents[k].y = b3_rm.hooky + i * dy; }
			}
		}
		if (e->energy < 2000)                            /* the chain shot through: it drops */
		{
			e->state = RH_WARTEN;
			e->ys = FX(-20); e->ya = FX(6);
			b3_rm.anims = B3_T(10);
			for (int i = 0; i < B3_LINKS; i++)
			{
				const int k = b3_rm.link[i];
				if (k >= 0 && ents[k].type == E_B3_KETTE) ents[k].state = 1;
			}
			kinds[E_B3_KETTE].shootable = 0;
		}
		break;
	case RH_LINKS:                                       /* rolls left */
	case RH_LINKS2:
		if (e->xs < FX(-60)) e->xs = FX(-60);
		if (x + 65 < v1 + 135) e->xa = FX(19);
		if (e->state == RH_LINKS2 && e->ys == 0)         /* stops in the middle and jumps */
		{
			if (x + 65 < v1 + 380) e->xa = FX(23);
			if (e->xs > 0)
			{
				e->xs = e->xa = 0;
				e->ya = FX(10);
				e->state = RH_SPECIAL2;
				b3_rm.shotcount = B3_T(2);
				e->ys = FX(-77);
				break;
			}
		}
		b3_rm.animc -= e->xs / 10;
		if (b3_rm.animc > b3_rm.anims) { b3_rm.animc = 0; e->anim++; }
		if (e->anim > 9) e->anim = 0;
		if (e->xs > 0)                                   /* turned */
		{
			e->state = RH_RECHTS;
			b3_rm.shotdelay -= B3_T(1);
			if ((b3_rm.shotdelay / B3_T(1)) % 3 == 0) { e->xs = FX(12); e->ys = FX(-60); e->ya = FX(15); }
			if (b3_rm.shotdelay <= 0) b3_rm_gun(e, 1);
		}
		b3_rm_bounce(e, -102);
		break;
	case RH_RECHTS:                                      /* rolls right */
	case RH_RECHTS2:
		if (e->xs > FX(60)) e->xs = FX(60);
		if (x + 65 > v1 + 505) e->xa = FX(-19);
		if (e->state == RH_RECHTS2 && e->ys == 0)
		{
			if (x + 65 > v1 + 300) e->xa = FX(-23);
			if (e->xs < 0)
			{
				e->xs = e->xa = 0;
				e->ya = FX(8);
				e->state = RH_SPECIAL3;
				b3_rm.shotcount = B3_T(2);
				e->ys = FX(-65);
				break;
			}
		}
		b3_rm.animc += e->xs / 10;
		if (b3_rm.animc > b3_rm.anims) { b3_rm.animc = 0; e->anim--; }
		if (e->anim < 0) e->anim = 9;
		if (e->xs < 0)
		{
			e->state = RH_LINKS;
			b3_rm.shotdelay -= B3_T(1);
			if ((b3_rm.shotdelay / B3_T(1)) % 3 == 0) { e->xs = FX(-16); e->ys = FX(-60); e->ya = FX(12); }
			if (b3_rm.shotdelay <= 0) b3_rm_gun(e, 1);
		}
		b3_rm_bounce(e, -102);
		break;
	case RH_WARTEN:                                      /* dropped: lies a moment */
		b3_rm_bounce(e, -102);
		if (e->ys == 0) b3_rm.anims -= FX(1);
		if (b3_rm.anims <= 0)
		{
			b3_rm.shotdelay = B3_T(5);
			e->state = RH_LINKS;
			e->xs = FX(-60); e->xa = 0;
			e->anim = 0;
			b3_rm.anims = B3_T(1);
		}
		break;
	case RH_OEFFNEN:                                     /* opens / closes */
	case RH_SCHLIESSEN:
	{
		int done = 0;
		b3_rm.animc += FX(1);
		if (b3_rm.animc > b3_rm.anims)
		{
			b3_rm.animc = 0;
			if (e->state == RH_OEFFNEN) { if (++e->anim >= 8) { done = 1; b3_rm.delay = B3_T(10); b3_rm.gw = 0; } }
			else if (--e->anim <= 0) done = 1;
		}
		if (done)
		{
			if (e->state == RH_OEFFNEN)
			{
				e->anim = 8;
				b3_rm.shotcount = FX(rnd(15) + 15);
				e->state = RH_VERFOLGEN;
				b3_rm.destroy = 1;
			}
			else
			{
				b3_rm.destroy = 0;
				b3_rm.shotdelay = B3_T(5);
				e->xa = 0;
				e->anim = 0;
				b3_rm.anims = B3_T(1);
				if (x > v1 + 320) { e->state = e->energy < 1000 ? RH_LINKS2 : RH_LINKS; e->xs = FX(-40); }
				else { e->state = e->energy < 1000 ? RH_RECHTS2 : RH_RECHTS; e->xs = FX(40); }
			}
		}
		break;
	}
	case RH_VERFOLGEN:                                   /* aims its gun, icicles */
	{
		int yd = (pl.y >> 8) + 40 - (y + 60), xd = (pl.x >> 8) + 35 - (x + 50);
		if (!xd) xd = 1;
		int nw = b3_atan16(yd, xd) * 16;                 /* degrees 8.8 */
		if (x >= v1 + 320) nw = -nw;
		nw = nw < FX(-30) ? FX(-30) : nw > FX(90) ? FX(90) : nw;
		if (b3_rm.gw < nw) b3_rm.gw += SYNC(FX(5));
		if (b3_rm.gw > nw) b3_rm.gw -= SYNC(FX(5));
		b3_rm.shotdelay -= FX(1);
		if (b3_rm.shotdelay < 0)
		{
			const int g = b3_rm.gw / 256;
			int w, xo = 0;
			if (x > v1 + 320) w = 270 - g;
			else { w = g + 90; xo = -70; }
			b3_icicle(x + xo + 90 + sin_d(w) * 105 / 256, y + 46 - cos_d(w) * 90 / 256, w);
			b3_rm.shotdelay = B3_T(3);
			b3_rm.shotcount -= FX(1);
			if (b3_rm.shotcount <= 0 || b3_rm.hits - e->energy > 100) b3_rm_gun(e, 0);
		}
		break;
	}
	case RH_SPECIAL2:                                    /* jumps in the middle: icicle rings */
	case RH_SPECIAL3:                                    /* or a turning ring */
		b3_rm_bounce(e, -77);
		if (e->ys == 0)
		{
			b3_rm.shotdelay = B3_T(3);
			if (e->state == RH_SPECIAL2) { e->state = RH_LINKS; e->xs = FX(-30); e->xa = FX(-10); }
			else { e->state = RH_RECHTS; e->xs = FX(30); e->xa = FX(10); }
		}
		else if (y < v2 + (e->state == RH_SPECIAL2 ? 260 : 250))
		{
			b3_rm.shotcount -= FX(1);
			if (b3_rm.shotcount <= 0)
			{
				b3_rm_round(e, e->state == RH_SPECIAL3);
				b3_rm.shotcount = e->state == RH_SPECIAL2 ? B3_T(4) : B3_T(0.5);
			}
		}
		break;
	case RH_EXPLODIEREN:                                 /* bursts, faster and faster */
		b3_rm_bounce(e, -77);
		b3_rm.animc += FX(1);
		while (b3_rm.animc > b3_rm.anims)
		{
			if (e->anim & 1)
			{
				fx_add(&P_expl, x - 20 + rnd(140) + 20, y - 20 + rnd(140) + 20, 2);
				sfx(SFX_EXPLODE);
			}
			b3_rm.animc -= b3_rm.anims;
			b3_rm.anims -= FX(0.06);
			if (++e->anim > 9) e->anim = 0;
			if (b3_rm.anims <= FX(0.6)) { e->energy = 0; break; }
		}
		break;
	default:
		break;
	}

	if (e->energy <= 0 && e->state != RH_EXPLODIEREN)
	{
		e->state = RH_EXPLODIEREN;
		e->xs = e->ys = e->xa = 0;
		e->energy = 1;
		b3_rm.destroy = 0;
		b3_rm.anims = B3_T(1);
		b3_rm.animc = 0;
		b3_rm.delay = 0;
		e->anim = 0;
		music(0);
	}
	kinds[E_ROLLMOPS].shootable = (uint8_t)b3_rm.destroy;
	return 0;
}
static int ROLLMOPS_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	fx_add(&P_explbig, ecx(e) - 30, ecy(e) + 30, 3);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int ROLLMOPS_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const int mir = (e->x >> 8) < e->v1 + 320;
	switch (e->state)
	{
	case RH_FIRST: case RH_EINFLIEGEN:
		break;
	case RH_INIT: case RH_STEHEN:                        /* the ball (Hurrican scales it to 120) */
		draw_pic(&b3_ball, e->anim, e->x + FX(36), e->y + FX(20), 0);
		break;
	case RH_OEFFNEN: case RH_SCHLIESSEN:
		draw_pic(&b3_auf, e->anim, e->x, e->y, mir);
		break;
	case RH_VERFOLGEN:
		draw_pic(&b3_gun, 0, e->x + FX(mir ? 71 : 6), e->y + FX(38), mir);
		draw_pic(&b3_auf, 8, e->x, e->y, mir);
		break;
	default:
		draw_pic(&P_ROLLMOPS, e->anim, e->x, e->y, 0);
		break;
	}
	return 1;
}

/* ================================================================ (-1) B3_KETTE
 * (Gegner_Kettenglied.cpp) a link of the snow ball's chain, placed by it: the damage it takes
 * goes to the snow ball (v2). When the chain breaks the links fly apart (bouncing on the
 * ground) and are gone after five seconds. */
static void B3_KETTE_load(void) { }
static void B3_KETTE_init(Ent *e) { (void)e; }
static int B3_KETTE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)touching;
	if (e->state == 0)
	{
		Ent *p = &ents[e->v2];
		if (e->energy < 1000 && p->type == E_ROLLMOPS) p->energy -= (int16_t)(1000 - e->energy);
		e->energy = 1000;
		if (p->type != E_ROLLMOPS) e->type = E_NONE;
		if (e->hit) e->hit--;
		return 1;
	}
	if (e->state == 1)
	{
		e->state = 2;
		e->xs = (int16_t)FX(rnd(40) - 20);
		e->ys = (int16_t)-FX(rnd(40) - 20);
		e->ya = FX(5);
		e->t1 = 306;
	}
	if ((bu & B_WALL) && e->ys > 0)
	{
		e->ys = (int16_t)(e->ys * -77 / 256);
		if (e->ys > FX(-0.1)) e->ys = 0;
	}
	if (--e->t1 <= 0) { e->type = E_NONE; return 1; }
	return 0;
}
static int B3_KETTE_explode(Ent *e) { (void)e; return 1; }
static int B3_KETTE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }
