/*
 * group factory: the spider factory's kinds (stage 5), the flying level's cave (stage 9),
 * the cockroach (stage 6, 9) and the press (stage 2, 5). Hurrican's DoKI, one 60 Hz step each.
 *
 * Hurrican's counters run in units of 1/6 s at 60 Hz (Timer.sync(1) per step = 1/6): a delay of
 * n units is n * 6 steps here, an AnimSpeed a is about a * 6 steps per frame.
 * Left out everywhere: particles (smoke, sparks, splinters, chain parts), glows, rotation of
 * the pictures (the spiders, the cockroach and the falling wall crab are drawn unrotated), the
 * picture flipped upside down (the wall crab crawling up), sounds without an SFX_ equivalent.
 */

/* ------------------------------------------------------------------ helpers */
/* Hurrican's PlayerAbstandHoriz / Vert: the player's box centre to the kind's */
static int factory_hdist(const Ent *e)
{
	const int d = (pl.x >> 8) + 35 - ecx(e);
	return d < 0 ? -d : d;
}
static int factory_vdist(const Ent *e)
{
	const int d = (pl.y >> 8) + 45 - ((e->y >> 8) + ent_box(e)->b / 2);
	return d < 0 ? -d : d;
}
/* the angle (whole degrees 0..359) of the vector whose sine goes with s and cosine with c:
 * atan(r) ~ 45 r + 15.64 r (1 - r) degrees for 0 <= r <= 1 (error < 0.3 degrees) */
static int factory_atan2(int s, int c)
{
	const int as = s < 0 ? -s : s, ac = c < 0 ? -c : c;
	int a;
	if (as == 0 && ac == 0) return 0;
	if (ac >= as)
	{
		const int r = (as << 8) / ac;
		a = (11520 * r + 4004 * r * (256 - r) / 256 + 32768) >> 16;
	}
	else
	{
		const int r = (ac << 8) / as;
		a = 90 - ((11520 * r + 4004 * r * (256 - r) / 256 + 32768) >> 16);
	}
	if (c >= 0) return s >= 0 ? a : (360 - a) % 360;
	return s >= 0 ? 180 - a : 180 + a;
}
/* the climbing spiders' "winkel": the direction to the player as Hurrican's compass angle
 * (0 = up, clockwise), in (0, 360] like Hurrican's quadrant code */
static int factory_winkel(int dx, int dy)
{
	int w = factory_atan2(-dx, dy);
	if (w <= 0) w += 360;
	return w;
}
static int factory_abs(int v) { return v < 0 ? -v : v; }
/* one frame further every per steps (Hurrican's AnimCount / AnimSpeed); returns 1 on a change */
static int factory_tick(Ent *e, int per)
{
	if (++e->animt < per) return 0;
	e->animt = 0;
	return 1;
}
/* off the screen by more than Hurrican's projectiles live */
static int factory_offscreen(int x, int y)
{
	return x < cam_x * 2 - 200 || x > cam_x * 2 + 840 || y < cam_y * 2 - 200 || y > cam_y * 2 + 600;
}
/* Wegschieben: the player touching rectangle r is pushed aside (and loses dmg per step) */
static void factory_push(const Ent *e, const Box *r, fx dmg)
{
	if (pl.state == PS_DEAD) return;
	const int x = e->x >> 8, y = e->y >> 8, px = pl.x >> 8, py = pl.y >> 8;
	if (!boxes_meet(x, y, r, px, py, &player_box)) return;
	if (dmg) player_damage(dmg);
	if (px + 35 < x + r->l + (r->r - r->l) / 2)
	{
		if (!(blk_left(px, py, px, &player_box, 0) & B_WALL)) pl.x -= FX(21) / 6;
	}
	else if (!(blk_right(px, py, px, &player_box, 0) & B_WALL)) pl.x += FX(21) / 6;
}
static Pic factory_laser1, factory_flame;   /* krabblerlaser1.png, flame.png */

/* ------------------------------------------------------------------ GESCHUETZ */
/*
 * The ceiling gun (Gegner_Geschuetz.cpp): closed until the player is near (and not far above
 * it), then opens (frames 1..11), fires a laser straight down, closes (12..24, 0). Energy 50,
 * touching costs 5 / unit. The laser (KRABBLERLASER1) hurts while it passes through the player
 * (8 / unit); here once, 20.
 */
static void GESCHUETZ_load(void) { factory_laser1 = pic("krabblerlaser1.p"); }
static void GESCHUETZ_init(Ent *e) { e->state = 0; }
static int GESCHUETZ_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->dir = 0;
	if (e->state == 1)                       /* opening */
	{
		if (factory_tick(e, 5) && ++e->anim >= 11) e->anim = 11;
		if (e->anim == 11)
		{
			enemy_shot_pic(&factory_laser1, x + 27, y + 36, 0, FX(30), 0, DMG(20), 0);
			e->state = 2;
		}
	}
	else if (e->state == 2)                  /* closing */
	{
		if (factory_tick(e, 5) && ++e->anim >= 25) e->anim = 0;
		if (e->anim == 0) e->state = 0;
	}
	else
	{
		e->anim = 0;
		if ((pl.y >> 8) + 70 >= y && factory_hdist(e) < 400 && factory_vdist(e) < 400)
		{
			e->state = 1; e->anim = 1; e->animt = 0;
		}
	}
	return 0;
}
static int GESCHUETZ_explode(Ent *e) { return 0; }
static int GESCHUETZ_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ the climbing spiders */
/*
 * The climbing spider (Gegner_Climber.cpp) and the fire spider (Gegner_FireSpider.cpp) crawl
 * over the back wall: they turn (rot) towards the player and walk where they look (7.5).
 * The climber shoots a seeking bullet when it looks at the player (nearer than 350); the fire
 * spider stops and breathes fire while the player is nearer than 200..350. Shot, they are
 * thrown off the wall (energy 100 again) and burst on the next wall they hit.
 * rot is kept in t2 (1/16 degree), the turning speed in size (half degrees), the shot delay
 * in t1 (steps). States: 0 start, 1 walking, 2 shooting, 3 thrown off.
 */
static void factory_spider_hit(Ent *e)
{
	e->energy = 100;
	e->state = 3;
	e->xs = (int16_t)FX(rnd(15) - 7);
	e->ys = (int16_t)FX(-rnd(8) - 8);
	e->xa = 0;
	e->ya = FX(3);
	sfx(SFX_EXPLODE);
	fx_add(&P_expl, (e->x >> 8) + 25, (e->y >> 8) + 15, 2);
}
/* turn rot (t2) towards winkel w by inc (1/16 degree per step) */
static void factory_turn(Ent *e, int w, int inc)
{
	const int target = w * 16;
	if (e->t2 < target) { e->t2 = (int16_t)(e->t2 + inc > target ? target : e->t2 + inc); }
	else if (e->t2 > target) { e->t2 = (int16_t)(e->t2 - inc < target ? target : e->t2 - inc); }
}
static void factory_walk(Ent *e, int dist, int bl, int br, int bo, int bu)
{
	if (dist < 1000)
	{
		const int r = e->t2 / 16;
		e->xs = (int16_t)(sin_d(r) * 15 / 2);
		e->ys = (int16_t)(-cos_d(r) * 15 / 2);
	}
	else e->xs = e->ys = 0;
	if (e->xs < 0 && (bl & (B_WALL | B_EWALL))) e->xs = 0;
	if (e->xs > 0 && (br & (B_WALL | B_EWALL))) e->xs = 0;
	if (e->ys < 0 && (bo & (B_WALL | B_EWALL))) e->ys = 0;
	if (e->ys > 0 && (bu & (B_WALL | B_EWALL))) e->ys = 0;
}
static int factory_thrown(Ent *e, int bl, int br, int bo, int bu)
{
	if ((bl | br | bo | bu) & B_WALL) { e->energy = 0; return 1; }
	if (e->ys > FX(60)) e->ys = FX(60);
	return 0;
}

static void CLIMBSPIDER_load(void) { }
static void CLIMBSPIDER_init(Ent *e)
{
	e->state = 0;
	e->anim = 3;
	e->t1 = 60;
	e->t2 = e->v1 == 99 ? (int16_t)(((360 + rnd(50) - 50) % 360) * 16) : 0;
	e->size = (uint8_t)(rnd(8) + 3);
}
static int CLIMBSPIDER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->energy <= 0 && e->state != 3) factory_spider_hit(e);
	if (e->state != 3 && factory_tick(e, 3))
	{
		if (e->state == 2) { if (++e->anim >= 3) { e->state = 1; e->anim = 3; } }
		else if (--e->anim <= 3) e->anim = 13;
	}
	if (e->state == 0)
	{
		if (e->v1 != 99) e->t2 = (int16_t)(factory_winkel(x + 25 - ((pl.x >> 8) + 35), y + 18 - ((pl.y >> 8) + 20)) * 16);
		e->state = 1;
	}
	else if (e->state == 1)
	{
		int w = factory_winkel(x + 25 - ((pl.x >> 8) + 35), y + 18 - ((pl.y >> 8) + 40));
		if (e->v1 == 99 && x - cam_x * 2 < 250 && w > 75) w = 75;
		const int dist = player_distance(e);
		int inc = e->size;                                   /* half degrees per unit */
		if (dist > 250) inc *= 3; else if (dist > 150) inc *= 2;
		if ((bl | br | bo | bu) & B_WALL) inc *= 5;
		factory_turn(e, w, inc * 4 / 3);                     /* x 16 / 2 / 6 */
		if (dist > 300 && e->v1 != 99) e->t2 = (int16_t)(w * 16);
		factory_walk(e, dist, bl, br, bo, bu);
		if (--e->t1 < 0 && e->v1 != 99 && factory_abs(w - e->t2 / 16) < 10 && dist < 350)
		{
			e->t1 = (int16_t)((rnd(10) + 10) * 6);
			e->state = 2; e->anim = 0; e->animt = 0;
			e->xs = e->ys = 0;
			fx xs, ys;
			aim_at(x + 25, y + 17, 20, &xs, &ys);
			enemy_shot(SH_BULLET, x + 21, y + 13, xs, ys, DMG(8));
		}
	}
	else if (e->state == 3)
	{
		if (factory_thrown(e, bl, br, bo, bu)) return 1;
	}
	if (touching && e->state != 3) player_damage(DMG_T(4));
	return 0;
}
static int CLIMBSPIDER_explode(Ent *e) { return 0; }
static int CLIMBSPIDER_draw(const Ent *e, uint32_t t) { return 0; }

static void FIRESPIDER_load(void) { factory_flame = pic("flame.png"); }
static void FIRESPIDER_init(Ent *e) { e->state = 1; e->t1 = 60; e->t2 = 0; }
static int FIRESPIDER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->energy <= 0 && e->state != 3) factory_spider_hit(e);
	if (e->state != 2 && factory_tick(e, 5) && ++e->anim >= 10) e->anim = 0;
	const int w = factory_winkel(x + 25 - ((pl.x >> 8) + 35), y + 18 - ((pl.y >> 8) + 40));
	const int dist = e->state == 3 ? 0 : player_distance(e);
	if (e->state == 1)
	{
		const int inc = dist > 250 ? 12 : dist > 150 ? 8 : 5;   /* degrees per unit */
		factory_turn(e, w, inc * 16 / 6);
		if (dist > 300) e->t2 = (int16_t)(w * 16);
		factory_walk(e, dist, 0, 0, 0, 0);
		if (--e->t1 < 0 && factory_abs(w - e->t2 / 16) < 10 && dist < 200)
		{
			e->t1 = 2;
			e->state = 2; e->anim = 11; e->animt = 0;
			e->xs = e->ys = 0;
		}
	}
	else if (e->state == 2)                  /* fire: until the player is out of the way, 20 units at most */
	{
		e->animt++;
		if (--e->t1 <= 0)
		{
			const int r = e->t2 / 16;
			const int sx = x + 8 + sin_d(r) * 30 / 256, sy = y - 13 - cos_d(r) * 30 / 256;
			fx xs, ys;
			aim_at(sx, sy, 20, &xs, &ys);
			Shot *s = enemy_shot_pic(&factory_flame, sx, sy, xs, ys, 0, DMG(5), 1);
			if (s) s->life = 45;
			e->t1 = 6;                       /* Hurrican: a flame every 3 steps, each hurting while it touches */
		}
		if (factory_abs(w - e->t2 / 16) > 10 || dist > 350 || e->animt > 120)
		{
			e->state = 1; e->anim = 0; e->animt = 0;
			e->t1 = (int16_t)((rnd(20) + 20) * 6);
		}
	}
	else if (factory_thrown(e, bl, br, bo, bu)) return 1;
	if (touching && e->state != 3) player_damage(DMG_T(4));
	return 0;
}
static int FIRESPIDER_explode(Ent *e) { return 0; }
static int FIRESPIDER_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ EIERMANN, EIERBOMBE */
/*
 * The egg man (Gegner_EierMann.cpp) stands and lays an egg bomb at the end of each round of
 * its animation (faster on harder skills) while the player is nearer than 700. The egg
 * (EIERBOMBE projectile) rolls towards the player (18), falls and bounces, bursts on a wall or
 * on the player (40).
 */
static void EIERMANN_load(void) { }
static void EIERMANN_init(Ent *e) { }
static int EIERMANN_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	static const uint8_t per[3] = { 12, 10, 8 };      /* AnimSpeed 1.9 / 1.6 / 1.3 */
	if (factory_tick(e, per[skill]) && ++e->anim >= 10)
	{
		e->anim = 0;
		if (player_distance(e) <= 700)
		{
			Ent *b = ent_new(E_EIERBOMBE, x + 16, y + 40);
			if (b)
			{
				b->active = 1;
				b->xs = (pl.x >> 8) + 40 < x + 36 ? FX(-18) : FX(18);
				b->anim = b->xs < 0 ? 18 : 0;
				b->dir = b->xs > 0;
			}
		}
	}
	return 0;
}
static int EIERMANN_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int EIERMANN_draw(const Ent *e, uint32_t t) { return 0; }

static void EIERBOMBE_load(void) { }
static void EIERBOMBE_init(Ent *e) { }
static int EIERBOMBE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (factory_tick(e, 3))
	{
		if (e->xs < 0) { if (--e->anim < 0) e->anim = 18; }
		else if (++e->anim > 19) e->anim = 0;
	}
	if (e->ys > FX(30)) e->ys = FX(30);
	const int ground = bu & (B_WALL | B_PLATFORM);
	if (!ground && e->ys == 0) e->ya = FX(4);
	if (e->ys > 0 && ground)
	{
		e->ys = (int16_t)(-e->ys * 3 / 4);
		if (e->ys > -FX(1)) { e->ys = 0; e->ya = 0; }
	}
	if ((bl | br) & B_WALL) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(40)); e->energy = 0; return 1; }
	if (factory_offscreen(x, y)) { e->type = E_NONE; return 1; }
	return 0;
}
static int EIERBOMBE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int EIERBOMBE_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ FIESEDRONE */
/*
 * The mean drone (Gegner_FieseDrone.cpp) hovers 100 above the player, turns round (frames
 * 13..17 and back) when he passes under it, and fires an aimed laser (15) every 12..24 units
 * while he is nearer than 600. Its laser picture is not in the packs: the drone bullet.
 * States: 0 flying, 1 turning, 2 turning back.
 */
static void FIESEDRONE_load(void) { }
static void FIESEDRONE_init(Ent *e) { e->t1 = (int16_t)((rnd(12) + 12) * 6); }
static int FIESEDRONE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int px = (pl.x >> 8) + 35, py = pl.y >> 8;
	if (e->state == 0)
	{
		if (factory_tick(e, 9) && ++e->anim >= 12) e->anim = 0;
	}
	else if (e->state == 1)
	{
		if (factory_tick(e, 3) && ++e->anim > 17)
		{
			e->state = 2;
			e->dir ^= 1;
			e->x -= FX(e->dir ? 5 : -5);
			e->anim = 17;
		}
	}
	else if (factory_tick(e, 3) && --e->anim <= 13)
	{
		e->anim = 0; e->state = 0;
	}
	if (x + 30 < px) e->xa = FX(1);
	if (x + 30 > px) e->xa = FX(-1);
	if (y < py - 100) e->ya = FX(1);
	if (y > py - 100) e->ya = FX(-1);
	if (e->xs > FX(10)) e->xs = FX(10);
	if (e->xs < FX(-10)) e->xs = FX(-10);
	if (e->ys > FX(8)) e->ys = FX(8);
	if (e->ys < FX(-8)) e->ys = FX(-8);
	if (e->state == 0 && player_distance(e) < 600 && --e->t1 <= 0)
	{
		e->t1 = (int16_t)((rnd(12) + 12) * 6);
		fx xs, ys;
		aim_at(x + 20, y + 20, 40, &xs, &ys);
		enemy_shot(SH_BULLET, x + 20, y + 20, xs, ys, DMG(15));
	}
	if (e->state == 0 && ((x + 30 < px && e->dir == 0) || (x + 30 > px && e->dir == 1)))
	{
		e->state = 1; e->anim = 13; e->animt = 0;
	}
	return 0;
}
static int FIESEDRONE_explode(Ent *e) { return 0; }
static int FIESEDRONE_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ WANDKRABBE */
/*
 * The wall crab (Gegner_WandKrabbe.cpp) crawls up and down a wall (3) and leaps at the player
 * when he is level with it (within 100); it clings to him (10 at once) until he shakes it off
 * (turning left / right quickly) or dies. Shot (energy 100) or landing after a leap, it is
 * thrown off and bursts when it hits the ground. States: 0 crawling, 1 leaping, 2 falling
 * (shaken off), 3 thrown off (burning), 4 clinging. Clinging: v1 / v2 = its place on the
 * player, spare = the way he looked last.
 */
static void WANDKRABBE_load(void) { }
static void WANDKRABBE_init(Ent *e) { e->ys = FX(3); e->anim = 1; }
static int WANDKRABBE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->state == 0 || e->state == 2)
	{
		if (e->state == 0 && factory_tick(e, 5) && ++e->anim >= 10) e->anim = 1;
	}
	e->dir = (bl & B_WALL) != 0;
	if (((bo & (B_WALL | B_EWALL)) && e->ys < 0) || ((bu & (B_WALL | B_EWALL)) && e->ys > 0 && e->state == 0))
		e->ys = (int16_t)-e->ys;
	switch (e->state)
	{
	case 0:
		if (factory_abs((pl.y >> 8) - y) < 100 && pl.state != PS_DEAD)
		{
			const int ax = (pl.x >> 8) + 35 - (x + 20), ay = (pl.y >> 8) + 40 - (y + 20) - 160;
			e->xs = (int16_t)(FX(ax) / 10);
			e->ys = (int16_t)(FX(ay) / 10);
			e->ya = FX(13) / 4;
			e->state = 1;
		}
		break;
	case 1:
		if (e->ys > FX(20)) e->ys = FX(20);
		break;
	case 2:
	case 3:
		if (e->ys > FX(40)) e->ys = FX(40);
		if (e->state == 3)
		{
			if (((bl & B_WALL) && e->xs < 0) || ((br & B_WALL) && e->xs > 0)) e->xs = (int16_t)-e->xs;
			if (bu & B_WALL) { e->energy = 0; return 1; }
		}
		break;
	case 4:
		e->xo = e->x; e->yo = e->y;
		if (e->hit) e->hit--;                /* the engine's step is skipped */
		e->x = pl.x + FX(e->v1);
		e->y = pl.y + FX(e->v2);
		e->dir = (e->x >> 8) + 15 >= (pl.x >> 8) + 35;
		if (pl.dir != (e->spare ? 1 : -1))
		{
			e->spare = pl.dir > 0;
			e->v2 = (int16_t)(e->v2 + rnd(4) + 2);
		}
		if (e->v2 > 80 || pl.state == PS_DEAD) { e->state = 2; e->ys = FX(5); e->ya = FX(3); }
		break;
	}
	if (e->state == 1 && touching)
	{
		e->xs = e->ys = e->xa = e->ya = 0;
		e->anim = 0;
		player_damage(DMG(10));
		e->state = 4;
		int v1 = x - (pl.x >> 8), v2 = y - (pl.y >> 8);
		e->v1 = (int16_t)(v1 < -10 ? -10 : v1 > 50 ? 50 : v1);
		e->v2 = (int16_t)(v2 < -30 ? -30 : v2 > 80 ? 80 : v2);
		e->spare = pl.dir > 0;               /* a shake: turning the other way */
	}
	if (e->state != 3 && (e->energy <= 0 || ((bu & B_WALL) && e->state != 0)))
	{
		e->energy = 100;
		e->anim = 0;
		e->ys = FX(-15);
		e->ya = FX(3);
		e->xs = (int16_t)(e->dir ? FX(rnd(8) + 4) : -FX(rnd(8) + 4));
		e->state = 3;
		sfx(SFX_EXPLODE);
		fx_add(&P_expl, x, y + 20, 2);
	}
	if (y > cam_y * 2 + 550) { e->energy = 0; return 1; }
	return e->state == 4;                    /* clinging: moves with the player */
}
static int WANDKRABBE_explode(Ent *e) { return 0; }
static int WANDKRABBE_draw(const Ent *e, uint32_t t)
{
	draw_pic(&P_WANDKRABBE, e->anim, e->x, e->y, e->dir == 0);   /* the picture looks right */
	return 1;
}

/* ------------------------------------------------------------------ MUTANT */
/*
 * The mutant (Gegner_Mutant.cpp) crawls towards the player (5) and leaps at him (15, -30)
 * whenever its delay (8..12 units, counting while he is nearer than 400) runs out on the
 * ground. Value1 != 0: it drops out of a tube first. Touching it costs 6 / unit.
 * States: 0 crawling, 1 leaping, 2 dropping in. t1 = the leap delay (steps).
 */
static void MUTANT_load(void) { }
static void MUTANT_init(Ent *e)
{
	if (e->v1 == 0) { e->state = 0; e->anim = 4; }
	else { e->state = 2; e->anim = 1; e->ys = FX(2); e->ya = FX(3); }
}
static int MUTANT_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int ground = bu & (B_WALL | B_PLATFORM);
	if (e->ys > FX(60)) e->ys = FX(60);
	if (e->state == 0)
	{
		if (factory_hdist(e) > 20)
		{
			if (factory_tick(e, 5) && (++e->anim >= 17 || e->anim < 4)) e->anim = 4;
			e->dir = x + 50 < (pl.x >> 8) + 35;
			e->xs = e->dir ? FX(5) : FX(-5);
		}
		else { e->xs = 0; e->anim = 3; }
		if (!ground) e->ya = FX(5);
		else
		{
			e->ys = 0; e->ya = 0;
			if (player_distance(e) < 400) e->t1--;
			if (e->t1 <= 0)
			{
				e->state = 1;
				e->xs = e->dir ? FX(15) : FX(-15);
				e->anim = 18; e->animt = 0;
				e->t1 = (int16_t)((8 + rnd(5)) * 6);
				e->ys = FX(-30);
				e->ya = FX(6);
				sfx(SFX_LAND);
			}
		}
	}
	else if (e->state == 1)
	{
		if (e->anim < 24) { if (factory_tick(e, 9) && ++e->anim >= 26) e->anim = 18; }
		else { e->anim = 24; e->ya = FX(15); }
		if (e->ys > 0 && ground)
		{
			e->state = 0; e->anim = 4; e->animt = 0;
			e->ys = 0; e->ya = 0;
		}
	}
	else
	{
		if (factory_tick(e, e->anim > 2 ? 24 : 18)) e->anim++;
		if ((bu & B_WALL) && e->ys > 0) { e->ys = 0; e->ya = 0; sfx(SFX_LAND); }
		if (e->anim > 3) { e->state = 0; e->anim = 4; }
	}
	if (touching && e->state != 2) player_damage(DMG_T(6));
	return 0;
}
static int MUTANT_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int MUTANT_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ SCHABE */
/*
 * The cockroach (Gegner_Schabe.cpp) runs round the edges of the walls (5), turning at corners
 * and at the map's turning points, and fires a seeking bullet every 8..16 units while the
 * player is nearer than 600. Hurrican turns its picture by 0/90/180/270 degrees; here only the
 * mirroring (spare) is kept. Touching it costs 2 / unit.
 */
static void SCHABE_load(void) { }
static void SCHABE_init(Ent *e)
{
	e->t1 = (int16_t)((5 + rnd(5)) * 6);
	e->xs = e->v1 ? FX(-5) : FX(5);
	e->spare = e->v1 != 0;
}
#define FACTORY_W(dx, dy) (blk(xl + (dx), yl + (dy)) & B_WALL)
static int SCHABE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (player_distance(e) < 600) e->t1--;
	if (e->t1 < 0)
	{
		e->t1 = (int16_t)((8 + (4 - skill) * 2) * 6);
		fx xs, ys;
		aim_at(x + 9, y + 9, 20, &xs, &ys);
		enemy_shot(SH_BULLET, x + 5, y + 5, xs, ys, DMG(8));
	}
	if (factory_tick(e, 3) && ++e->anim >= 10) e->anim = 0;
	const int xl = (x + 10) / 20, yl = (y + 10) / 20;
	const int S = FX(5);
	int snap = 0;
	if (e->xs < 0)
	{
		if (!FACTORY_W(0, 1) && !FACTORY_W(-1, -1) && !FACTORY_W(1, -1) && !FACTORY_W(-1, 1) && FACTORY_W(1, 1))
			{ e->xs = 0; e->ys = (int16_t)S; snap = 1; }
		if (!FACTORY_W(0, -1) && !FACTORY_W(-1, -1) && FACTORY_W(1, -1) && !FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = 0; e->ys = (int16_t)-S; snap = 1; }
	}
	if (e->xs > 0)
	{
		if (!FACTORY_W(0, 1) && !FACTORY_W(-1, -1) && !FACTORY_W(1, -1) && FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = 0; e->ys = (int16_t)S; snap = 1; }
		if (!FACTORY_W(0, -1) && FACTORY_W(-1, -1) && !FACTORY_W(1, -1) && !FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = 0; e->ys = (int16_t)-S; snap = 1; }
	}
	if (e->ys < 0)
	{
		if (!FACTORY_W(1, 0) && !FACTORY_W(-1, -1) && FACTORY_W(1, 1) && !FACTORY_W(-1, 1) && !FACTORY_W(1, -1))
			{ e->xs = (int16_t)S; e->ys = 0; snap = 1; }
		if (!FACTORY_W(-1, 0) && !FACTORY_W(-1, -1) && !FACTORY_W(1, -1) && FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = (int16_t)-S; e->ys = 0; snap = 1; }
	}
	if (e->ys > 0)
	{
		if (!FACTORY_W(1, 0) && !FACTORY_W(-1, -1) && FACTORY_W(1, -1) && !FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = (int16_t)S; e->ys = 0; snap = 1; }
		if (!FACTORY_W(-1, 0) && FACTORY_W(-1, -1) && !FACTORY_W(1, -1) && !FACTORY_W(-1, 1) && !FACTORY_W(1, 1))
			{ e->xs = (int16_t)-S; e->ys = 0; snap = 1; }
	}
	if ((e->xs < 0 && FACTORY_W(-1, 0)) || (e->xs > 0 && FACTORY_W(1, 0)))
	{
		e->xs = 0; snap = 1;
		e->ys = (int16_t)(FACTORY_W(0, -1) ? S : -S);
	}
	else if ((e->ys > 0 && FACTORY_W(0, 1)) || (e->ys < 0 && FACTORY_W(0, -1)))
	{
		e->ys = 0; snap = 1;
		e->xs = (int16_t)(FACTORY_W(-1, 0) ? S : -S);
	}
	if (snap) { e->x = FX(xl * 20); e->y = FX(yl * 20); }
	/* the way it looks, and the turning points */
	if (e->ys < 0)
	{
		e->spare = (FACTORY_W(-1, -1) || FACTORY_W(-1, 0) || FACTORY_W(-1, 1)) != 0;
		if (blk(xl, yl - 1) & B_TURN) e->ys = (int16_t)S;
	}
	else if (e->ys > 0)
	{
		e->spare = !(FACTORY_W(-1, -1) || FACTORY_W(-1, 0) || FACTORY_W(-1, 1));
		if (blk(xl, yl + 1) & B_TURN) e->ys = (int16_t)-S;
	}
	if (e->xs != 0)
	{
		const int floor_ = FACTORY_W(-1, 1) || FACTORY_W(0, 1) || FACTORY_W(1, 1);
		e->spare = (e->xs > 0) != (floor_ != 0);
		if (e->xs > 0 && (blk(xl + 1, yl) & B_TURN)) e->xs = (int16_t)-S;
		else if (e->xs < 0 && (blk(xl - 1, yl) & B_TURN)) e->xs = (int16_t)S;
	}
	return 0;
}
#undef FACTORY_W
static int SCHABE_explode(Ent *e) { return 0; }
static int SCHABE_draw(const Ent *e, uint32_t t)
{
	draw_pic(&P_SCHABE, e->anim, e->x, e->y, e->spare);
	return 1;
}

/* ------------------------------------------------------------------ WANDKANNONE */
/*
 * The wall cannon (Gegner_WandKannone.cpp) looks left (Value2 = 0) or right and fires a laser
 * every Value1 units while the player is nearer than 600, recoiling (frames 0..2).
 */
static void WANDKANNONE_load(void) { }
static void WANDKANNONE_init(Ent *e) { e->t1 = (int16_t)(e->v1 * 6); }
static int WANDKANNONE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->dir = e->v2 != 0;
	if (e->state == 1)
	{
		if (factory_tick(e, 3) && ++e->anim >= 3) { e->anim = 0; e->state = 0; e->t1 = (int16_t)(e->v1 * 6); }
	}
	else if (--e->t1 <= 0)
	{
		e->state = 1; e->anim = 0; e->animt = 0;
		if (player_distance(e) < 600)
		{
			if (e->dir) enemy_shot(SH_LASER, x + 30, y + 15, FX(30), 0, DMG(8));
			else enemy_shot(SH_LASER, x - 18, y + 15, FX(-30), 0, DMG(8));
		}
	}
	return 0;
}
static int WANDKANNONE_explode(Ent *e) { return 0; }
static int WANDKANNONE_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ FLUGKANONE */
/*
 * The flying cannon (Gegner_FlugKanone.cpp) hovers 100 above the player and fires a laser
 * down (40, 15) every 10 / 7.5 / 5 units when he is under it (within 100, nearer than 300).
 */
static void FLUGKANONE_load(void) { factory_laser1 = pic("krabblerlaser1.p"); }
static void FLUGKANONE_init(Ent *e) { }
static int FLUGKANONE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	static const uint8_t delay[3] = { 60, 45, 30 };
	anim_loop(e, 0, 18, 6);
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (x + 30 < px + 35) e->xa = FX(5) / 2;
	if (x + 30 > px + 35) e->xa = -FX(5) / 2;
	if (y < py - 100) e->ya = FX(5) / 2;
	if (y > py - 100) e->ya = -FX(5) / 2;
	if (e->xs > FX(20)) e->xs = FX(20);
	if (e->xs < FX(-20)) e->xs = FX(-20);
	if (e->ys > FX(10)) e->ys = FX(10);
	if (e->ys < FX(-10)) e->ys = FX(-10);
	if (e->t1 > 0) e->t1--;
	if (e->t1 <= 0)
	{
		e->t1 = delay[skill];
		if (factory_abs(px - x) < 100 && player_distance(e) < 300)
			enemy_shot_pic(&factory_laser1, x + 30, y + 40, 0, FX(40), 0, DMG(15), 0);
	}
	return 0;
}
static int FLUGKANONE_explode(Ent *e) { return 0; }
static int FLUGKANONE_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ FLUGSACK */
/*
 * The flying fat man (Gegner_Flugsack.cpp) bobs about, drifts after the player and fires a
 * seeking bullet every 20 / 15 / 12 units while facing him (nearer than 400). Shot, it
 * crashes (energy 40, smoking with explosions) and bursts on the first wall. Value1 = 99:
 * it starts crashing. States: 0 flying, 1 crashing. t1 = the shot delay (steps).
 */
static void FLUGSACK_load(void) { }
static void FLUGSACK_init(Ent *e)
{
	e->xa = FX(2); e->ya = FX(2);
	e->t1 = 6;
	if (e->v1 == 99) e->energy = 0;
}
static int FLUGSACK_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (e->energy <= 0 && e->state != 1)
	{
		e->state = 1;
		e->energy = 40;
		e->ys = FX(3); e->ya = FX(2); e->xa = 0;
		e->xs = e->v1 == 99 ? 0 : e->dir ? FX(5) : FX(-5);
	}
	const int frame = factory_tick(e, e->state ? 3 : 9);
	if (frame && ++e->anim >= 10) e->anim = 0;
	if (e->state == 0)
	{
		if (px + 45 < x + 5) e->dir = 0;
		if (px + 45 > x + 75) e->dir = 1;
		if (e->ys > FX(6)) e->ya = FX(-2);
		if (e->ys < FX(-6)) e->ya = FX(2);
		if (e->xs > FX(5)) e->xa = FX(-2);
		if (e->xs < FX(-5)) e->xa = FX(2);
		if (px + 45 < x + 40 && !(bl & B_WALL)) e->x -= FX(4) / 6;
		if (px + 45 > x + 40 && !(br & B_WALL)) e->x += FX(4) / 6;
		if (py + 45 < y + 45 && !(bo & B_WALL)) e->y -= FX(4) / 6;
		if (py + 45 > y + 45 && !(bu & B_WALL)) e->y += FX(4) / 6;
		if (bo & (B_WALL | B_EWALL)) e->ys = FX(4);
		if (bu & (B_WALL | B_EWALL)) e->ys = FX(-4);
		if (bl & (B_WALL | B_EWALL)) e->xs = FX(4);
		if (br & (B_WALL | B_EWALL)) e->xs = FX(-4);
		if (player_distance(e) <= 400 && ((e->dir == 0 && px + 45 <= x) || (e->dir == 1 && px - 45 >= x)))
		{
			if (--e->t1 < 0)
			{
				static const uint8_t again[3] = { 120, 90, 72 };
				const int sx = e->dir ? x + 53 : x + 17;
				fx xs, ys;
				aim_at(sx + 4, y + 14, 20, &xs, &ys);
				enemy_shot(SH_BULLET, sx, y + 10, xs, ys, DMG(8));
				e->t1 = (int16_t)(e->t1 + again[skill]);
			}
		}
		if (touching) player_damage(DMG_T(4));
	}
	else
	{
		if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM)) { e->energy = 0; return 1; }
		if (e->ys > FX(35)) e->ya = 0;
		if (frame && !(e->anim & 1) && rnd(2) == 0 && player_distance(e) <= 600)
		{
			fx_add(&P_expl, x - 30 + rnd(80) + 30, y - 30 + rnd(70) + 30, 2);
			sfx(SFX_EXPLODE);
		}
	}
	return 0;
}
static int FLUGSACK_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int FLUGSACK_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ PRESSE */
/*
 * The steel press (Trigger_Presse.cpp): its plate goes down (Value2), back up at half the
 * speed, waits Value2 units at the top. Touching the plate costs 10 / unit and pushes the
 * player aside. Not destroyable. States: 0 moving, 1 waiting.
 */
static void PRESSE_load(void) { }
static void PRESSE_init(Ent *e) { e->ys = (int16_t)FX(e->v2); }
static int PRESSE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->dir = 0;
	e->energy = 100;
	if (e->state == 0)
	{
		if (e->ys > 0 && (bu & (B_WALL | B_EWALL)))
		{
			e->ys = (int16_t)(-FX(e->v2) / 2);
			if (player_distance(e) < 600) sfx(SFX_LAND);
			fx_add(&P_smoke, x + 50, y + 250, 3);
		}
		if (bo & B_WALL) { e->state = 1; e->t1 = (int16_t)(e->v2 * 6); e->ys = 0; }
	}
	else if (--e->t1 <= 0)
	{
		e->state = 0;
		e->ys = (int16_t)FX(e->v2);
	}
	factory_push(e, ent_box(e), DMG_T(10));
	return 0;
}
static int PRESSE_explode(Ent *e) { return 1; }
static int PRESSE_draw(const Ent *e, uint32_t t) { return 0; }

/* ------------------------------------------------------------------ PRESSWURST */
/*
 * The spider press (Trigger_Presswurst.cpp) waits in the ceiling until the player is under it
 * (within 200), comes down (10, accelerating) crushing him (500) unless he is in a jump (then
 * he is pushed down), lets off steam, puts a spider bomb out and goes back up. The player is
 * pushed aside by its whole body and cannot jump up through it. Only the part below its place
 * in the ceiling is drawn.
 * t2 = that place (the top at the start). States: 1 waiting, 2 moving, 3 down, 4 steaming.
 */
static void PRESSWURST_load(void) { }
static void PRESSWURST_init(Ent *e)
{
	e->y -= FX(251);
	e->yo = e->y;
	e->t2 = (int16_t)(e->y >> 8);
	e->state = 1;
}
static int PRESSWURST_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	static const Box body = { 28, 0, 157, 357 };
	const Box *b = ent_box(e);
	const int py = pl.y >> 8;
	e->energy = 100;
	e->dir = 0;
	/* the player cannot jump through it from below */
	if (touching && y < py && pl.ys < 0) pl.ys = -pl.ys / 4;
	factory_push(e, &body, 0);
	bo = blk_top(x, y, y, b, 0);
	bu = blk_below(x, y, y, b, 0);
	switch (e->state)
	{
	case 1:
		if (pl.state != PS_DEAD && py > y && factory_hdist(e) < 200 && factory_vdist(e) < 500)
		{
			e->state = 2;
			e->ys = FX(10);
			e->ya = FX(5) / 2;
			sfx(SFX_LAND);
		}
		break;
	case 2:
		if (touching && py > y)
		{
			if (pl.state != PS_JUMP && pl.safe_t <= 0) player_damage(FX(500));
			else pl.y = FX(y + b->b);
		}
		if (e->ys > 0 && (bu & (B_WALL | B_EWALL)))
		{
			if (touching && py > y && pl.safe_t <= 0) player_damage(FX(500));
			e->ys = 0; e->ya = 0;
			e->t1 = 30;
			sfx(SFX_BIGEXPLODE);
			e->state = 3;
		}
		if (e->ys < 0 && (bo & (B_WALL | B_EWALL))) { e->ys = 0; e->ya = 0; e->state = 1; }
		if (e->ys < 0 && y <= e->t2) { e->y = FX(e->t2); e->ys = 0; e->ya = 0; e->state = 1; return 1; }
		break;
	case 3:
		if (--e->t1 <= 0) { e->state = 4; e->t1 = 28 * 6; }
		break;
	case 4:
		if (e->t1 % 30 == 0) fx_add(&P_smoke, x + 30 + rnd(2) * 105, y + 300, 4);
		if (--e->t1 < 0)
		{
			Ent *s = ent_new(E_SPIDERBOMB, x + 60, y + 357 - 34);
			if (s) { s->active = 1; s->ya = FX(5); s->dir = pcx() > x + 85; }
			e->state = 2;
			e->ys = FX(-15);
			e->ya = FX(1) / 5;
			sfx(SFX_LAND);
		}
		break;
	}
	return 0;
}
static int PRESSWURST_explode(Ent *e) { return 1; }
static int PRESSWURST_draw(const Ent *e, uint32_t t)
{
	/* only the part below its place in the ceiling (Hurrican: SetRect from oldy - yPos + 251):
	 * the view's top is moved down to that line while it is drawn - when its bottom is in the
	 * view (else the whole picture, as when the line is above the view) */
	const int clip = (e->t2 + 251) >> 1, bottom = (((e->y >> 8) + 357) >> 1) + 1;
	if (clip >= view_y + PLAY_H) return 1;
	if (clip > view_y && bottom <= view_y + PLAY_H)
	{
		const int save = view_y;
		view_y = clip;
		draw_pic(&P_PRESSWURST, 0, e->x, e->y, 0);
		view_y = save;
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ LAFASSSPAWNER, LAFASS, LAFASSFALL */
/*
 * The barrel spawner (Trigger_LaFassSpawner.cpp) sends a barrel hanging from the overhead
 * track (Gegner_LaFass.cpp) left (Value1 = 0) or right (5) every Value2 units (40) while the
 * player is nearer than 800. A barrel that reaches a wall is gone; shot (energy 1), it lets go
 * and the falling barrel (LAFASSSHOT, the player's weapon: 8000 to the enemy it hits) bursts on
 * the first wall.
 */
static void LAFASSSPAWNER_load(void) { }
static void LAFASSSPAWNER_init(Ent *e)
{
	if (e->v2 == 0) e->v2 = 40;
	e->t1 = (int16_t)(e->v2 * 6);
}
static int LAFASSSPAWNER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (player_distance(e) < 800) e->t1--;
	if (e->t1 <= 0)
	{
		e->t1 = (int16_t)(e->v2 * 6);
		Ent *f = ent_new(E_LAFASS, x, y);
		if (f)
		{
			f->active = 1;
			f->xs = e->v1 == 0 ? FX(-5) : FX(5);
			f->anim = (int16_t)rnd(15);
		}
	}
	return 1;
}
static int LAFASSSPAWNER_explode(Ent *e) { return 1; }
static int LAFASSSPAWNER_draw(const Ent *e, uint32_t t) { return 1; }

static void LAFASS_load(void) { }
static void LAFASS_init(Ent *e) { e->xs = (int16_t)FX(e->v1); }
static int LAFASS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->dir = 0;
	if (factory_tick(e, 7) && ++e->anim >= 16) e->anim = 0;
	if ((bl | br) & B_WALL) { e->type = E_NONE; return 1; }
	return 0;
}
static int LAFASS_explode(Ent *e)
{
	Ent *f = ent_new(E_LAFASSFALL, (e->x >> 8) + 7, (e->y >> 8) + 90);
	if (f) { f->active = 1; f->xs = e->xs; f->ya = FX(5); }
	fx_add(&P_smoke, (e->x >> 8) + 33, (e->y >> 8) + 40, 3);
	sfx(SFX_HIT);
	return 1;
}
static int LAFASS_draw(const Ent *e, uint32_t t) { return 0; }

static void LAFASSFALL_load(void) { }
static void LAFASSFALL_init(Ent *e) { e->ya = FX(5); }
static int LAFASSFALL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 13, 6);
	if (e->ys > FX(60)) e->ya = 0;
	if ((bl | br | bo | bu) & B_WALL) { e->energy = 0; return 1; }
	/* the enemies on the screen it falls on */
	const Box *b = ent_box(e);
	for (int k = 0; k < ntargets; k++)
	{
		Ent *o = &ents[targets[k]];
		if (o == e || o->type == E_NONE || o->type == E_LAFASS || o->energy <= 0) continue;
		if (!boxes_meet(x, y, b, o->x >> 8, o->y >> 8, ent_box(o))) continue;
		o->energy = (int16_t)(o->energy > 8000 ? o->energy - 8000 : 0);
		o->hit = 4;
		e->energy = 0;
		return 1;
	}
	if (factory_offscreen(x, y)) { e->type = E_NONE; return 1; }
	return 0;
}
static int LAFASSFALL_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int LAFASSFALL_draw(const Ent *e, uint32_t t) { return 0; }
