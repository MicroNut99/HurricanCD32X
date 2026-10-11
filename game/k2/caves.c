/*
 * The caves group (stages 4, 5, 6, 7, 8, 9): bats, lava, crabs, slime, fish and fire traps.
 * Each kind follows its Hurrican DoKI (src/enemies/Gegner_*.cpp, src/triggers/Trigger_*.cpp);
 * what is left out is said at the kind. Common leaving-outs: particles, glows (LavaFlare),
 * scaled and rotated drawing (one exception: the ceiling crab is drawn upside down).
 */

static Pic caves_flame;                  /* flame.png: the flames of the fire walker, the fire trap, the lava man */

/* ---------------------------------------------------------------- helpers */

/* Hurrican's PlayerAbstandVert: the player's box centre to the enemy's half height */
static int caves_vert(const Ent *e)
{
	return abs((pl.y >> 8) + 45 - ((e->y >> 8) + ent_box(e)->b / 2));
}

/* the ground under a walker: a slope (then it stands on it: y set) or BlockUnten's flags */
static int caves_floor(Ent *e, int x, int y)
{
	const Box *b = ent_box(e);
	if (e->ys >= 0)
	{
		int ny = y;
		const int s = blk_slopes(x, &ny, b, e->ys);
		if (s & (B_SLOPE_L | B_SLOPE_R)) { e->y = FX(ny); return s | B_WALL; }
	}
	return blk_below(x, y, y, b, 0);
}

/* landed: stand on the ground (not a few pixels in it) */
static void caves_land(Ent *e, int x, int y)
{
	int ny;
	if (blk_below(x, y, y, ent_box(e), &ny) & (B_WALL | B_PLATFORM)) e->y = FX(ny - 1);
}

/* Wegschieben: the player is pushed out sideways (and hurt by dmg a step) */
static void caves_push(const Ent *e, int touching, fx dmg)
{
	if (!touching || pl.state == PS_DEAD) return;
	if (dmg) player_damage(dmg);
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (pcx() < ecx(e)) { if (!(blk_left(px, py, px, &player_box, 0) & B_WALL)) pl.x -= SYNC(FX(21)); }
	else if (!(blk_right(px, py, px, &player_box, 0) & B_WALL)) pl.x += SYNC(FX(21));
}

/* a flame (flame.png as a shot) centred at (cx, cy), alive for life steps. Hurrican draws its
 * flames growing around (xPos + 28, yPos + 34). Enemy shots end in walls here (Hurrican's
 * flames pass them), so the centre must be outside the wall it comes from. */
static void caves_flame_shot(int cx, int cy, fx xs, fx ys, int dmg, int life)
{
	Shot *s = enemy_shot_pic(&caves_flame, cx - caves_flame.fw, cy - caves_flame.fh, xs, ys, 0, dmg, 1);
	if (s) s->life = (int16_t)life;
}

/* the frame turned by 180 degrees (both ways mirrored): draw_frame with the rows from the bottom */
static void caves_draw_turned(const Pic *p, int frame, int32_t fx_, int32_t fy_, int mirror)
{
	if (!p->count) return;
	Frame f;
	frame_of(p, frame, &f);
	if (!f.data) return;
	const int x = (int)(fx_ >> 9), y = (int)(fy_ >> 9);
	const int bx = mirror ? x + p->fw - (f.x0 + f.w) : x + f.x0;
	const int by = y + p->fh - (f.y0 + f.h);
	const int vx0 = view_x, vx1 = view_x + VIEW_W, vy0 = view_y, vy1 = view_y + VIEW_H;
	if (bx >= vx1 || by >= vy1 || bx + f.w <= vx0 || by + f.h <= vy0) return;
	const int r0 = by < vy0 ? vy0 - by : 0;
	const int r1 = by + f.h > vy1 ? vy1 - by : f.h;
	for (int r = r0; r < r1; r++)
	{
		const uint8_t *q = f.data + rd32(f.data + 4 * (f.h - 1 - r));
		int runs = rd16(q);
		q += 2;
		int col = 0;
		const int wy = by + r;
		while (runs--)
		{
			const int skip = rd16(q), len = rd16(q + 2);
			const uint8_t *s = q + 4;
			q = s + len + (len & 1);
			col += skip;
			int a, e;
			if (!mirror) { a = bx + col; e = a + len; }
			else { e = bx + f.w - col; a = e - len; }
			col += len;
			if (e <= vx0 || a >= vx1) continue;
			const uint8_t *src = s;
			if (mirror)
			{
				for (int i = 0; i < len; i++) rowbuf[i] = s[len - 1 - i];
				src = rowbuf;
			}
			if (a < vx0) { src += vx0 - a; a = vx0; }
			if (e > vx1) e = vx1;
			fb_put(lin_of(a, wy), src, e - a, 0);
			px_sprites += (uint32_t)(e - a);
		}
	}
	rect_add(bx, by, bx + f.w, by + f.h);
	if (over_list_n < MAX_RECTS)
		over_list[over_list_n++] = (Rect){ (int16_t)bx, (int16_t)by, (int16_t)(bx + f.w), (int16_t)(by + f.h) };
}

/* ---------------------------------------------------------------- 53 the bat (Gegner_Fledermaus.cpp)
 * Hangs from the ceiling (frames 15-20) until the player is below it within 240 pixels or it is
 * shot; then flies (frames 0-13) to points near the player (xAcc / yAcc 3, at most 10), bounces
 * off walls, picks a new point at walls, when there or when the player is over 300 away; back
 * at the ceiling it hangs again. Value2 = 1: flies at once. Touching costs 4 a tenth second.
 * state 0 hanging, 1 new point, 2 flying; v1 / v2 the point; t1 steps before it may hang again
 * (Hurrican's DeckenCount, which is one for all bats there). Left out: the sparks. */
static void FLEDERMAUS_load(void) { caves_flame = pic("flame.png"); }
static void FLEDERMAUS_init(Ent *e)
{
	e->state = e->v2 == 1 ? 1 : 0;
	e->anim = 15;
}
static int FLEDERMAUS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->state == 0) anim_loop(e, 15, 20, 5);
	else anim_loop(e, 0, 13, 3);

	if ((bu & (B_WALL | B_PLATFORM)) && e->ys > 0) e->ys = -e->ys;
	if ((bo & B_WALL) && e->ys < 0) e->ys = -e->ys;
	if ((br & B_WALL) && e->xs > 0) { e->xs = -e->xs; e->dir = 0; }
	if ((bl & B_WALL) && e->xs < 0) { e->xs = -e->xs; e->dir = 1; }

	if (e->state == 0)
	{
		if ((abs(pcx() - ecx(e)) < 240 && (pl.y >> 8) > y) || e->hit) { e->state = 1; e->t1 = 60; }
	}
	if (e->state == 1)
	{
		e->v1 = (int16_t)((pl.x >> 8) + rnd(80) - 30);
		e->v2 = (int16_t)((pl.y >> 8) - 50 + rnd(100));
		e->state = 2;
	}
	else if (e->state == 2)
	{
		if (e->v1 < x && !(bl & B_WALL)) { e->xa = FX(-3); e->dir = 0; }
		if (e->v1 > x && !(br & B_WALL)) { e->xa = FX(3); e->dir = 1; }
		if (e->v2 < y) { if (!(bo & B_WALL)) e->ya = FX(-3); else e->ys = FX(20); }
		if (e->v2 > y) { if (!(bu & B_WALL)) e->ya = FX(3); else { e->ys = FX(-10); e->ya = 0; } }
		if (e->xs > FX(10)) e->xs = FX(10);
		if (e->xs < FX(-10)) e->xs = FX(-10);
		if (e->ys > FX(10)) e->ys = FX(10);
		if (e->ys < FX(-10)) e->ys = FX(-10);
		const int dx = x - e->v1, dy = y - e->v2;
		if (player_distance(e) > 300 || dx * dx + dy * dy < 20 * 20) e->state = 1;
		if (((bl | br | bo | bu) & B_WALL) || (bu & B_PLATFORM)) { e->state = 1; e->anim = 0; }
		if (e->t1 > 0) e->t1--;
		if (e->t1 <= 0 && (bo & B_WALL))
		{
			e->t1 = 60;
			e->state = 0;
			e->anim = 15;
			e->xs = e->ys = e->xa = e->ya = 0;
		}
	}
	return 0;
}
static int FLEDERMAUS_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int FLEDERMAUS_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 38 the lava ball spawner (Gegner_LavaBallSpawner.cpp)
 * Invisible in the lava; every Value2 (+ up to 20) tenths of a second while the player is
 * within 2000 it throws up a lava ball (speed Value1). Its box reaches 400 pixels up so it
 * wakes when the balls' height is on the screen (Hurrican's runs all the time). t1 the delay. */
static void LAVABALLSPAWNER_load(void) { }
static void LAVABALLSPAWNER_init(Ent *e) { e->t1 = (int16_t)(e->v2 * 6); }
static int LAVABALLSPAWNER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (player_distance(e) < 2000) e->t1--;
	if (e->t1 <= 0)
	{
		e->t1 = (int16_t)((e->v2 + rnd(20)) * 6);
		Ent *b = ent_new(E_LAVABALL, x, y);
		if (b)
		{
			b->active = 1;
			b->ys = (int16_t)FX(-e->v1);
			b->ya = FX(4);
			b->v1 = (int16_t)y;            /* yStart: it sinks back in there */
		}
	}
	return 1;
}
static int LAVABALLSPAWNER_explode(Ent *e) { return 1; }
static int LAVABALLSPAWNER_draw(const Ent *e, uint32_t t) { return 1; }

/* ---------------------------------------------------------------- 39 the lava ball (Gegner_LavaBall.cpp)
 * Flies up out of the lava and falls back (yAcc 4, at most 50); gone when back under its
 * start; touching the player costs 50 and it bursts. Not shootable. Left out: the lava flare,
 * smoke, splashes. */
static void LAVABALL_load(void) { }
static void LAVABALL_init(Ent *e) { }
static int LAVABALL_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 18, 3);
	if (e->ys > FX(50)) e->ys = FX(50);
	if (e->ys > 0 && y > e->v1) { e->type = E_NONE; return 1; }
	if (touching)
	{
		player_damage(DMG(50));
		fx_add(&P_explbig, ecx(e), ecy(e), 2);
		sfx(SFX_EXPLODE);
		e->type = E_NONE;
		return 1;
	}
	return 0;
}
static int LAVABALL_explode(Ent *e) { return 1; }
static int LAVABALL_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 71 the ceiling crab (Gegner_DeckenKrabbe.cpp)
 * Hangs upside down from the ceiling; shot, or the player under it (within 100 sideways),
 * it drops, turning over (the faster the nearer the floor), lands, turns to the player
 * (frames 4..0, 0..4) and crawls at him (speed 7, frames 5-13); turns again when past him.
 * Touching costs 8 a tenth second. e->dir = the way it crawls; state 0 hanging, 1 falling,
 * 2 turning away, 3 turning to, 4 crawling; t1 its turn (1/6 degrees: 1080 = upside down),
 * t2 the turning speed. Drawn upside down until half turned. Left out: tilting on slopes,
 * the legs and head flying off. */
static void DECKENKRABBE_load(void) { }
static void DECKENKRABBE_init(Ent *e) { e->t1 = 180 * 6; }
static int DECKENKRABBE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->state != 0)
	{
		const int f = caves_floor(e, x, y);
		if (!(f & (B_WALL | B_PLATFORM))) { if (e->ys == 0) e->ys = FX(5); e->ya = FX(3); }
		else { if (e->ys > 0 && !(f & (B_SLOPE_L | B_SLOPE_R))) caves_land(e, x, y); e->ys = 0; e->ya = 0; }
		bu = f;
	}
	switch (e->state)
	{
	case 0:                                          /* hanging */
		if (e->hit || (abs(pcx() - ecx(e)) < 100 && caves_vert(e) < 800 && (pl.y >> 8) > y))
		{
			int a = 0, b = blk(x / 20, y / 20);
			while (a < 20 && !(b & B_WALL)) { b = blk(x / 20, y / 20 + a); a++; }
			e->t2 = (int16_t)((500 - 18 * a) / 10);
			e->ys = FX(5);
			e->ya = FX(5);
			e->state = 1;
		}
		break;
	case 1:                                          /* falling */
		if (e->ys > FX(30)) { e->ys = FX(30); e->ya = 0; }
		if (e->t1 >= 180 * 6) { e->t1 += e->t2; if (e->t1 >= 360 * 6) e->t1 = 0; }
		else e->t1 = 0;
		if (bu & (B_WALL | B_PLATFORM))
		{
			e->state = 3;
			e->ys = 0; e->ya = 0;
			e->anim = 1; e->animt = 0;
			if (x + 35 < (pl.x >> 8) + 35) e->dir = 1;
			if (x + 35 > (pl.x >> 8) + 35) e->dir = 0;
		}
		break;
	case 2:                                          /* turning away: 4..0 */
		e->t1 = 0;
		e->xs = 0;
		if (++e->animt > 4)
		{
			e->animt = 0;
			if (--e->anim <= 0) { e->anim = 0; e->state = 3; e->dir ^= 1; }
		}
		break;
	case 3:                                          /* turning to: 0..4 */
		e->t1 = 0;
		e->xs = 0;
		if (++e->animt > 4)
		{
			e->animt = 0;
			if (++e->anim >= 4) { e->anim = 5; e->state = 4; }
		}
		break;
	default:                                         /* crawling */
		e->xs = e->dir ? FX(7) : FX(-7);
		if (++e->animt > 4)
		{
			e->animt = 0;
			if (++e->anim >= 14) e->anim = 5;
		}
		if ((e->dir && x > (pl.x >> 8)) || (!e->dir && x < (pl.x >> 8)))
		{
			e->state = 2; e->animt = 0; e->anim = 4; e->xs = 0;
		}
		break;
	}
	return 0;
}
static int DECKENKRABBE_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int DECKENKRABBE_draw(const Ent *e, uint32_t t)
{
	/* the picture looks right; Hurrican mirrors it when crawling left */
	if (e->t1 >= 180 * 6 && e->t1 < 270 * 6) caves_draw_turned(&P_DECKENKRABBE, e->anim, e->x, e->y, e->dir);
	else draw_pic(&P_DECKENKRABBE, e->anim, e->x, e->y, !e->dir);
	return 1;
}

/* ---------------------------------------------------------------- 73 the lava man (Gegner_Lavamann.cpp)
 * Runs fast (24) leaving burning flames behind, turns at walls, falls off edges, runs at the
 * player when shot; touching the player costs 35 and he bursts. Left out: the glow (additive
 * drawing); fewer flames (one every 12 steps, each burning 48 steps, costing 2). */
static void LAVAMANN_load(void) { }
static void LAVAMANN_init(Ent *e) { e->xs = FX(24); }
static int LAVAMANN_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	anim_loop(e, 0, 19, 1);
	if (++e->t1 >= 12)
	{
		e->t1 = 0;
		caves_flame_shot(x + 18 - (e->xs >> 8) * 3 / 4, y + 34, 0, 0, DMG(2), 48);
	}
	if (e->hit)
	{
		e->dir = pcx() > ecx(e);
		e->xs = e->dir ? FX(24) : FX(-24);
	}
	const int f = caves_floor(e, x, y);
	if (!(f & (B_WALL | B_PLATFORM))) { if (e->ys == 0) e->ys = FX(5); e->ya = FX(3); if (e->ys > FX(50)) e->ys = FX(50); }
	else { if (e->ys > 0) caves_land(e, x, y); e->ys = 0; e->ya = 0; }
	if (e->dir == 0 && (bl & (B_WALL | B_EWALL))) { e->dir = 1; e->xs = FX(24); }
	else if (e->dir == 1 && (br & (B_WALL | B_EWALL))) { e->dir = 0; e->xs = FX(-24); }
	if (touching) { player_damage(DMG(35)); e->energy = 0; }
	return 0;
}
static int LAVAMANN_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int LAVAMANN_draw(const Ent *e, uint32_t t)
{
	draw_pic(&P_LAVAMANN, e->anim, e->x, e->y, !e->dir);     /* the picture looks right */
	return 1;
}

/* ---------------------------------------------------------------- 68 the fire walker (Gegner_FieserFireWalker.cpp)
 * Walks (speed 5, frames 0-13), turns at walls; when the player is ahead of it within 400
 * (and 150 up or down) it stops and breathes flames at him until he comes too near or 40 are
 * out; shot, it turns to him and may fire at once. Touching costs 4 a tenth second.
 * state 0 walking, 1 breathing, 2 falling; t1 ShotDelay (steps), t2 TurnCount, size flames
 * left. Left out: the glow at its mouth; one flame every 8 steps instead of 2.4 (each costs
 * 10 and goes out after 64 steps; Hurrican's burn 4 a tenth second while they touch). */
static void FIESERWALKER2_load(void) { }
static void FIESERWALKER2_init(Ent *e) { e->xs = FX(5); e->t1 = 180; }
static int FIESERWALKER2_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int px = pl.x >> 8;
	if (e->state != 1) anim_loop(e, 0, 13, 3);
	if (e->state == 0)
	{
		if (!(bu & (B_WALL | B_PLATFORM))) { e->state = 2; e->ys = FX(2); e->ya = FX(3); }
		if (e->t1 <= 0 && e->anim == 3 && player_distance(e) < 400 && caves_vert(e) < 150 &&
		    ((e->dir && x + 59 < px + 20) || (!e->dir && x + 5 > px + 50)))
		{
			e->t1 = 0;
			e->size = 12;
			e->state = 1;
			e->xs = 0;
		}
		if (e->t1 > 0) e->t1--;
		if (e->t2 > 0) e->t2--;
		if (e->hit && e->t2 <= 0)
		{
			e->t2 = 120;
			if ((px < x && e->dir) || (px > x && !e->dir)) { e->dir ^= 1; e->xs = -e->xs; }
			e->t1 = -1;
		}
	}
	else if (e->state == 1)
	{
		if (--e->t1 <= 0)
		{
			e->t1 = 8;
			e->size--;
			const int sx = x + 5 + (e->dir ? 38 : -38), sy = y - 7;
			const int dx = px + 35 - sx, dy = (pl.y >> 8) + 5 - sy;
			int d = isqrt(dx * dx + dy * dy);
			if (d < 1) d = 1;
			fx xs = FX(22) * dx / d, ys = FX(22) * dy / d;
			if (ys > FX(5)) ys = FX(5);
			if (ys < FX(-5)) ys = FX(-5);
			if (xs >= 0 && xs < FX(5)) xs = FX(5);
			if (xs < 0 && xs > FX(-5)) xs = FX(-5);
			caves_flame_shot(sx + 28, sy + 34, xs, ys, DMG(10), 64);
			sfx(SFX_ESHOT);
		}
		if ((e->dir && x + 60 >= px) || (!e->dir && x <= px + 80)) e->size = 0;
		if (e->size <= 0)
		{
			e->t1 = 120;
			e->state = 0;
			e->xs = e->dir ? FX(5) : FX(-5);
		}
	}
	else
	{
		if (e->ys > FX(35)) { e->ys = FX(35); e->ya = 0; }
		if (bu & (B_WALL | B_PLATFORM)) { caves_land(e, x, y); e->ys = 0; e->ya = 0; e->state = 0; }
	}
	if (((bl & (B_WALL | B_EWALL)) && e->xs < 0) || ((br & (B_WALL | B_EWALL)) && e->xs > 0)) { e->xs = -e->xs; e->dir ^= 1; }
	return 0;
}
static int FIESERWALKER2_explode(Ent *e)
{
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int FIESERWALKER2_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 64 the slime ball (Gegner_SchleimBoller.cpp)
 * Jumps about towards the player; shot, one of size 30 or more splits in two of size - 8
 * (Value1 the size, 60 if 0; its energy = its size). Touching costs 2 + 2 a tenth second and
 * pushes the player away. state 0 in the air, 1 sitting (frames up to 11, then it jumps).
 * Left out: the scaling (all sizes are drawn full size), the glow, the slime drops. */
static void SCHLEIMBOLLER_load(void) { }
static void SCHLEIMBOLLER_init(Ent *e)
{
	e->size = (uint8_t)(e->v1 > 0 && e->v1 < 256 ? e->v1 : 60);
	e->energy = e->size;
	e->xs = (int16_t)FX(e->v2);
	e->ys = e->v2 ? FX(-40) : 0;
	e->ya = FX(8);
}
static int SCHLEIMBOLLER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	e->dir = 0;
	if (e->state == 0)
	{
		anim_loop(e, 0, 1, 12);
		if (e->ys > FX(50)) { e->ys = FX(50); e->ya = 0; }
		if (e->ys > 0 && (bu & (B_WALL | B_PLATFORM)))
		{
			caves_land(e, x, y);
			e->state = 1;
			e->ys = e->ya = e->xs = 0;
			e->animt = 0;
		}
		if (e->ys < 0 && (bo & (B_WALL | B_EWALL))) e->ys = -e->ys;
	}
	else if (++e->animt >= 3)
	{
		e->animt = 0;
		if (++e->anim >= 12)
		{
			e->state = 0;
			e->ys = (int16_t)FX(-(30 + rnd(10)));
			e->ya = FX(8);
			e->anim = 0;
			e->xs = (int16_t)FX(6 + rnd(8));
			if (pcx() <= x + 30) e->xs = (int16_t)-e->xs;
		}
	}
	caves_push(e, touching, 0);
	return 0;
}
static int SCHLEIMBOLLER_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8, size = e->size;
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_HIT);
	if (size >= 30)
	{
		const int mul = x + 30 - size / 2 > pcx() ? -1 : 1;
		for (int k = 0; k < 2; k++)
		{
			Ent *s = ent_new(E_SCHLEIMBOLLER, k ? x - 20 * mul : x + 20 * mul, y);
			if (!s) break;
			s->active = 1;
			s->v1 = (int16_t)(size - 8);
			s->v2 = k ? -5 : 5;
			SCHLEIMBOLLER_init(s);
		}
	}
	return 1;
}
static int SCHLEIMBOLLER_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 82 the spiked ball (Gegner_Stachelbeere.cpp)
 * Rolls along the passage (xAcc 10, at most 10), turns at walls, bounces when it falls; now
 * and then (near the player, every 50 tenths) it rolls out, opens (frames of two halves, 120
 * wide), fires 4 homing mini rockets and closes. Only open can it be hurt (energy kept in v2
 * while shut); touching pushes the player away and costs 4 a tenth second; dying it lets 3
 * rockets out. state 0 rolling, 1 slowing, 2 opening, 3 firing, 4 closing; t1 RollCount
 * (steps), t2 rockets left. Left out: the sparks and smoke when it lands. */
static void STACHELBEERE_load(void) { }
static void STACHELBEERE_init(Ent *e) { e->v2 = e->energy; }
static void caves_rocket(int x, int y)
{
	Ent *r = ent_new(E_STACHELROCKET, x, y);
	if (!r) return;
	r->active = 1;
	r->ys = FX(-20);
	r->t1 = 18;
}
static int STACHELBEERE_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int open = e->state >= 2;
	if (open) e->v2 = e->energy;
	else e->energy = e->v2;                 /* shut: not hurt (Destroyable = false) */
	const int f = caves_floor(e, x, y);
	if (!(f & (B_WALL | B_PLATFORM))) e->ya = FX(8);
	if (e->ys > 0)
	{
		if (e->ys > FX(40)) e->ys = FX(40);
		if (f & (B_WALL | B_PLATFORM))
		{
			caves_land(e, x, y);
			e->ys = (int16_t)(-e->ys / 2);
			if (e->ys > FX(-5)) { e->ys = 0; e->ya = 0; }
		}
	}
	const int sp = abs(e->xs);
	int per = 17 - (sp * 3 / 2 >> 8);
	if (per < 1) per = 1;
	switch (e->state)
	{
	case 0:                                          /* rolling */
		if (e->t1 > 0) e->t1--;
		anim_loop(e, 0, 4, per);
		e->xa = e->dir ? FX(10) : FX(-10);
		if (e->xs > FX(10)) e->xs = FX(10);
		if (e->xs < FX(-10)) e->xs = FX(-10);
		if ((!e->dir && (bl & (B_WALL | B_EWALL))) || (e->dir && (br & (B_WALL | B_EWALL))))
		{
			e->xs = (int16_t)-e->xs;
			e->dir ^= 1;
		}
		if (e->ys == 0 && e->t1 <= 0 && (f & B_WALL) && player_distance(e) < 400) { e->state = 1; e->xa = 0; }
		break;
	case 1:                                          /* slowing down */
		anim_loop(e, 0, 4, per);
		if (e->xs > 0) { e->xs -= SYNC(FX(1)); if (e->xs <= 0) e->xs = 0; }
		else if (e->xs < 0) { e->xs += SYNC(FX(1)); if (e->xs >= 0) e->xs = 0; }
		if (e->xs == 0) { e->state = 2; e->anim = 0; e->animt = 0; }
		break;
	case 2:                                          /* opening */
		if (++e->animt > 5) { e->animt = 0; e->anim++; }
		if (e->anim >= 15) { e->state = 3; e->t2 = 4; e->t1 = 24; e->anim = 14; }
		break;
	case 3:                                          /* firing */
		if (--e->t1 < 0)
		{
			caves_rocket(x + e->t2 * 15 + (e->dir ? 25 : -60), y + 10);
			e->t1 = 12;
			if (--e->t2 <= 0) e->state = 4;
		}
		break;
	default:                                         /* closing */
		if (++e->animt > 5) { e->animt = 0; e->anim--; }
		if (e->anim <= 0) { e->anim = 0; e->state = 0; e->t1 = 300; }
		break;
	}
	caves_push(e, touching, DMG_T(4));
	return 0;
}
static int STACHELBEERE_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	caves_rocket(x + 15, y + 35);
	caves_rocket(x + 45, y + 35);
	caves_rocket(x + 30, y + 20);
	fx_add(&P_explbig, ecx(e), ecy(e), 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int STACHELBEERE_draw(const Ent *e, uint32_t t)
{
	if (e->state < 2) { draw_pic(&P_STACHELBEERE, e->anim, e->x, e->y, e->dir); return 1; }
	/* open: a 120 x 60 picture = two 60 x 60 frames; looking left it is mirrored 60 further left */
	const int k = e->anim < 0 ? 0 : e->anim > 14 ? 14 : e->anim;
	const int a = (k / 3 + 1) * 6 + (k % 3) * 2;
	if (e->dir) { draw_pic(&P_STACHELBEERE, a, e->x, e->y, 0); draw_pic(&P_STACHELBEERE, a + 1, e->x + FX(60), e->y, 0); }
	else { draw_pic(&P_STACHELBEERE, a + 1, e->x - FX(60), e->y, 1); draw_pic(&P_STACHELBEERE, a, e->x, e->y, 1); }
	return 1;
}

/* ---------------------------------------------------------------- the spiked ball's mini rocket (Gegner_MiniRocket.cpp)
 * Rises (speed 20) for 3 tenths, then homes in on the player (acceleration 2.5, at most 10);
 * bursts at walls or on him (8). Left out: the turning of the picture, the flare, the smoke. */
static void STACHELROCKET_load(void) { }
static void STACHELROCKET_init(Ent *e) { }
static int STACHELROCKET_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->t1 > 0) e->t1--;
	else
	{
		e->xa = x + 6 < pcx() ? FX(5) / 2 : -FX(5) / 2;
		e->ya = y + 12 < (pl.y >> 8) + 40 ? FX(5) / 2 : -FX(5) / 2;
		if (e->xs > FX(10)) e->xs = FX(10);
		if (e->xs < FX(-10)) e->xs = FX(-10);
		if (e->ys > FX(10)) e->ys = FX(10);
		if (e->ys < FX(-10)) e->ys = FX(-10);
	}
	if (touching) { player_damage(DMG(8)); e->energy = 0; }
	if ((bl | br | bo | bu) & B_WALL) e->energy = 0;
	return 0;
}
static int STACHELROCKET_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int STACHELROCKET_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 156 / 157 the goggle eyes (Trigger_Glubschi*.cpp)
 * Scenery: the eye looks after the player (frame 0..20 by how far right of it he is). Left
 * out: the slowly turning glow. */
static void caves_glubschi(Ent *e)
{
	const int a = (e->x >> 8) - (pl.x >> 8);
	if (a < 200)
	{
		const int anim = (200 - a) / 18;
		e->anim = (int16_t)(anim > 20 ? 20 : anim);
	}
}
static void GLUBSCHI_load(void) { }
static void GLUBSCHI_init(Ent *e) { }
static int GLUBSCHI_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching) { caves_glubschi(e); return 1; }
static int GLUBSCHI_explode(Ent *e) { return 1; }
static int GLUBSCHI_draw(const Ent *e, uint32_t t) { return 0; }
static void GLUBSCHI2_load(void) { }
static void GLUBSCHI2_init(Ent *e) { }
static int GLUBSCHI2_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching) { caves_glubschi(e); return 1; }
static int GLUBSCHI2_explode(Ent *e) { return 1; }
static int GLUBSCHI2_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 172 the mushroom (Trigger_Mushroom.cpp)
 * A cap to stand on; it gives (squashing a little) and a jump within 2 tenths of landing on
 * it goes up to 2.5 times as fast. state 0 free, 1 stood on; t1 the 2 tenths (steps); t2 the
 * cap's height x 6 (366 = 61); spare = the player stood on it last step. The squash is drawn
 * as a shift down (no scaling). */
static void MUSHROOM_load(void) { }
static void MUSHROOM_init(Ent *e) { e->t2 = 366; }
static int MUSHROOM_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int on = pl.platform == idx + 1;
	if (e->spare && !on && pl.state == PS_JUMP && pl.ys < 0 && e->t1 > 0)
	{
		/* PlayerJumps: yspeed = -(43 x (0.5 + count)), at least 43 */
		pl.ys = -FX(43) * (3 + e->t1) / 6;
		pl.jumpadd = FX(25) / 2;
		sfx(SFX_LAND);
	}
	if (e->state == 0)
	{
		e->t2 += 10;
		if (e->t2 > 366) e->t2 = 366;
		if (on) { e->state = 1; e->t1 = 12; }
	}
	else
	{
		e->t2 -= 20;
		if (e->t2 < 330) e->t2 = 330;
		if (e->t1 > 0) e->t1--;
		if (!on) e->state = 0;
	}
	ent_platform(e, idx);
	e->spare = pl.platform == idx + 1;
	return 1;
}
static int MUSHROOM_explode(Ent *e) { return 1; }
static int MUSHROOM_draw(const Ent *e, uint32_t t)
{
	draw_pic(&P_MUSHROOM, 0, e->x, e->y + (366 - e->t2) * 256 / 3, 0);
	return 1;
}

/* ---------------------------------------------------------------- 155 the shoot platform (Trigger_ShootPlattform.cpp, Trigger_ShootButton.cpp)
 * A platform with a button on top: while the button is shot it rises (yAcc -3, at most 30;
 * bouncing back when the player's head meets the ceiling), else it sinks back to where it
 * was put. The button is a kind of its own (shootable), made on the platform's first step.
 * v1 the platform's rest height, v2 its button + 1. Changed: Hurrican's weapons that hit every
 * step (lightning, laser) and bounce along the floor are not here, so a hit lifts for the next
 * 12 steps (the spread gun's rate), and the button's box reaches 36 pixels up so that a shot
 * of the player ducking on the platform hits it. Left out: the jet flame and its glow (a puff
 * of smoke instead). */
static void SHOOTPLATTFORM_load(void) { }
static void SHOOTPLATTFORM_init(Ent *e) { e->v1 = (int16_t)(e->y >> 8); e->v2 = 0; }
static int SHOOTPLATTFORM_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->v2 && ents[e->v2 - 1].type != E_SHOOTBUTTON) e->v2 = 0;      /* (its button shot away) */
	if (!e->v2)
	{
		Ent *b = ent_new(E_SHOOTBUTTON, x + 42, y - 9);
		if (b) { b->active = 1; b->v1 = (int16_t)idx; e->v2 = (int16_t)(b - ents + 1); }
	}
	ent_platform(e, idx);
	if (e->ys > FX(30)) e->ys = FX(30);
	if (e->ys < FX(-30)) e->ys = FX(-30);
	if (e->ys > 0 && y > e->v1) { e->y = FX(e->v1); e->ys = 0; e->ya = 0; }
	if (pl.platform == idx + 1 && e->ys < 0)
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		if (blk_top(px, py, py, &player_box, 0) & B_WALL) e->ys = (int16_t)-e->ys;
	}
	return 0;
}
static int SHOOTPLATTFORM_explode(Ent *e) { return 1; }
static int SHOOTPLATTFORM_draw(const Ent *e, uint32_t t) { return 0; }

static void SHOOTBUTTON_load(void) { }
static void SHOOTBUTTON_init(Ent *e) { }
static int SHOOTBUTTON_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	Ent *p = &ents[e->v1];
	if (p->type != E_SHOOTPLATTFORM) { e->type = E_NONE; return 1; }
	e->x = e->xo = p->x + FX(42);
	e->y = e->yo = p->y - FX(9);
	if (e->energy < 100) { e->energy = 100; e->t2 = 12; }      /* shot: lifts for the next 12 steps */
	if (e->t2 > 0)
	{
		e->t2--;
		p->ya = FX(-3);
		if (++e->t1 >= 12) { e->t1 = 0; fx_add(&P_smoke, (p->x >> 8) + 50, (p->y >> 8) + 45, 3); }
	}
	else if ((p->y >> 8) < p->v1) p->ya = FX(3);
	anim_loop(e, 0, 1, 30);
	if (e->hit) e->hit--;
	return 1;
}
static int SHOOTBUTTON_explode(Ent *e) { return 1; }
static int SHOOTBUTTON_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 143 the fire trap (Trigger_Feuerspucker.cpp)
 * Set in a wall, pointing (Value1) up, right, down or left: rests 20 tenths, then spits flames
 * for Value2 tenths (while the player is within 800). Not shootable. state 0 resting, 1
 * spitting; t1 the time in it, t2 steps to the next flame. Left out: Hurrican's flames come
 * every 1.8 steps and burn 1 a tenth second while they touch; here one every 12 steps
 * (costing 15, burning out after 90). */
static void FEUERSPUCKER_load(void) { }
static void FEUERSPUCKER_init(Ent *e) { e->anim = (int16_t)(e->v1 & 3); e->t2 = 1; }
static int FEUERSPUCKER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->state == 0)
	{
		if (++e->t1 >= 120) { e->t1 = 0; e->state = 1; }
		return 1;
	}
	if (++e->t1 >= e->v2 * 6) { e->t1 = 0; e->state = 0; }
	if (player_distance(e) < 800 && --e->t2 <= 0)
	{
		e->t2 = 12;
		const fx wob = (fx)(FX(rnd(20) - 10) / 8);
		switch (e->v1)
		{
		case 0: caves_flame_shot(x + 33, y - 6, wob, FX(-20), DMG(15), 90); break;
		case 1: caves_flame_shot(x + 61, y + 27 - rnd(2), FX(25), wob - FX(1), DMG(15), 90); break;
		case 2: caves_flame_shot(x + 28, y + 64, wob, FX(20), DMG(15), 90); break;
		case 3: caves_flame_shot(x - 6, y + 19 - rnd(2), FX(-25), wob - FX(1), DMG(15), 90); break;
		default: break;
		}
	}
	return 1;
}
static int FEUERSPUCKER_explode(Ent *e) { return 1; }
static int FEUERSPUCKER_draw(const Ent *e, uint32_t t) { return 0; }

/* ---------------------------------------------------------------- 47 the biting fish (Gegner_NeuFisch.cpp)
 * Swims (speed 5..10, frames 0-8), turns at walls and, when the player is in the water, to
 * face him (frames 9-13 and back) and follows his height; when its mouth meets him it bites
 * on and hangs on him (1.5 a tenth second, and he jumps only half as high) until it is shot
 * or he leaves the water (Hurrican: until he rolls up as a wheel; then out of the water it
 * dies). state 0 swimming, 1 / 2 turning, 3 biting; t2 its speed; v1 / v2 where it holds on.
 * Left out: the blood, bubbles and bits. */
static void NEUFISCH_load(void) { }
static void NEUFISCH_init(Ent *e) { e->t2 = (int16_t)(FX(5) + FX(rnd(50)) / 10); }
static int NEUFISCH_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (e->state == 0) anim_loop(e, 0, 8, 3);
	else if (e->state == 1)
	{
		if (++e->animt >= 2) { e->animt = 0; if (++e->anim >= 14) { e->state = 2; e->dir ^= 1; } }
	}
	else if (e->state == 2)
	{
		if (++e->animt >= 2) { e->animt = 0; if (--e->anim <= 9) { e->anim = 0; e->state = 0; } }
	}
	if (e->state == 3)
	{
		/* biting: rides along with the player */
		e->anim = 0;
		if (pl.state == PS_DEAD) { e->state = 0; return 0; }
		player_damage(DMG_T(3) / 2);
		if (pl.state == PS_JUMP && pl.jumpadd == 0 && pl.ys < FX(-30)) { pl.ys /= 2; pl.jumpadd = FX(25) / 2; }
		e->x = e->xo = pl.x - FX(e->v1);
		e->y = e->yo = pl.y - FX(e->v2);
		if (!pl.in_liquid) e->energy = 0;
		if (e->hit) e->hit--;
		return 1;
	}
	e->ys = 0;
	e->xs = e->dir ? e->t2 : (int16_t)-e->t2;
	if (pl.in_liquid)
	{
		const int cx = (x + 45) / 20;
		if (y < py - 8 && (blk(cx, (y + 69) / 20) & B_LIQUID)) e->ys = (int16_t)(FX(50 + rnd(50)) / 10);
		if (y > py - 8 && y > 5 && (blk(cx, (y + 4) / 20) & B_LIQUID)) e->ys = (int16_t)-(FX(50 + rnd(50)) / 10);
	}
	if (e->state == 0)
	{
		const int wall = (!e->dir && (bl & (B_WALL | B_EWALL))) || (e->dir && (br & (B_WALL | B_EWALL)));
		if (wall || (pl.in_liquid && ((x + 30 < px + 35 && !e->dir) || (x + 30 > px + 35 && e->dir))))
		{
			e->state = 1; e->anim = 9; e->animt = 0;
			e->xs = 0; e->ys = 0;
		}
		static const Box mouth_l = { 10, 32, 20, 40 }, mouth_r = { 70, 32, 80, 40 };
		if (pl.state != PS_DEAD && boxes_meet(px, py, &player_box, x, y, e->dir ? &mouth_r : &mouth_l))
		{
			e->state = 3;
			e->v1 = (int16_t)(px - x);
			e->v2 = (int16_t)(py - y);
			e->anim = 0;
			e->xs = e->ys = 0;
		}
	}
	return 0;
}
static int NEUFISCH_explode(Ent *e)
{
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int NEUFISCH_draw(const Ent *e, uint32_t t) { return 0; }
