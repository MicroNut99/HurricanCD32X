/*
 * The ice group (stages 3, 5-9): side rockets, ski walkers, mini dragons, gun pots, icicles,
 * rail creatures, gun drones, water mines, "Zitrone" drones, snow balls, ice spikes, and the
 * floating platforms and the lift. Each kind follows its Gegner_*.cpp / Trigger_*.cpp DoKI.
 *
 * Shared here: the extra pictures (the drone gun and the shots), a small atan, the base
 * movement for kinds that carry the player (ice_move), and the carrying (ice_carry).
 * Timers are kept in 60 Hz steps (Hurrican's units x 6).
 */

static Pic ice_stiel, ice_such, ice_such2, ice_rotz, ice_flaser, ice_snowsmall;

/* atan(n / d) in 1/16 degrees (-1440..1440), d != 0: 45x + 15.6x(1 - x) per octant */
static int ice_atan16(int n, int d)
{
	const int an = n < 0 ? -n : n, ad = d < 0 ? -d : d;
	int r;
	if (an <= ad)
	{
		const int t = an * 256 / ad;
		r = (t * 720 + t * (256 - t) * 250 / 256) / 256;
	}
	else
	{
		const int t = ad * 256 / an;
		r = 1440 - (t * 720 + t * (256 - t) * 250 / 256) / 256;
	}
	return ((n < 0) != (d < 0)) ? -r : r;
}

/* PlattformTest: the player lands on it (only when he stands on nothing else) */
static void ice_platform(Ent *e, int idx)
{
	if (pl.platform == 0) ent_platform(e, idx);
}

/* GegnerClass::Run for kinds that move themselves (to carry the player afterwards) */
static void ice_move(Ent *e, int idx, int bl, int br, int bo, int bu)
{
	e->xo = e->x; e->yo = e->y;
	e->xs += SYNC(e->xa);
	e->ys += SYNC(e->ya);
	if ((e->xs < 0 && !(bl & B_WALL)) || (e->xs > 0 && !(br & B_WALL))) e->x += SYNC(e->xs);
	if ((e->ys < 0 && !(bo & B_WALL)) || (e->ys > 0 && !(bu & B_WALL))) e->y += SYNC(e->ys);
	if (e->hit) e->hit--;
	const Box *b = ent_box(e);
	if ((e->x >> 8) + b->r < 0 || (e->y >> 8) + b->b < 0 || (e->x >> 8) > map_w * 20 || (e->y >> 8) > map_h * 20)
	{
		if (pl.platform == idx + 1) pl.platform = 0;
		e->type = E_NONE;
	}
}

/* PlattformTest's carrying: the player standing on it moves with it (not into walls) */
static void ice_carry(Ent *e, int idx)
{
	if (pl.platform != idx + 1 || pl.state == PS_DEAD || e->type == E_NONE) return;
	const int dx = e->x - e->xo;
	if (dx)
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		if ((dx > 0 && !(blk_right(px, py, px, &player_box, 0) & B_WALL)) ||
		    (dx < 0 && !(blk_left(px, py, px, &player_box, 0) & B_WALL)))
			pl.x += dx;
	}
	pl.y = e->y + FX(ent_box(e)->t - player_box.b - 1);
}

/* the player's centre against the kind's box centre (PlayerAbstandHoriz / Vert) */
static int ice_dx(const Ent *e)
{
	const Box *b = ent_box(e);
	const int d = pcx() - ((e->x >> 8) + (b->l + b->r) / 2);
	return d < 0 ? -d : d;
}
static int ice_dy(const Ent *e)
{
	const int d = (pl.y >> 8) + 45 - ((e->y >> 8) + ent_box(e)->b / 2);
	return d < 0 ? -d : d;
}

/* a laser in direction w (degrees, Hurrican's FLUGLASER: speed 40, damage 15): the walker's
 * flat laser when it flies mostly sideways, the upright one otherwise (no turned pictures) */
static void ice_laser(int x, int y, int w)
{
	const fx xs = cos_d(w) * 40, ys = sin_d(w) * 40;
	if ((xs < 0 ? -xs : xs) > (ys < 0 ? -ys : ys)) enemy_shot(SH_LASER, x - 12, y + 14, xs, ys, DMG(15));
	else enemy_shot_pic(&ice_flaser, x, y, xs, ys, 0, DMG(15), 0);
}

/* ================================================================ 62 SIDEROCKET
 * (Gegner_SideRocket.cpp) hangs on a wall facing left (value1 0) or right (1); when the player
 * comes level with it (within 100 up / down, 400 across) on its side, it counts value2 (10 if
 * 0) tenths of a second and flies off (20, +5 per unit, at most 30); bursts against a wall.
 * Touching the player costs 25 and bursts it. Left out: the exhaust smoke. */
static void SIDEROCKET_load(void)
{
	ice_stiel = pic("Zitronestiel.png");     ice_such = pic("suchschuss.png");
	ice_such2 = pic("suchschuss2.png");      ice_rotz = pic("rotzshot.png");
	ice_flaser = pic("flugkanonelaser.png"); ice_snowsmall = pic("snowbombsmall.png");
}
static void SIDEROCKET_init(Ent *e) { if (e->v2 == 0) e->v2 = 10; }
static int SIDEROCKET_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bo; (void)bu;
	e->dir = e->v1 != 0;
	if (e->state == 0)
	{
		e->t1 = (int16_t)(e->v2 * 6);
		if (ice_dy(e) < 100 && ice_dx(e) < 400 && ((e->v1 == 0 && x > (pl.x >> 8)) || (e->v1 && x < (pl.x >> 8))))
			e->state = 1;
	}
	else if (e->state == 1)
	{
		if (--e->t1 <= 0)
		{
			e->state = 2;
			e->xs = e->v1 ? FX(20) : FX(-20);
			e->xa = e->v1 ? FX(5) : FX(-5);
			sfx(SFX_ESHOT);
		}
	}
	else
	{
		if (e->xs < FX(-30)) e->xs = FX(-30);
		if (e->xs > FX(30)) e->xs = FX(30);
		if ((e->v1 == 0 && (bl & B_WALL)) || (e->v1 && (br & B_WALL))) e->energy = 0;
	}
	if (touching) { player_damage(DMG(25)); e->energy = 0; }
	return 0;
}
static int SIDEROCKET_explode(Ent *e) { (void)e; return 0; }
static int SIDEROCKET_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 29 SKIWALKER
 * (Gegner_SkiWalker.cpp) races along on skis (20), turns at walls, shoots its laser at the
 * player ahead of it (within 220, every 1-1.5 s); touching it costs 4 a tenth. Jumped on, it
 * throws the player up, hops (-30) and then races off at 43 until a wall bursts it - while
 * it hops and races the player can stand on it and ride it. */
static void SKIWALKER_load(void) { }
static void SKIWALKER_init(Ent *e) { e->xs = FX(20); }
static int SKIWALKER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const Box *b = ent_box(e);
	anim_loop(e, 0, 10, e->state >= 2 ? 2 : 6);
	if ((br & (B_WALL | B_EWALL)) && e->xs > 0)
	{
		if (e->state == 3 && (br & B_WALL)) e->energy = 0;
		e->dir = 0; e->xs = (int16_t)-e->xs;
	}
	if ((bl & (B_WALL | B_EWALL)) && e->xs < 0)
	{
		if (e->state == 3 && (bl & B_WALL)) e->energy = 0;
		e->dir = 1; e->xs = (int16_t)-e->xs;
	}
	ice_platform(e, idx);
	if (pl.platform == idx + 1 && e->state <= 1 && pl.ys > 0)
	{
		/* jumped on: the player is thrown up */
		pl.platform = 0;
		pl.jump_ok = 0; pl.anim = 0; pl.state = PS_JUMP;
		pl.jumpstart = pl.y; pl.ys = FX(-43); pl.jumpadd = 0;
		e->state = 2; e->animt = 0;
		e->xs = 0; e->ys = FX(-30); e->ya = FX(5);
		sfx(SFX_HIT);
	}
	switch (e->state)
	{
	case 0:                                          /* racing */
		if (!(bu & (B_WALL | B_PLATFORM))) { e->ya = FX(5); e->y -= FX(1); e->state = 1; }
		if (player_distance(e) <= 220 &&
		    ((e->dir == 0 && (pl.x >> 8) + 45 <= x) || (e->dir == 1 && (pl.x >> 8) - 45 >= x)))
			if (--e->t1 <= 0)
			{
				e->t1 = (int16_t)(60 + rnd(30));
				if (e->dir) enemy_shot(SH_LASER, x + 40, y + 9, FX(30), 0, DMG(8));
				else enemy_shot(SH_LASER, x - 18, y + 9, FX(-30), 0, DMG(8));
			}
		break;
	case 1:                                          /* falling */
		if (bu & (B_WALL | B_PLATFORM))
		{
			if (e->ys > 0) e->ys = (int16_t)(-e->ys / 2);
			if (e->ys > FX(-2))
			{
				int ny;
				if (blk_below(x, y, y, b, &ny) & (B_WALL | B_PLATFORM)) e->y = FX(ny);
				e->ys = 0; e->ya = 0; e->state = 0;
			}
		}
		break;
	case 2:                                          /* hopping */
		if (e->ys > FX(38)) e->ys = FX(38);
		if (e->ys > 0 && (bu & (B_WALL | B_PLATFORM)))
		{
			int ny;
			e->state = 3; e->ya = 0; e->ys = 0;
			e->xs = e->dir ? FX(43) : FX(-43);
			if (blk_below(x, y, y, b, &ny) & (B_WALL | B_PLATFORM)) e->y = FX(ny);
		}
		break;
	default:                                         /* racing off */
		if (!(bu & (B_WALL | B_PLATFORM))) { e->ya = FX(1.6); e->state = 2; }
		break;
	}
	if (e->state <= 1 && touching) player_damage(DMG_T(4));
	ice_move(e, idx, bl, br, bo, bu);
	ice_carry(e, idx);
	return 1;
}
static int SKIWALKER_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int SKIWALKER_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 78 MINIDRAGON
 * (Gegner_Minidragon.cpp) a dragon's head on a neck out of the wall: when the player comes
 * within 300 it darts out, then sways towards him (at most 80 from its base) and spits a
 * fireball (50, damage 15) to his side now and then; touching it costs 4 a tenth. Shot down
 * to 100 of its 200 it bursts piece by piece, head first. Kept: base (v1, v2), t1 the shot
 * delay (x60), t2 the dart timer, size the pieces left.
 * Drawn: the head and three of the six neck pieces. Left out: the variant the dragon boss
 * throws (value2 99), the sparks; the fireball is the plant's (Hurrican turns fireball.png). */
static void MINIDRAGON_load(void) { }
static void MINIDRAGON_init(Ent *e) { e->t1 = 240; e->size = 6; }
static int MINIDRAGON_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	if ((e->ys < 0 && (bo & B_WALL)) || (e->ys > 0 && (bu & B_WALL))) e->ys = (int16_t)-e->ys;
	if ((e->xs < 0 && (bl & B_WALL)) || (e->xs > 0 && (br & B_WALL))) e->xs = (int16_t)-e->xs;
	if (e->state != 3 && e->state != 0 && e->energy < 100)
	{
		e->state = 3; e->energy = 1; e->t1 = 0;
		e->xs = e->ys = e->xa = e->ya = 0;
		fx_add(&P_explbig, x + 30, y + 23, 2);
		sfx(SFX_BIGEXPLODE);
	}
	e->dir = (pl.x >> 8) + 35 >= x + 30;
	switch (e->state)
	{
	case 0:                                          /* waiting in the wall: not hurt */
		e->energy = 200;
		e->v1 = (int16_t)x; e->v2 = (int16_t)y;
		if (ice_dx(e) < 300 && ice_dy(e) < 300)
		{
			e->state = 1;
			if ((pl.x >> 8) + 35 < x + 30) { e->xs = FX(10); e->xa = FX(-2); }
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
			if (xd < -80) xd = -80;
			if (xd > 80) xd = 80;
			if (yd < -80) yd = -80;
			if (yd > 80) yd = 80;
			e->xa = x < e->v1 + xd ? FX(2) : FX(-2);
			e->ya = y < e->v2 + yd ? FX(2) : FX(-2);
			if (e->xs < FX(-8)) e->xs = FX(-8);
			if (e->xs > FX(8)) e->xs = FX(8);
			if (e->ys < FX(-8)) e->ys = FX(-8);
			if (e->ys > FX(8)) e->ys = FX(8);
		}
		break;
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
		return 1;
	}
	if (touching) player_damage(DMG_T(4));
	return 0;
}
static int MINIDRAGON_explode(Ent *e) { (void)e; return 1; }
static int MINIDRAGON_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const Pic *p = &P_MINIDRAGON;
	if (e->state)
	{
		const int32_t dx = (FX(e->v1) - e->x) / 6, dy = (FX(e->v2) - e->y) / 6;
		for (int i = 0; i < 6 && i < e->size; i += 2)
			draw_pic(p, 4, e->x + (6 - i) * dx, e->y + (6 - i) * dy, 0);
	}
	if (e->size == 6)
	{
		const int sd = e->t1 / 60;
		const int a = e->state == 2 ? (sd == 0 ? 1 : sd == 8 || sd == 7 ? 2 : sd == 6 ? 1 : 0) : 0;
		draw_pic(p, a, e->x + (e->dir ? FX(15) : FX(-15)), e->y, !e->dir);
	}
	return 1;
}

/* ================================================================ 81 ROTZPOTT
 * (Gegner_Rotzpott.cpp) a gun pot: turns its gun to the player (5 degrees a step) and, when
 * near (400) and nearly aimed, shoots - in an arc (ROTZSHOT, 12) when he is below it, else a
 * laser (FLUGLASER, 15). Touching it costs 2 a tenth. Kept: anim = the gun's angle, animt =
 * the wanted one (1/16 degrees), t1 the shot delay, size the turning direction.
 * Drawn: the body and the gun unturned (Hurrican turns the gun). Left out: the muzzle flame. */
static void ROTZPOTT_load(void) { }
static void ROTZPOTT_init(Ent *e) { e->t1 = 30; }
static int ice_wrap16(int w) { while (w < 0) w += 5760; while (w >= 5760) w -= 5760; return w; }
static int ROTZPOTT_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	const int below = (pl.y >> 8) > y + 30;
	int g = ice_wrap16(e->anim), nw;
	const int dx = (x + 27) - ((pl.x >> 8) + 35), dy = (y + 20) - ((pl.y >> 8) + 40);
	if (below) nw = dx > 0 ? dx * 123 / 64 + dy * 34 / 64 : dx * 123 / 64 - dy * 34 / 64;
	else
	{
		const int w = ice_atan16(dx, dy ? dy : 1);
		if (dx >= 0 && dy >= 0) nw = w;
		else if (dx > 0 && dy < 0) nw = 2880 + w;
		else if (dx < 0 && dy > 0) nw = 5760 + w;
		else nw = 2880 + w;
	}
	nw = ice_wrap16(nw);
	if (nw > g) e->size = nw - g > 2880 ? 1 : 2;
	if (nw < g) e->size = g - nw < 2880 ? 1 : 2;
	if (e->size == 1) { g -= 80; if (g < nw) g = nw; }
	else if (e->size == 2) { g += 80; if (g > nw) g = nw; }
	e->anim = (int16_t)g;
	e->animt = (int16_t)nw;
	if (player_distance(e) < 400 && abs(g - nw) < 160) e->t1--;
	if (e->t1 <= 0)
	{
		const int gd = g / 16, a = 180 - gd;
		if (below)
		{
			e->t1 = 54;
			const int w = 360 - gd + (rnd(10) - 5) / 2 - 90;
			enemy_shot_pic(&ice_rotz, x + 20 - sin_d(a) * 25 / 256, y - 12 + cos_d(a) * 20 / 256,
			               cos_d(w) * 40, sin_d(w) * 30, FX(5) / 6, DMG(12), 0);
		}
		else
		{
			e->t1 = 42;
			ice_laser(x + 24 - sin_d(a) * 45 / 256, y - 12 + cos_d(a) * 45 / 256, 360 - gd - 90);
		}
	}
	return 0;
}
static int ROTZPOTT_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int ROTZPOTT_draw(const Ent *e, uint32_t t)
{
	(void)t;
	draw_pic(&ice_stiel, 0, e->x + FX(20), e->y - FX(28), 0);
	draw_pic(&P_ROTZPOTT, 0, e->x, e->y, 0);
	return 1;
}

/* ================================================================ 59 EISZAPFEN
 * (Gegner_Eiszapfen.cpp) an icicle in the ceiling: falls (30, +5, at most 40) when the player
 * passes under it (within 60 of it), bursts on the ground. Touching it costs 30 and bursts
 * it. Not shootable. value1 1: falling from the start. Left out: the snow particles. */
static void EISZAPFEN_load(void) { }
static void EISZAPFEN_init(Ent *e) { if (e->v1 == 1) { e->state = 1; e->ys = FX(30); e->ya = FX(5); } }
static int EISZAPFEN_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo;
	if (e->state == 0)
	{
		const int px = (pl.x >> 8) + 35;
		if ((pl.y >> 8) > y && px > x + 10 - 60 && px < x + 10 + 60)
		{
			e->state = 1; e->ys = FX(30); e->ya = FX(5);
			sfx(SFX_LAND);
		}
	}
	else
	{
		if (e->ys > FX(40)) { e->ys = FX(40); e->ya = 0; }
		if (bu & B_WALL) e->energy = 0;
	}
	if (touching) { player_damage(DMG(30)); e->energy = 0; }
	return 0;
}
static int EISZAPFEN_explode(Ent *e)
{
	fx_add(&P_smoke, ecx(e), (e->y >> 8) + 70, 3);
	sfx(SFX_EXPLODE);
	return 1;
}
static int EISZAPFEN_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 51 SCHIENENVIECH
 * (Gegner_SchienenViech.cpp) runs on a rail towards the player (4 a unit, at most 20; 15 when
 * he is near), turns at walls; every turn of its wheels (the animation run forwards or back
 * by its direction) it shoots straight up or down at him (20, damage 8). No contact damage. */
static void SCHIENENVIECH_load(void) { }
static void SCHIENENVIECH_init(Ent *e) { e->xa = FX(4); }
static int SCHIENENVIECH_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	const Box *b = ent_box(e);
	const int wl = blk_left(x, y, x, b, 0), wr = blk_right(x, y, x, b, 0);
	if (((wl & (B_EWALL | B_WALL)) && e->xs < 0) || (((wr & B_EWALL) || (wl & B_WALL)) && e->xs > 0))
	{
		e->state = 0;
		e->xs = (int16_t)-e->xs;
		e->xa = (int16_t)-e->xa;
	}
	if (++e->animt >= 6)
	{
		int shoot = 0;
		e->animt = 0;
		if (e->xs < 0) { if (++e->anim >= 4) { e->anim = 0; shoot = 1; } }
		else if (--e->anim < 0) { e->anim = 4; shoot = 1; }
		if (shoot)
		{
			if ((pl.y >> 8) < y) enemy_shot_pic(&ice_such, x + 18, y - 8, 0, FX(-20), 0, DMG(8), 1);
			else enemy_shot_pic(&ice_such, x + 18, y + 48, 0, FX(20), 0, DMG(8), 1);
		}
	}
	const int pdx = pcx() - (x + 24);
	if (e->state == 0)
	{
		if ((e->xs > 0 && pdx > 0) || (e->xs < 0 && pdx < 0)) e->state = 1;
	}
	else if (abs(pdx) < 250) e->xa = pdx > 0 ? FX(4) : FX(-4);
	if (e->xs > FX(20)) e->xs = FX(20);
	if (e->xs < FX(-20)) e->xs = FX(-20);
	if (abs(pdx) < 60)
	{
		if (e->xs > FX(15)) { e->xa = 0; e->xs = FX(15); }
		if (e->xs < FX(-15)) { e->xa = 0; e->xs = FX(-15); }
	}
	return 0;
}
static int SCHIENENVIECH_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int SCHIENENVIECH_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 56 BALLERDRONE
 * (Gegner_BallerDrone.cpp) a fat drone: flies to 140 above the player (2.5 a unit, at most
 * 20 across, 10 up and down), leaning by its speed, and fires two aimed shots (20, damage 12)
 * every 1.5 s; touching it costs 4 a tenth. Shot down it drops (+3) with 100 more energy and
 * bursts when it hits anything. Left out: the smoke, the red flare and tint. */
static void BALLERDRONE_load(void) { }
static void BALLERDRONE_init(Ent *e) { e->t1 = 60; }
static int BALLERDRONE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	if (e->energy <= 0 && e->state == 0)
	{
		e->state = 1; e->energy = 100;
		e->xa = 0; e->ya = FX(3);
		fx_add(&P_explbig, x + 45, y + 30, 2);
		sfx(SFX_EXPLODE);
	}
	if (e->xs > FX(20)) e->xs = FX(20);
	if (e->xs < FX(-20)) e->xs = FX(-20);
	if (e->ys < FX(-10)) e->ys = FX(-10);
	{
		int a = 6 + e->xs / 512;
		e->anim = (int16_t)(a < 0 ? 0 : a > 12 ? 12 : a);
	}
	if (e->state == 0)
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		if (x + 45 < px + 35) e->xa = FX(2.5);
		if (x + 45 > px + 35) e->xa = FX(-2.5);
		if (y < py - 140) e->ya = FX(2.5);
		if (y > py - 140) e->ya = FX(-2.5);
		if (e->ys > FX(10)) e->ys = FX(10);
		if (player_distance(e) < 800 && --e->t1 <= 0)
		{
			fx xs, ys;
			e->t1 = 90;
			aim_at(x + 32, y + 70, 20, &xs, &ys);
			enemy_shot_pic(&ice_such2, x + 12, y + 50, xs, ys, 0, DMG(12), 0);
			aim_at(x + 84, y + 70, 20, &xs, &ys);
			enemy_shot_pic(&ice_such2, x + 64, y + 50, xs, ys, 0, DMG(12), 1);
		}
		if (touching) player_damage(DMG_T(4));
	}
	else if (((bo | bu | bl | br) & B_WALL) || (bu & B_PLATFORM)) e->energy = 0;
	return 0;
}
static int BALLERDRONE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int BALLERDRONE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 52 WASSERMINE
 * (Gegner_WasserMine.cpp) a mine bobbing in the water: touching it costs 50 and sets it off.
 * Not shootable. Left out: the water splash and the screen shake. */
static void WASSERMINE_load(void) { }
static void WASSERMINE_init(Ent *e) { (void)e; }
static int WASSERMINE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu;
	anim_loop(e, 0, 9, 24);
	if (touching) { player_damage(DMG(50)); e->energy = 0; }
	return 1;
}
static int WASSERMINE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int WASSERMINE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 80 ZITRONE
 * (Gegner_Zitrone.cpp) a drone that follows the player: picks a point 30-90 beside him and
 * 80-160 above, flies there (5 a unit, at most 20, braking 7), hovers 2 s (at once when he is
 * further than 300) firing its gun at him every half second (FLUGLASER, 15); it wobbles up
 * and down. Shot down it falls (+2) with 200 more energy and bursts when it hits anything.
 * Kept: anim = the gun's angle (1/16 degrees, 100-260), animt = the wobble (1/16 degrees),
 * t1 the hover time, t2 the shot delay, v1 / v2 the point. Drawn: the body and the gun
 * unturned. Left out: the jet flares and the smoke. */
static void ZITRONE_load(void) { }
static void ZITRONE_init(Ent *e) { e->anim = 180 * 16; }
static int ice_zitrone_frame(const Ent *e)
{
	const int a = 5 - ((pl.x >> 8) + 35 - ((e->x >> 8) + 57)) / 50;
	return a < 0 ? 0 : a > 10 ? 10 : a;
}
static int ZITRONE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)touching;
	if (e->energy <= 0 && e->state != 2)
	{
		e->state = 2; e->energy = 200;
		e->ys = FX(4); e->ya = FX(2); e->xa = 0;
		e->xs = rnd(2) ? FX(5) : FX(-5);
		fx_add(&P_explbig, x + 50, y + 40, 2);
		sfx(SFX_EXPLODE);
	}
	/* the gun turns towards the player */
	{
		int yd = (pl.y >> 8) - y;
		const int xd = (pl.x >> 8) + 35 - (x + 60);
		if (yd == 0) yd = 1;
		int w = ice_atan16(xd, yd) + 2880;
		if (xd > 0 && yd < 0) w += 2880;
		else if (xd < 0 && yd > 0) w += 5760;
		else if (xd < 0 && yd < 0) w += 2880;
		w = ice_wrap16(w);
		if (w < 1600) w = 1600;
		if (w > 4160) w = 4160;
		if (e->anim < w) e->anim += 53;
		if (e->anim > w) e->anim -= 53;
	}
	e->animt = (int16_t)((e->animt + 38) % 5760);
	if ((bl | br) & B_WALL) e->xs = 0;
	if ((bo | bu) & B_WALL) e->ys = 0;
	switch (e->state)
	{
	case 0:                                          /* hovering */
		if (player_distance(e) > 300) e->t1 = 0;
		if (--e->t1 < 0)
		{
			int d = rnd(60) + 30;
			if (rnd(2) == 0) d = -d;
			e->v1 = (int16_t)((pl.x >> 8) + d);
			e->v2 = (int16_t)((pl.y >> 8) - rnd(80) - 80);
			e->xa = e->v1 > x ? FX(5) : FX(-5);
			e->ya = e->v2 > y ? FX(5) : FX(-5);
			e->state = 1;
		}
		if (--e->t2 < 0)
		{
			const int k = e->anim / 16, a = 180 - k;
			e->t2 = 30;
			ice_laser(x + 53 - sin_d(a) * 45 / 256, y + 56 + cos_d(a) * 45 / 256 + sin_d(e->animt / 16) * 10 / 256, 360 - k - 90);
		}
		break;
	case 1:                                          /* flying to the point */
		if (e->xs > FX(20)) e->xs = FX(20);
		if (e->xs < FX(-20)) e->xs = FX(-20);
		if (e->ys > FX(20)) e->ys = FX(20);
		if (e->ys < FX(-20)) e->ys = FX(-20);
		if (e->xs < 0 && x < e->v1) { e->xa = 0; e->xs += SYNC(FX(7)); if (e->xs > 0) e->xs = 0; }
		if (e->xs > 0 && x > e->v1) { e->xa = 0; e->xs -= SYNC(FX(7)); if (e->xs < 0) e->xs = 0; }
		if (e->ys < 0 && y < e->v2) { e->ya = 0; e->ys += SYNC(FX(7)); if (e->ys > 0) e->ys = 0; }
		if (e->ys > 0 && y > e->v2) { e->ya = 0; e->ys -= SYNC(FX(7)); if (e->ys < 0) e->ys = 0; }
		if (e->xs == 0 && e->ys == 0) { e->state = 0; e->t1 = 120; e->t2 = 30; }
		break;
	default:                                         /* falling */
		if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM)) e->energy = 0;
		if (e->ys > FX(35)) e->ya = 0;
		break;
	}
	return 0;
}
static int ZITRONE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int ZITRONE_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const int f = ice_zitrone_frame(e), a = f < 3 ? 3 : f > 7 ? 7 : f;
	const int32_t yo = e->y + sin_d(e->animt / 16) * 10;
	draw_pic(&ice_stiel, 0, e->x + FX(73 - a * 5), yo + FX(48), 0);
	draw_pic(&P_ZITRONE, f, e->x, yo, 0);
	return 1;
}

/* ================================================================ 31 SNOWBOMB
 * (Gegner_SnowBomb.cpp) a big snow ball rolling (15) towards the side the player was on; it
 * falls off edges, bounces (x 0.7) and rolls on; each wall it bounces from costs it 1 of its
 * 30 - then it bursts into 8 small snow balls (damage 15) thrown up. The player can stand on
 * it and ride. Not shootable, no contact damage. The small balls do not bounce here. */
static void SNOWBOMB_load(void) { }
static void SNOWBOMB_init(Ent *e) { (void)e; }
static int SNOWBOMB_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)touching;
	ice_platform(e, idx);
	anim_loop(e, 0, 18, 3);
	switch (e->state)
	{
	case 0:
		e->xs = (pl.x >> 8) > x ? FX(15) : FX(-15);
		e->state = 1;
		break;
	case 1:
		if (!(bu & B_WALL)) { e->ys = FX(10); e->ya = FX(5); e->state = 2; }
		break;
	default:
		if (e->ys > 0 && (bu & (B_WALL | B_PLATFORM)))
		{
			e->ys = (int16_t)(-e->ys * 7 / 10);
			if (e->ys > FX(-1))
			{
				int ny;
				e->ys = 0; e->ya = 0; e->state = 1;
				if (blk_below(x, y, y, ent_box(e), &ny) & (B_WALL | B_PLATFORM)) e->y = FX(ny);
			}
			sfx(SFX_LAND);
		}
		if (e->ys > FX(30)) e->ys = FX(30);
		break;
	}
	if ((e->xs < 0 && (bl & B_WALL)) || (e->xs > 0 && (br & B_WALL))) { e->energy--; e->xs = (int16_t)-e->xs; }
	ice_move(e, idx, bl, br, bo, bu);
	ice_carry(e, idx);
	return 1;
}
static int SNOWBOMB_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	for (int i = 0; i < 8; i++)
		enemy_shot_pic(&ice_snowsmall, x + rnd(60), y + rnd(50), FX(rnd(80) - 40) / 5, -FX(rnd(40) + 80) / 5,
		               FX(5) / 6, DMG(15), 1);
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_LAND);
	return 1;
}
static int SNOWBOMB_draw(const Ent *e, uint32_t t)
{
	(void)t;
	if (e->state) draw_pic(&P_SNOWBOMB, e->xs < 0 ? e->anim : 19 - e->anim, e->x, e->y, 0);
	return 1;
}

/* ================================================================ 28 EISSTACHEL
 * (Gegner_EisStachel.cpp) a spinning ice spike swinging to and fro (1.7 a unit up to value1);
 * touching it costs 10 a tenth and pushes the player away. Not shootable. */
static void EISSTACHEL_load(void) { }
static void EISSTACHEL_init(Ent *e) { (void)e; }
static int EISSTACHEL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)y; (void)bl; (void)br; (void)bo; (void)bu;
	if (e->state == 0) { e->state = 1; e->xs = 0; e->xa = FX(1.7); }
	anim_loop(e, 0, 18, 6);
	if (e->xs > FX(e->v1) && e->xa > 0) e->xa = (int16_t)-e->xa;
	if (e->xs < -FX(e->v1) && e->xa < 0) e->xa = (int16_t)-e->xa;
	if (touching && pl.platform != idx + 1)
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		player_damage(DMG_T(10));
		if (px + 35 < x + 52) { if (!(blk_left(px, py, px, &player_box, 0) & B_WALL)) pl.x -= SYNC(FX(21)); }
		else if (!(blk_right(px, py, px, &player_box, 0) & B_WALL)) pl.x += SYNC(FX(21));
	}
	return 0;
}
static int EISSTACHEL_explode(Ent *e) { (void)e; return 0; }
static int EISSTACHEL_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ================================================================ 150-152 FLOATING
 * (Trigger_Floating*.cpp) floating platforms: when the player lands on one it dips (his fall
 * speed / 3, / 5 for the big one) and comes back up (-3 a unit) to its height (kept in v2).
 * The big one (152) turns over 0.8 s after he landed and drops him (it cannot be stood on
 * while it turns: t1 the count, t2 the angle in 1/16 degrees, spare the turning direction).
 * Drawn unturned; while it is edge-on its top is left out. Left out: the jets' smoke. */
static void ice_float(Ent *e, int idx, int div)
{
	const int on = pl.platform == idx + 1;
	if (e->state == 0)
	{
		ice_platform(e, idx);
		if (!on && pl.platform == idx + 1)
		{
			if (div == 5) { e->t1 = 1; e->spare = (uint8_t)(pcx() < (e->x >> 8) + 50 ? 1 : 2); }
			if (e->ys == 0)
			{
				e->v2 = (int16_t)(e->y >> 8);
				e->ys = (int16_t)(pl.ys / div);
				e->ya = FX(-3);
			}
		}
		if (e->t1 > 0 && ++e->t1 > 48)
		{
			if (pl.platform == idx + 1)
			{
				e->spare = (uint8_t)(pcx() < (e->x >> 8) + 50 ? 1 : 2);
				pl.platform = 0;
			}
			e->t1 = 0;
			e->t2 = 0;
			e->state = 1;
		}
	}
	else
	{
		e->t2 += e->spare == 1 ? 67 : -67;
		if (e->t2 > 2880 || e->t2 < -2880) { e->t2 = 0; e->state = 0; }
	}
	if (e->ys < 0 && (e->y >> 8) <= e->v2) { e->ys = 0; e->ya = 0; e->y = FX(e->v2); }
	ice_move(e, idx, 0, 0, 0, 0);
	ice_carry(e, idx);
}

static void FLOATING_load(void) { }
static void FLOATING_init(Ent *e) { e->dir = 1; }
static int FLOATING_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	e->dir = 1;
	ice_float(e, idx, 3);
	return 1;
}
static int FLOATING_explode(Ent *e) { (void)e; return 1; }
static int FLOATING_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

static void FLOATING2_load(void) { }
static void FLOATING2_init(Ent *e) { e->dir = 1; }
static int FLOATING2_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	e->dir = 1;
	ice_float(e, idx, 3);
	return 1;
}
static int FLOATING2_explode(Ent *e) { (void)e; return 1; }
static int FLOATING2_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

static void FLOATING3_load(void) { }
static void FLOATING3_init(Ent *e) { (void)e; }
static int FLOATING3_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	ice_float(e, idx, 5);
	return 1;
}
static int FLOATING3_explode(Ent *e) { (void)e; return 1; }
static int FLOATING3_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const int a = e->t2 < 0 ? -e->t2 : e->t2;
	if (e->state == 0 || a < 720 || a > 2160) draw_pic(&P_FLOATING3, 0, e->x, e->y, 0);
	draw_pic(&P_FLOATING3, 1, e->x, e->y, 0);
	return 1;
}

/* ================================================================ 159 LIFT
 * (Trigger_Lift.cpp) when the player stands on it, it rises (-3, -3 a unit, at most 30) and
 * counts down on its display (10 frames, value1 / 10 tenths each); at 0 it bursts, drops him
 * and waits again where it started. */
static void LIFT_load(void) { }
static void LIFT_init(Ent *e) { (void)e; }
static int LIFT_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)x; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	if (e->size && ++e->animt > e->v1 * 6 / 10)
	{
		e->animt = 0;
		if (++e->anim >= 10)
		{
			fx_add(&P_explbig, (e->x >> 8) + 40, y + 10, 2);
			sfx(SFX_EXPLODE);
			e->state = 0; e->anim = 0; e->size = 0;
			e->y = e->yo = FX(e->v2);
			e->ys = 0; e->ya = 0;
			if (pl.platform == idx + 1) pl.platform = 0;
			return 1;
		}
	}
	ice_platform(e, idx);
	if (e->state == 0)
	{
		e->v2 = (int16_t)y;
		if (pl.platform == idx + 1)
		{
			e->state = 1; e->ys = FX(-3); e->ya = FX(-3);
			e->anim = 0; e->animt = 0; e->size = 1;
		}
	}
	else if (e->ys < FX(-30)) { e->ya = 0; e->ys = FX(-30); }
	ice_move(e, idx, 0, 0, 0, 0);
	ice_carry(e, idx);
	return 1;
}
static int LIFT_explode(Ent *e) { (void)e; return 1; }
static int LIFT_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }
