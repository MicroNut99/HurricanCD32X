/*
 * Stage 2 (the temple) and stage 3 (the elevator shaft): the lift, the temple's wasps, caterpillars,
 * rocket launchers, stamping stones and crumbling rocks, and the shaft's falling boulders, bombs and
 * nukes, stilt walkers, lava crabs and wall crawlers.
 *
 * The lift (FAHRSTUHL) keeps where it is and how fast it goes in temple_lift_* (Hurrican's
 * g_Fahrstuhl_yPos / g_Fahrstuhl_Speed): the shaft's enemies fall onto it, stand on it, or
 * burst on it.
 */

/* ------------------------------------------------------------------ shared */
static int temple_lift_on;          /* a lift has run this stage */
static int32_t temple_lift_y;       /* its top, 8.8 full-size pixels */
static int16_t temple_lift_ys;      /* its speed, 8.8 units */

/* the stage 3 lift's speed changes: the map's PARTIKELSPAWN objects with Value1 10 (Trigger_PartikelSpawner
 * case 10: counts Value2 tenths of a second down again and again; the first time it runs out while the
 * player is nearer than 1000 it sets Player.AutoScrollspeed = Value2 and is gone) */
#define TEMPLE_NTRIG 12
static int16_t temple_trig_x[TEMPLE_NTRIG], temple_trig_y[TEMPLE_NTRIG], temple_trig_v[TEMPLE_NTRIG], temple_trig_t[TEMPLE_NTRIG];
static int temple_ntrig;
static int16_t temple_autospeed;    /* Player.AutoScrollspeed, 8.8 units */

/* one frame of a picture as a picture of its own (a shot that does not animate) */
static Pic temple_frame(Pic p, int i)
{
	if (p.count > i) { p.first += i; p.count = 1; }
	return p;
}

/* frame `frame` of p at (fx, fy) (8.8 full size), its rows above full-size y `top` left out
 * (Hurrican's SetRect on the picture): the view's top is moved to the cut for the call */
static void temple_draw_cut(const Pic *p, int frame, int32_t fx_, int32_t fy, int top)
{
	if (!p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	const int ty = top >> 1, sy = (int)(fy >> 9), vy = view_y, vb = view_y + PLAY_H;
	if (ty <= vy) { draw_frame(&f, p->fw, (int)(fx_ >> 9), sy, 0, 1); return; }
	if (ty >= vb) return;
	const int by = sy + f.y0;
	if (by + f.h > vb) f.h = vb - by;          /* the real view's bottom stays the bottom */
	if (f.h <= 0) return;
	view_y = ty;
	draw_frame(&f, p->fw, (int)(fx_ >> 9), sy, 0, 1);
	view_y = vy;
}

/* ------------------------------------------------------------------ FAHRSTUHL (141) */
/*
 * The lift (Trigger_FahrStuhl.cpp): waits until the player stands on it, then goes down.
 *  Value1 != 0 (the temple's exit): down at speed 8, the player cannot jump off.
 *  Value1 == 0 (the elevator stage): its speed follows the level's speed triggers (0.3 units per
 *  unit towards it); at Value2 it explodes and the player jumps off. The player comes back to
 *  life on it.
 * Left out: the shadow, the sparks and smoke, the parallax layer; the camera lock (Hurrican fixes
 * the screen to the lift: XOffset = xPos + 200 - 320, YOffset = yPos - 390) - p1.c's camera
 * follows the player.
 */
static void FAHRSTUHL_load(void)
{
	temple_lift_on = 0;
	temple_lift_y = -FX(1);
	temple_lift_ys = 0;
	temple_ntrig = 0;
	temple_autospeed = 0;
}
static void FAHRSTUHL_init(Ent *e)
{
	e->state = 0;
	if (e->v1 != 0) return;
	const uint8_t *o;
	uint32_t size;
	temple_ntrig = 0;
	if (!pack_file("OBJECTS.BIN", &o, &size)) return;
	const int n = (int)rd32(o + 4);
	for (int i = 0; i < n && temple_ntrig < TEMPLE_NTRIG; i++)
	{
		const uint8_t *r = o + 8 + 12 * i;
		if (rd16(r) != 140 || (int16_t)rd16(r + 6) != 10 || rd16(r + 10) > MAP_SKILL) continue;
		temple_trig_x[temple_ntrig] = (int16_t)rd16(r + 2);
		temple_trig_y[temple_ntrig] = (int16_t)rd16(r + 4);
		temple_trig_v[temple_ntrig] = (int16_t)rd16(r + 8);
		temple_trig_t[temple_ntrig] = (int16_t)(6 * (int16_t)rd16(r + 8));
		temple_ntrig++;
	}
}
static void temple_lift_triggers(void)
{
	for (int k = 0; k < temple_ntrig; k++)
	{
		if (temple_trig_v[k] < 0) continue;                    /* used */
		if (--temple_trig_t[k] > 0) continue;
		temple_trig_t[k] = (int16_t)(temple_trig_v[k] > 0 ? 6 * temple_trig_v[k] : 1);
		const int dx = pcx() - (temple_trig_x[k] + 20), dy = pcy() - (temple_trig_y[k] + 20);
		if (dx > -1000 && dx < 1000 && dy > -1000 && dy < 1000 && isqrt(dx * dx + dy * dy) < 1000)
		{
			temple_autospeed = (int16_t)FX(temple_trig_v[k]);
			temple_trig_v[k] = -1;
		}
	}
}
static int FAHRSTUHL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	e->energy = 100;
	if (e->v1 == 0) temple_lift_triggers();
	ent_platform(e, idx);
	const int on = pl.platform == idx + 1;
	if (on) e->state = 1;                                       /* off it goes */
	if (e->state == 1)
	{
		int want;
		if (e->v1 != 0) { want = FX(8); if (on) pl.jump_ok = 0; }
		else want = temple_autospeed;
		if (e->ys < want) e->ys += SYNC(FX(0.3));
		if (e->ys > want) e->ys -= SYNC(FX(0.3));
	}
	/* it moves itself (through walls, never out of the level) and takes the player along */
	e->xo = e->x; e->yo = e->y;
	e->y += SYNC(e->ys);
	if (on) pl.y += e->y - e->yo;
	temple_lift_on = 1;
	temple_lift_y = e->y;
	temple_lift_ys = e->ys;
	if (e->v1 == 0)
	{
		/* the player comes back on the lift (Player.cpp: g_Fahrstuhl_yPos > -1) */
		pl.safe_x = e->x + FX(200 - 35);
		pl.safe_y = e->y + FX(10 - player_box.b - 1);
		if (y >= e->v2 && e->v2 > 0) e->energy = 0;              /* the end of the shaft */
		/* Hurrican fixes the screen to the lift (XOffset = x + 200 - 320, the lift's top about
		 * 90 pixels above the bottom of the 400 shown) */
		if (e->state == 1) { cam_lock = 1; cam_lock_x = (e->x >> 8) + 200 - 320; cam_lock_y = (e->y >> 8) - 310; }
	}
	(void)x;
	return 1;
}
static int FAHRSTUHL_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 80, y + 40, 2);
	fx_add(&P_explbig, x + 200, y + 60, 3);
	fx_add(&P_explbig, x + 320, y + 40, 2);
	sfx(SFX_BIGEXPLODE);
	if (pl.state != PS_DEAD)
	{
		/* the player jumps off */
		pl.state = PS_JUMP;
		pl.anim = 0;
		pl.jumpstart = pl.y;
		pl.ys = FX(-43);
		pl.jumpadd = 0;
		pl.jump_ok = 0;
	}
	pl.platform = 0;
	return 1;
}
static int FAHRSTUHL_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ SMALLWESPE (70) */
/*
 * The small wasp (Gegner_SmallWespe.cpp): waits until the player is nearer than 500, then flies at
 * points near him (accel 3, at most 8, 20 when nearer than 200); flying into him from the side
 * costs 8 and bounces it back. Shot down it falls and bursts on the ground (200 points).
 * Left out: the smoke.
 */
static void SMALLWESPE_load(void) { }
static void SMALLWESPE_init(Ent *e) { e->state = 0; }
static int SMALLWESPE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
	anim_loop(e, 0, 3, 3);
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (e->energy <= 0 && e->state != 2)                        /* shot down: falls */
	{
		e->state = 2;
		e->energy = 30;
		e->ys = FX(3);
		e->xa = 0;
		e->ya = FX(2);
		e->xs = e->dir ? FX(5) : FX(-5);
		fx_add(&P_expl, x + 25, y + 10, 2);
		sfx(SFX_HIT);
	}
	if (e->state == 0)
	{
		if (player_distance(e) < 500)
		{
			e->v1 = (int16_t)(px + 35 - 50 + rnd(100));
			e->v2 = (int16_t)(py + 40 - 50 + rnd(30));
			e->state = 1;
		}
	}
	else if (e->state == 1)
	{
		if (e->v1 < x && !(bl & B_WALL)) { e->xa = FX(-3); e->dir = 0; }
		if (e->v1 > x && !(br & B_WALL)) { e->xa = FX(3); e->dir = 1; }
		if (e->v2 < y) { if (!(bo & B_WALL)) e->ya = FX(-3); else e->ys = FX(20); }
		if (e->v2 > y) { if (!(bu & B_WALL)) e->ya = FX(3); else { e->ys = FX(-10); e->ya = 0; } }
		const int d = player_distance(e);
		const int m = d < 200 ? FX(20) : FX(8);
		if (e->xs > m) e->xs = (int16_t)m;
		if (e->xs < -m) e->xs = (int16_t)-m;
		if (e->ys > m) e->ys = (int16_t)m;
		if (e->ys < -m) e->ys = (int16_t)-m;
		const int dx = x - e->v1, dy = y - e->v2;
		if (d > 400 || dx * dx + dy * dy < 400)
		{
			e->v1 = (int16_t)(px - 50 + rnd(100));
			e->v2 = (int16_t)(py - 50 + rnd(100));
		}
		if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM))
		{
			e->v1 = (int16_t)(px + 35 - 50 + rnd(100));
			e->v2 = (int16_t)(py + 40 - 50 + rnd(30));
		}
	}
	else
	{
		if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM)) e->energy = 0;
		if (e->ys > FX(35)) e->ya = 0;
	}
	if (e->state != 2 && touching)
	{
		if (x + 25 > px + 35 && e->dir == 0 && e->xs < FX(-4)) { e->xs = FX(16); player_damage(DMG(8)); }
		else if (x + 25 < px + 35 && e->dir == 1 && e->xs > FX(4)) { e->xs = FX(-16); player_damage(DMG(8)); }
	}
	return 0;
}
static int SMALLWESPE_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int SMALLWESPE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ ROBORAUPE (22) */
/*
 * The robot caterpillar (Gegner_RoboRaupe.cpp): crawls 18 pixels each round of its 10 frames,
 * turns at walls, falls off edges; each step, when the player is nearer than 200, it may fire three
 * shots in arcs (10 each). Touching it costs 5 per unit. 250 points.
 * Left out: the blue smoke of the shots and the explosion's particles.
 */
static Pic temple_p_arcshot;
static void ROBORAUPE_load(void) { temple_p_arcshot = pic("arcshot.png"); }
static void ROBORAUPE_init(Ent *e) { e->state = 0; }
static int ROBORAUPE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bo; (void)touching; (void)x;
	if (e->state == 0) { e->state = 1; e->dir = 0; }            /* Hurrican: starts going left */
	if (++e->animt >= 6)
	{
		e->animt = 0;
		if (++e->anim > 9) e->anim = 0;
		if (e->anim == 5)
		{
			e->x += e->dir ? FX(18) : FX(-18);
			const int nx = e->x >> 8;
			if (player_distance(e) < 200 && rnd(2) == 0)
			{
				const fx g = SYNC(FX(4));
				const int a = e->dir ? 31 : 50;
				enemy_shot_pic(&temple_p_arcshot, nx + a, y + 10, 0, FX(-30), g, DMG(10), 0);
				enemy_shot_pic(&temple_p_arcshot, nx + a - 16 - (e->dir ? 0 : 2), y + 10, FX(-12), FX(-30), g, DMG(10), 1);
				enemy_shot_pic(&temple_p_arcshot, nx + a + 20, y + 10, FX(12), FX(-30), g, DMG(10), 1);
			}
		}
	}
	if (!(bu & (B_WALL | B_PLATFORM)))
	{
		e->ya = FX(3);
		if (e->ys > FX(20)) e->ys = FX(20);
	}
	else { e->ys = 0; e->ya = 0; }
	if (bl & (B_WALL | B_EWALL)) e->dir = 1;
	if (br & (B_WALL | B_EWALL)) e->dir = 0;
	return 0;
}
static int ROBORAUPE_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e) - 15, ecy(e), 2);
	fx_add(&P_expl, ecx(e) + 15, ecy(e), 3);
	sfx(SFX_EXPLODE);
	return 1;
}
static int ROBORAUPE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ RAKETENWERFER (24) */
/*
 * The rocket launcher in the ceiling (Gegner_Raketenwerfer.cpp): while the player is below it and
 * nearer than 600 it runs its 10 frames (1.3 units each, a short pause after) and fires a rocket at
 * frame 6 (40 damage). The rocket flies down (or up) at 20 until it is at the player's height, then
 * turns towards him (10, accelerating by 5). 400 points.
 * The launcher steers its last rocket (the engine's shots do not steer); left out: the flame trail.
 */
static Pic temple_p_rocket[2];
static void RAKETENWERFER_load(void)
{
	const Pic p = pic("rocketwerfer.png");
	temple_p_rocket[0] = temple_frame(p, 0);
	temple_p_rocket[1] = temple_frame(p, 1);
}
static void RAKETENWERFER_init(Ent *e) { e->t1 = 0; e->t2 = 0; }
static int RAKETENWERFER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	e->dir = 0;
	if (player_distance(e) < 600 && pl.y > e->y) e->animt++;
	if (e->animt >= 8)                                         /* AnimSpeed 1.3: 7.8 steps */
	{
		e->animt = 0;
		e->anim++;
		if (e->anim == 6)
		{
			const int right = (pl.x >> 8) > x + 2;
			const fx ys = (pl.y >> 8) + 60 < y + 5 ? FX(-20) : FX(20);
			Shot *s = enemy_shot_pic(&temple_p_rocket[right], x + 2, y + 5, 0, ys, 0, DMG(40), 0);
			e->t2 = (int16_t)(s ? (s - shots) + 1 : 0);
			e->t1 = 0;
		}
		if (e->anim > 9) { e->anim = 0; e->animt = -12; }
	}
	/* its rocket */
	if (e->t2)
	{
		Shot *s = &shots[e->t2 - 1];
		if (!s->on || (s->pic != &temple_p_rocket[0] && s->pic != &temple_p_rocket[1])) e->t2 = 0;
		else
		{
			const int right = s->pic == &temple_p_rocket[1];
			if (s->xs == 0)
			{
				const int sy = s->y >> 8, py = pl.y >> 8;
				if (sy > py + player_box.t && sy < py + player_box.b)
				{
					e->t1 = s->ys < 0 ? 1 : -1;               /* the vertical speed brakes to 0 */
					s->xs = right ? FX(10) : FX(-10);
				}
			}
			else
			{
				s->xs += right ? SYNC(FX(5)) : -SYNC(FX(5));
				if (e->t1 > 0) { s->ys += SYNC(FX(5)); if (s->ys >= 0) { s->ys = 0; e->t1 = 0; } }
				else if (e->t1 < 0) { s->ys -= SYNC(FX(5)); if (s->ys <= 0) { s->ys = 0; e->t1 = 0; } }
			}
		}
	}
	return 0;
}
static int RAKETENWERFER_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int RAKETENWERFER_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ STAMPFSTEIN (72) */
/*
 * The stamping stone on its chain (Trigger_Stampfstein.cpp): falls (20, accel 15, at most 40) when
 * the player is under it (within 50 sideways); falling onto him kills him (500); on the ground it
 * waits 20 units and is pulled up again (4), the player can ride it (and is crushed against the
 * ceiling). Its spikes cost 20 per unit; it cannot be walked through or jumped through from below.
 * Shots hit it but cannot hurt it. Drawn as Hurrican: only the part below its rest height shows
 * (the chain comes out as it falls).
 * Left out: dust and splinters (one smoke effect), killing enemies under it, falling when the
 * screen shakes.
 */
static void STAMPFSTEIN_load(void) { }
static void STAMPFSTEIN_init(Ent *e) { e->state = 0; e->t2 = (int16_t)(e->y >> 8); }
static int STAMPFSTEIN_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)bl; (void)br;
	const int px = pl.x >> 8, py = pl.y >> 8;
	const int on = pl.platform == idx + 1;
	e->energy = 5000;
	/* the player cannot walk through it, nor jump through it from below */
	if (touching && !on)
	{
		if (px + 35 < x + 40) { if (!(blk_left(px, py, px, &player_box, 0) & B_WALL)) pl.x -= SYNC(FX(21)); }
		else if (!(blk_right(px, py, px, &player_box, 0) & B_WALL)) pl.x += SYNC(FX(21));
		if (pl.ys < 0) pl.ys = -pl.ys / 4;
	}
	switch (e->state)
	{
	case 0:                                                    /* waiting up there */
		e->t2 = (int16_t)y;
		if (abs(px + 35 - (x + 40)) < 50 && abs(py + 45 - (y + 122)) < 600 && py > y)
		{
			e->state = 1;
			e->ys = FX(20);
			e->ya = FX(15);
			sfx(SFX_LAND);
		}
		break;
	case 1:                                                    /* falling */
		if (e->ys > FX(40)) e->ys = FX(40);
		if (touching && !on) player_damage(DMG(500));
		if (bu & (B_WALL | B_PLATFORM))
		{
			fx_add(&P_smoke, x + 40, y + 230, 3);
			sfx(SFX_EXPLODE);
			e->ys = 0;
			e->ya = 0;
			e->t1 = 120;
			e->state = 2;
		}
		break;
	case 2:                                                    /* down: waits */
		ent_platform(e, idx);
		if (--e->t1 < 0) { e->ys = FX(-4); e->state = 3; }
		break;
	default:                                                   /* pulled up */
		ent_platform(e, idx);
		if (bo & B_WALL) { e->state = 0; e->ys = 0; }
		if (pl.platform == idx + 1 && (blk_top(px, py, py, &player_box, 0) & B_WALL))
		{
			player_damage(DMG(500));
			pl.platform = 0;
			pl.y += FX(10);
			pl.ys = FX(10);
		}
		break;
	}
	/* the spikes underneath */
	if (touching && py > y) player_damage(DMG_T(20));
	return 0;
}
static int STAMPFSTEIN_explode(Ent *e) { e->energy = 5000; return 1; }
static int STAMPFSTEIN_draw(const Ent *e, uint32_t t)
{
	(void)t;
	temple_draw_cut(&P_STAMPFSTEIN, 0, e->x, e->y, e->t2 + 165);
	return 1;
}

/* ------------------------------------------------------------------ BROCKELROCK (21) */
/*
 * The crumbling rock (Gegner_BrockelRock.cpp): a platform; once the player stands on it, it
 * crumbles (5 units: its second picture), then falls (accel 4, at most 50) with him on it and
 * bursts on the ground.
 * Left out: the cross-fade (the picture changes halfway), the splinters (one smoke effect).
 */
static void BROCKELROCK_load(void) { }
static void BROCKELROCK_init(Ent *e) { e->state = 0; e->t1 = 255; }
static int BROCKELROCK_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)bl; (void)br; (void)bo; (void)touching;
	ent_platform(e, idx);
	if (e->state == 0)
	{
		if (pl.platform == idx + 1) e->state = 1;
	}
	else if (e->state == 1)
	{
		e->t1 = (int16_t)(e->t1 - 50 / 6);
		if (e->t1 < 0)
		{
			e->t1 = 0;
			e->state = 2;
			e->ya = FX(4);
			fx_add(&P_smoke, x + 40, y + 20, 4);
			sfx(SFX_LAND);
		}
	}
	else
	{
		if (e->ys > FX(50)) e->ya = 0;
		if (bu & B_WALL) e->energy = 0;
	}
	e->anim = e->t1 < 128;
	return 0;
}
static int BROCKELROCK_explode(Ent *e)
{
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_EXPLODE);
	return 1;
}
static int BROCKELROCK_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ BOULDER (33) */
/*
 * The blue boulder (Gegner_BlueBoulder.cpp): in the shaft it comes from above the screen (480 up
 * when it appears) falling at Value1 and bursts on the lift or any wall; hitting the player costs 20
 * and bursts it. Without a lift it is thrown (Value1, Value2, falling 2). Cannot be shot.
 * Left out: the small stones and the smoke (one smoke effect).
 */
static void BOULDER_load(void) { }
static void BOULDER_init(Ent *e) { e->state = 0; e->ys = (int16_t)FX(e->v1); }
static int BOULDER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y;
	anim_loop(e, 0, 13, e->state == 2 ? 2 : 6);
	if (e->state == 0)
	{
		e->state = 1;
		if (temple_lift_on) e->y -= FX(480);
		else { e->xs = (int16_t)FX(e->v1); e->ys = (int16_t)FX(e->v2); e->ya = FX(2); e->state = 2; }
		return 1;
	}
	if (touching) { player_damage(DMG(20)); e->energy = 0; }
	if (temple_lift_on && e->y >= temple_lift_y - FX(50)) e->energy = 0;
	if ((bl | br | bo | bu) & B_WALL) e->energy = 0;
	return 0;
}
static int BOULDER_explode(Ent *e)
{
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_EXPLODE);
	return 1;
}
static int BOULDER_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ STELZSACK (34) */
/*
 * The stilt walker (Gegner_StelzSack.cpp): drops from above the screen onto the lift (at most 80
 * faster than it), lands springing, then stands on it firing a laser sideways every 12 units (8;
 * the laser falls 15 faster than the lift) and turns round when the player passes it. Touching it
 * costs 5 per unit. 500 points. P1h: hit by shots down to the knees, energy 40 (see the step).
 * Left out: the muzzle flare, the head and stilts flying off when it dies.
 */
static Pic temple_p_stelzlaser[2];
static void STELZSACK_load(void)
{
	const Pic p = pic("stelzlaser.png");
	temple_p_stelzlaser[0] = temple_frame(p, 0);
	temple_p_stelzlaser[1] = temple_frame(p, 1);
}
static void STELZSACK_init(Ent *e) { e->state = 0; e->ya = FX(10); }
static int STELZSACK_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching; (void)y;
	const int c = pcx();
	if (e->state >= 2) { e->y = temple_lift_y - FX(147); e->ys = 0; e->ya = 0; }
	/* P1h: the list's box reaches down the stilts (20,5 - 80,130) so the spread shot hits it
	 * straight (this port has no upward weapons; Hurrican's box is the body only, 20,5 - 80,60,
	 * and Energy 80 - here 40); touching costs 5 per unit only at the body, as in Hurrican */
	{
		static const Box body = { 20, 5, 80, 60 };
		if (pl.state != PS_DEAD && boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box, x, e->y >> 8, &body))
			player_damage(DMG_T(5));
	}
	switch (e->state)
	{
	case 0:                                                    /* appears: drops from above the screen */
		e->state = 1;
		e->y = temple_lift_y - FX(147 + 480 + 160);
		if (e->y < 0) e->y = 0;
		e->ys = temple_lift_ys;
		e->ya = FX(10);
		e->anim = 0;
		e->dir = x + 50 < c;
		break;
	case 1:                                                    /* falling */
		if (e->ys > temple_lift_ys + FX(80)) { e->ys = (int16_t)(temple_lift_ys + FX(80)); e->ya = 0; }
		if (e->y + FX(147) > temple_lift_y)
		{
			e->y = temple_lift_y - FX(147);
			e->ys = 0; e->ya = 0;
			e->state = 2;
			e->anim = 2; e->animt = 0;
		}
		break;
	case 2:                                                    /* landing */
		if (++e->animt > 4) { e->animt = 0; if (++e->anim >= 9) { e->anim = 9; e->state = 3; e->t1 = 0; } }
		break;
	case 3:                                                    /* standing, firing */
		if (++e->t1 >= 72)
		{
			e->t1 = 0;
			const fx ys = FX(15) + temple_lift_ys;
			if (e->dir) enemy_shot_pic(&temple_p_stelzlaser[0], x + 50, (e->y >> 8) + 25, FX(30), ys, 0, DMG(8), 0);
			else enemy_shot_pic(&temple_p_stelzlaser[1], x - 30, (e->y >> 8) + 25, FX(-30), ys, 0, DMG(8), 0);
		}
		if ((e->dir == 0 && x + 50 < c) || (e->dir == 1 && x + 50 > c)) { e->state = 4; e->animt = 0; }
		break;
	case 4:                                                    /* turning */
		if (++e->animt > 4) { e->animt = 0; if (++e->anim >= 15) { e->anim = 13; e->state = 5; e->dir ^= 1; } }
		break;
	default:                                                   /* turning back to the middle */
		if (++e->animt > 4) { e->animt = 0; if (--e->anim <= 9) { e->anim = 9; e->state = 3; } }
		break;
	}
	return 0;
}
static int STELZSACK_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int STELZSACK_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ FALLBOMBE (35) */
/*
 * The falling bomb (Gegner_FallBombe.cpp): comes from above the screen (10, accel 3, at most 50
 * faster than the lift) and bursts on the lift; hitting the player costs 25. Cannot be shot.
 * Left out: the smoke trail, the sparks.
 */
static void FALLBOMBE_load(void) { }
static void FALLBOMBE_init(Ent *e) { e->state = 0; e->ys = FX(10); e->ya = FX(3); }
static int FALLBOMBE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu;
	anim_loop(e, 0, 5, 6);
	if (e->state == 0)
	{
		e->state = 1;
		e->y -= FX(480 + 59);
		if (e->y < 0) e->y = 0;
	}
	else if (e->ys > temple_lift_ys + FX(50)) e->ys = (int16_t)(temple_lift_ys + FX(50));
	if (touching) { player_damage(DMG(25)); e->energy = 0; }
	if (temple_lift_on && e->y >= temple_lift_y - FX(59)) e->energy = 0;
	return 0;
}
static int FALLBOMBE_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), (e->y >> 8) + 45, 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int FALLBOMBE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ POKENUKE (36) */
/*
 * The big nuke (Gegner_PokeNuke.cpp): comes from above the screen at 27 and bursts on the lift;
 * hitting the player costs 100, the blast another 50 within 160. Cannot be shot.
 * Left out: the smoke trail, the screen shake, most of the blast's particles.
 */
static void POKENUKE_load(void) { }
static void POKENUKE_init(Ent *e) { e->state = 0; e->ys = FX(27); }
static int POKENUKE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu;
	e->dir = 0;
	if (e->state == 0) { e->state = 1; e->y -= FX(480 + 217); return 1; }
	if (temple_lift_on && e->y > temple_lift_y - FX(217)) e->energy = 0;
	if (touching) { player_damage(DMG(100)); e->energy = 0; }
	return 0;
}
static int POKENUKE_explode(Ent *e)
{
	if (pl.state != PS_DEAD && player_distance(e) < 160) player_damage(DMG(50));
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 40, y + 60, 2);
	fx_add(&P_explbig, x + 40, y + 170, 3);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int POKENUKE_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ LAVAKRABBE (37) */
/*
 * The lava crab (Gegner_LavaKrabbe.cpp): jumps out of the lava below (from 190 above the lift, at
 * 15 upwards against it, falling 1), lands on the lift and walks after the player (5), turning
 * round when he passes it; touching it costs 8 per unit. It can be shot once it is near (after 21
 * units of its jump) or has landed. P1h: hit by standing shots too, energy 40 (see the step).
 * Left out: it is not drawn small and turning while far away (it shows when it is near), its
 * colour; the head and legs flying off when it dies.
 */
static void LAVAKRABBE_load(void) { }
static void LAVAKRABBE_init(Ent *e) { e->state = 0; }
static int LAVAKRABBE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)y; (void)bl; (void)br; (void)bo; (void)bu;
	const int c = pcx();
	if (e->state >= 2) { e->y = temple_lift_y - FX(34); e->ys = 0; e->ya = 0; }
	switch (e->state)
	{
	case 0:                                                    /* appears: jumps */
		e->state = 1;
		e->y = temple_lift_on ? temple_lift_y - FX(190) : FX(cam_y * 2 + 200);
		e->ya = FX(1);
		e->ys = (int16_t)(temple_lift_ys - FX(15.05));
		e->t2 = 0;
		return 1;
	case 1:                                                    /* in the air */
		if (e->ys > temple_lift_ys + FX(60)) { e->ys = (int16_t)(temple_lift_ys + FX(60)); e->ya = 0; }
		if (e->t2 < 1000) e->t2++;
		if (e->t2 <= 130) e->energy = 40;                      /* far away: cannot be hurt */
		if (temple_lift_on && e->y >= temple_lift_y - FX(34) && e->ys > 0)
		{
			e->state = 3;
			e->y = temple_lift_y - FX(34);
			e->ys = 0; e->ya = 0;
			e->anim = 1; e->animt = 0;
			e->dir = x + 35 > c;
		}
		break;
	case 2:                                                    /* turning */
		if (++e->animt >= 6) { e->animt = 0; if (--e->anim <= 0) { e->anim = 0; e->state = 3; e->dir ^= 1; } }
		break;
	case 3:                                                    /* turning back */
		if (++e->animt >= 6) { e->animt = 0; if (++e->anim >= 4) { e->anim = 5; e->state = 4; } }
		break;
	default:                                                   /* walking */
		e->x += e->dir ? -SYNC(FX(5)) : SYNC(FX(5));
		if (++e->animt >= 6) { e->animt = 0; if (++e->anim >= 14) e->anim = 5; }
		if ((e->dir == 0 && x + 35 > c) || (e->dir == 1 && x + 35 < c)) { e->state = 2; e->animt = 0; e->anim = 4; }
		break;
	}
	/* P1h: the list's box reaches 24 above the crab (0,-24 - 70,34) so a standing player's
	 * spread shot hits it (Hurrican's box is 0,0 - 70,34: only ducking shots reached it; energy
	 * 90 - here 40); touching costs 8 per unit only at its body */
	if (e->state >= 2 && pl.state != PS_DEAD)
	{
		static const Box body = { 0, 0, 70, 34 };
		if (boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box, x, e->y >> 8, &body))
			player_damage(DMG_T(8));
	}
	(void)touching;
	return 0;
}
static int LAVAKRABBE_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int LAVAKRABBE_draw(const Ent *e, uint32_t t)
{
	(void)t;
	return e->state <= 1 && e->t2 < 100;                       /* far below: not drawn */
}

/* ------------------------------------------------------------------ KRABBLERLINKS (16), KRABBLERRECHTS (17) */
/*
 * The wall crawlers (Gegner_KrabblerLinks/Rechts.cpp): crawl up and down their wall at Value2
 * (turning at walls) and fire a laser across every Value1 tenths of a second (8 damage, 30). 300
 * points. Left out: the muzzle flame.
 */
static Pic temple_p_klaser;
static void temple_krabbler_init(Ent *e)
{
	e->state = 0;
	e->ys = (int16_t)FX(e->v2);
	e->t1 = (int16_t)(e->v1 * 6);
}
static int temple_krabbler_step(Ent *e, int x, int y, int bo, int bu, int right)
{
	e->dir = 0;
	if (e->state == 0) anim_loop(e, 0, 8, 3);
	else if (++e->animt >= 5) { e->animt = 0; if (++e->anim >= 14) { e->anim = 0; e->state = 0; } }
	if (bo & (B_WALL | B_EWALL)) e->ys = (int16_t)FX(e->v2);
	if (bu & (B_WALL | B_EWALL)) e->ys = (int16_t)FX(-e->v2);
	if (e->t1 > -30000) e->t1--;
	if (e->state == 0 && e->t1 <= 0)
	{
		e->t1 = (int16_t)(e->v1 > 0 ? e->v1 * 6 : 6);
		e->state = 1;
		e->anim = 10; e->animt = 0;
		if (right) enemy_shot_pic(&temple_p_klaser, x + 24, y + 23, FX(30), 0, 0, DMG(8), 0);
		else enemy_shot_pic(&temple_p_klaser, x - 24, y + 23, FX(-30), 0, 0, DMG(8), 0);
	}
	return 0;
}
static int temple_krabbler_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static void KRABBLERLINKS_load(void) { temple_p_klaser = pic("krabblerlaser2.png"); }
static void KRABBLERLINKS_init(Ent *e) { temple_krabbler_init(e); }
static int KRABBLERLINKS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)touching;
	return temple_krabbler_step(e, x, y, bo, bu, 1);
}
static int KRABBLERLINKS_explode(Ent *e) { return temple_krabbler_explode(e); }
static int KRABBLERLINKS_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }
static void KRABBLERRECHTS_load(void) { }
static void KRABBLERRECHTS_init(Ent *e) { temple_krabbler_init(e); }
static int KRABBLERRECHTS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)touching;
	return temple_krabbler_step(e, x, y, bo, bu, 0);
}
static int KRABBLERRECHTS_explode(Ent *e) { return temple_krabbler_explode(e); }
static int KRABBLERRECHTS_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }
