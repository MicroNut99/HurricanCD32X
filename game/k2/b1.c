/*
 * Stage bosses, group b1: the steel fist (stage 1), the pharaoh's head and the big fish (stage 2),
 * the elevator boss (stage 3), each after its Boss_*.cpp DoKI, and their helpers: the pharaoh's
 * homing laser and the piranhas the fish spits.
 *
 * Every boss: the screen held on its arena (boss_lock, Hurrican's ScrollLevel), the boss song once
 * the screen is there (or at once for the elevator boss), the energy bar, the stage song again and
 * the screen given back when it is beaten (ScrolltoPlayeAfterBoss). Hurrican's "Energy <= 100"
 * starts the dying; Energy is held at 100 until the end of it, then 0 (the engine's explosion,
 * NAME_explode, the score from the list).
 *
 * Timers are in 60 Hz steps (Hurrican's units x 6); angles in 1/64 degree.
 * A hit makes a boss flicker for one frame only (Hurrican: the boss fades red for a moment).
 * Left out everywhere: the particles (smoke, splinters, rock pieces, blood), the screen shaking,
 * the force feedback, the music fades (the songs change at once).
 */

static Pic b1_p_maul, b1_p_tail;                       /* the big fish's jaw and tail */
static Pic b1_p_head, b1_p_canon;                      /* the elevator boss's head and guns */
static Pic b1_p_klaser, b1_p_such, b1_p_ufolaser;      /* its shots */

/* a boss's hit flash: one frame (the shots set e->hit after the steps); PC check: HB_B1=1 prints
 * the boss's state every second */
static void b1_hit(Ent *e)
{
	e->hit = 0;
#ifndef OJ32X_MARS
	static int n;
	if (getenv("HB_B1") && ++n % 60 == 0)
		fprintf(stderr, "B1 type %d state %d energy %d at %d,%d cam %d,%d player %d,%d\n", e->type, e->state, e->energy,
		        (int)(e->x >> 8), (int)(e->y >> 8), cam_xoff(), cam_yoff(), (int)(pl.x >> 8), (int)(pl.y >> 8));
#endif
}

/* a crushing hit (1000 at once): PC check HB_B1_SOFT=1 makes it 149, which god mode (HB_GOD) ignores,
 * to play a whole fight standing */
static fx b1_crush(fx d)
{
#ifndef OJ32X_MARS
	if (getenv("HB_B1_SOFT") && d >= FX(150)) return FX(149);
#endif
	return d;
}

/* the player pushed along by dx (8.8), not into a wall */
static void b1_push_player(fx dx)
{
	const int px = pl.x >> 8, py = pl.y >> 8;
	if (dx < 0 && (blk_left(px, py, px, &player_box, 0) & B_WALL)) return;
	if (dx > 0 && (blk_right(px, py, px, &player_box, 0) & B_WALL)) return;
	pl.x += dx;
}

/* while a boss holds the screen the player stays in it (Player.cpp: Zustand != SCROLLBAR - x within
 * XOffset .. XOffset + 580, not above YOffset) */
static void b1_keep_player(void)
{
	if (pl.state == PS_DEAD) return;
	const int x0 = cam_xoff(), y0 = cam_yoff();
	if ((pl.x >> 8) < x0) pl.x = FX(x0);
	if ((pl.x >> 8) > x0 + 580) pl.x = FX(x0 + 580);
	if ((pl.y >> 8) < y0) pl.y = FX(y0);
}

/* ------------------------------------------------------------------ STAHLFAUST (120) */
/*
 * The steel fist (Boss_Stahlfaust.cpp), stage 1's mid boss. When it comes into view the screen
 * goes to Value1, Value2 and the fist is set 300 higher; once the screen is there the boss song
 * starts and it comes down (8) to Value2. Then it hovers over the player (accel 8, at most 18,
 * turning at Value1 - 90 / + 480), crushes down on him when he is under it (falls with accel 10
 * to the ground, back up with -1.5), and after 7 s of hovering flies out at the top (-5, accel -5,
 * 280 up),
 * comes down on him from above as a fist seen from above (50; 100 when it lands on him) and goes
 * up again (-0.5) to fly in from the top. Touching it costs 5 per unit. At 100 energy it bursts
 * for 5 s; 5000 points, the screen and the stage song come back.
 */
enum { SF_START, SF_INIT, SF_IN2, SF_IN, SF_RUN, SF_CRUSH, SF_CRUSHUP, SF_JUMP, SF_FALL, SF_STAND, SF_EXPL };
static void STAHLFAUST_load(void) { }
static void STAHLFAUST_init(Ent *e) { e->state = SF_START; e->t1 = 420; e->anim = 0; }
static int STAHLFAUST_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu;
	const Box *b = &kinds[E_STAHLFAUST].box;
	const int px = pl.x >> 8, top = e->v2;           /* top: ScrolltoY */
	b1_hit(e);
	if (e->state == SF_START)                        /* it comes into view: the screen to its arena */
	{
		boss_lock(e->v1, e->v2, 10);
		e->y -= FX(300);
		e->state = SF_INIT;
	}
	b1_keep_player();
	if (e->state != SF_INIT && e->state != SF_EXPL) boss_bar(e->energy, 4000);
	if (e->energy <= 100 && e->state != SF_EXPL)
	{
		e->state = SF_EXPL;
		e->xs = e->ys = e->xa = e->ya = 0;
		e->t1 = 300;
		e->t2 = 6;
	}
	switch (e->state)
	{
	case SF_INIT:                                    /* waits for the screen */
		if (boss_locked_there()) { music_boss(); e->state = SF_IN2; }
		break;
	case SF_IN2:                                     /* comes down the first time */
	case SF_IN:
		if (e->state == SF_IN2) e->energy = 4000;
		e->y += SYNC(FX(8));
		if (e->y >= FX(top)) { e->state = SF_RUN; e->xa = FX(-8); }
		break;
	case SF_RUN:                                     /* hovers over the player */
		e->t1--;
		if (px + player_box.r < x || x > e->v1 + 480) e->xa = FX(-8);
		if (px > x + b->r || x < e->v1 - 90) e->xa = FX(8);
		if (e->xs > FX(18)) e->xs = FX(18);
		if (e->xs < FX(-18)) e->xs = FX(-18);
		if (px < x + b->r - 115 && px + player_box.r > x + 155 && rnd(2) == 0)
		{
			e->state = SF_CRUSH;
			e->ys = 0; e->ya = FX(10); e->xs = 0; e->xa = 0;
		}
		if (e->t1 <= 0)                              /* out at the top */
		{
			e->t1 = 420;
			e->state = SF_JUMP;
			e->xa = 0; e->xs = 0; e->ys = FX(-5); e->ya = FX(-5);
		}
		break;
	case SF_CRUSH:                                   /* down on the player */
		if (blk_below(x, y, y, b, 0) & B_WALL)
		{
			sfx(SFX_LAND);
			fx_add(&P_smoke, x + b->r / 2 + 15, y + b->b - 10, 4);
			e->ya = -FX(1.5); e->ys = 0;
			e->state = SF_CRUSHUP;
		}
		break;
	case SF_CRUSHUP:                                 /* back up */
		if (e->ys < 0 && e->y <= FX(top))
		{
			e->state = SF_RUN;
			e->ys = 0; e->ya = 0;
			e->y = FX(top);
			e->xa = FX(-8);
		}
		break;
	case SF_JUMP:                                    /* flies out at the top */
		if (e->y <= FX(top - 280))
		{
			e->state = SF_FALL;
			e->anim = 1;
			e->ya = 0; e->ys = FX(50);
			e->x = FX(px - 110);
		}
		break;
	case SF_FALL:                                    /* down on him from above */
		if (blk_below(x, y, y, b, 0) & B_WALL)
		{
			sfx(SFX_LAND);
			fx_add(&P_smoke, x + b->r / 2 + 15, y + b->b - 10, 4);
			e->ya = -FX(0.5); e->ys = 0;
			e->state = SF_STAND;
			if (touching) player_damage(DMG(100));
		}
		break;
	case SF_STAND:                                   /* up and out again */
		if (e->y <= FX(top - 280))
		{
			e->anim = 0;
			e->state = SF_IN;
			e->ys = 0; e->ya = 0;
			e->y = FX(top - 250);
		}
		break;
	case SF_EXPL:                                    /* bursts for 5 s */
		e->energy = 100;
		if (--e->t2 < 0)
		{
			e->t2 = 6;
			fx_add(rnd(8) == 0 ? &P_explbig : &P_expl, x + 30 + rnd(200), y + 50 + rnd(200), 2);
			sfx(SFX_EXPLODE);
		}
		if (--e->t1 <= 0) e->energy = 0;
		break;
	default:
		break;
	}
	if (e->state != SF_EXPL && touching) player_damage(DMG_T(5));
	return 0;
}
static int STAHLFAUST_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 90, y + 110, 2);
	fx_add(&P_explbig, x + 160, y + 180, 3);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int STAHLFAUST_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ PHARAOKOPF (121) */
/*
 * The pharaoh's head (Boss_PharaoKopf.cpp), stage 2's boss. The screen goes to Value1, Value2;
 * once there, the boss song, and the head rises out of the floor (4.5). Then over and over: it
 * slides at the player (1, accel 3; away from him when he touches it) until it slams into the
 * arena's side (25 from the edges of the 652 wide arena: 1000 for the player caught there, he is
 * pushed along by it), or - when he stands on it (it is a platform) - shoots up (-10, accel -3) to
 * crush him at the ceiling (200), falls back (5, accel 10; 25 and up again when it lands on him).
 * After a rest (10 or 20 units) it either slides again, hops (-10, accel 2) and drops a falling
 * rock from the ceiling over the player when it lands, or - cracked, below half energy - fires
 * pairs of homing lasers from its eyes (every 10 / 8.5 / 7 units by skill; 40 each). Touching it
 * costs 2 per unit. At 100 energy it crumbles for 20 units; 8000 points.
 */
enum { PK_START, PK_NOTV, PK_IN, PK_RUN, PK_CRUSH, PK_REC, PK_SPECIAL, PK_SHOOT, PK_EXPL };
static int b1_pk_left(Ent *e)
{
	if ((e->x >> 8) < e->v1 + 25) { e->x = FX(e->v1 + 25); return 1; }
	return 0;
}
static int b1_pk_right(Ent *e)
{
	const int lim = e->v1 + 652 - 25;
	if ((e->x >> 8) + 224 > lim) { e->x = FX(lim - 224); return 1; }
	return 0;
}
static int b1_pk_bottom(Ent *e)
{
	const int lim = e->v2 + 480 - 20;
	if ((e->y >> 8) + 224 > lim) { e->y = FX(lim - 224); return 1; }
	return 0;
}
/* a homing laser from the eyes: aimed at the player at 40 */
static void b1_phlaser(int x, int y)
{
	Ent *s = ent_new(E_B1_PHLASER, x, y);
	if (!s) return;
	s->active = 1;
	fx xs, ys;
	aim_at(x + 24, y + 24, 40, &xs, &ys);
	s->xs = (int16_t)xs;
	s->ys = (int16_t)ys;
	s->t1 = 60 * 6;
}
static void PHARAOKOPF_load(void) { }
static void PHARAOKOPF_init(Ent *e) { e->state = PK_START; e->anim = 0; }
static int PHARAOKOPF_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)bl; (void)br; (void)bo; (void)bu;
	const int px = pl.x >> 8;
	b1_hit(e);
	if (e->state != PK_START && e->state != PK_NOTV && e->state != PK_EXPL) boss_bar(e->energy, 6000);
	ent_platform(e, idx);                                /* the head is a platform too */
	const int on = pl.platform == idx + 1;
	if (e->state == PK_START) { boss_lock(e->v1, e->v2, 10); e->state = PK_NOTV; }
	b1_keep_player();
	if (e->energy <= 100 && e->state != PK_EXPL)
	{
		e->state = PK_EXPL;
		e->xs = e->ys = e->xa = e->ya = 0;
		e->t1 = 120;
		sfx(SFX_BIGEXPLODE);
	}
	if (touching) player_damage(DMG_T(2));
	if (e->energy >= 3000 && e->anim > 1) e->anim = 0;
	if (e->energy < 3000 && e->anim < 2)                 /* half its energy gone: cracked */
	{
		e->anim = 2;
		fx_add(&P_smoke, x + 112, y + 112, 4);
		sfx(SFX_EXPLODE);
	}
	switch (e->state)
	{
	case PK_NOTV:                                        /* waits for the screen */
		if (boss_locked_there()) { music_boss(); e->state = PK_IN; e->t2 = 0; }
		break;
	case PK_IN:                                          /* rises out of the floor */
		{
			e->energy = 6000;
			e->y -= SYNC(FX(4.5));
			if (++e->t2 >= 20) { e->t2 = 0; fx_add(&P_smoke, x + rnd(224), e->v2 + 420, 3); }
			const int lim = e->v2 + 480 - 20 - 224;
			if (e->y <= FX(lim))
			{
				if (on) { e->ys = FX(-10); e->ya = FX(-3); }
				else if (px < x) { e->xs = FX(-1); e->xa = FX(-3); }
				else { e->xs = FX(1); e->xa = FX(3); }
				e->y = FX(lim);
				sfx(SFX_LAND);
				e->state = PK_RUN;
			}
		}
		break;
	case PK_RUN:                                         /* off at the player */
		e->xs = 0; e->ys = 0;
		if (on) { e->ys = FX(-10); e->ya = FX(-3); }
		else
		{
			if (px < x) { e->xs = FX(-1); e->xa = FX(-3); }
			else { e->xs = FX(1); e->xa = FX(3); }
			if (touching) { e->xs = -e->xs; e->xa = -e->xa; }
		}
		e->state = PK_CRUSH;
		break;
	case PK_CRUSH:                                       /* against a wall, the ceiling or the floor */
		if (e->xs < 0)
		{
			if (touching) b1_push_player(SYNC(e->xs));
			if (b1_pk_left(e))
			{
				e->xs = 0; e->xa = 0;
				fx_add(&P_smoke, x - 10, y + 112, 3);
				sfx(SFX_BIGEXPLODE);
				e->state = PK_REC; e->t1 = 60;
				if (touching) player_damage(b1_crush(DMG(1000)));
			}
		}
		if (e->xs > 0)
		{
			if (touching) b1_push_player(SYNC(e->xs));
			if (b1_pk_right(e))
			{
				e->xs = 0; e->xa = 0;
				fx_add(&P_smoke, x + 224, y + 112, 3);
				sfx(SFX_BIGEXPLODE);
				e->state = PK_REC; e->t1 = 60;
				if (touching) player_damage(b1_crush(DMG(1000)));
			}
		}
		if (e->ys < 0)
		{
			if (y < e->v2)                               /* the ceiling */
			{
				sfx(SFX_BIGEXPLODE);
				e->y = FX(e->v2);
				e->ys = FX(5); e->ya = FX(10);
				if (on) player_damage(DMG(200));
				pl.platform = 0;
			}
		}
		else if (e->ys > 0)
		{
			if (pl.state != PS_DEAD && !on && touching)  /* lands on him */
			{
				player_damage(DMG(25));
				e->ys = FX(-10); e->ya = FX(-3);
			}
			if (b1_pk_bottom(e))
			{
				e->ys = 0; e->ya = 0;
				fx_add(&P_smoke, x + 112, y + 210, 3);
				sfx(SFX_BIGEXPLODE);
				e->t1 = 120;
				e->state = PK_REC;
			}
		}
		break;
	case PK_REC:                                         /* rests, then the next attack */
		if (--e->t1 <= 0)
		{
			int j = rnd(3);
			if (j == 0 && e->anim > 1) { e->state = PK_SHOOT; e->anim = 3; e->t1 = 48; }
			else j = rnd(2) + 1;
			if (j == 1)                                  /* hops: a rock from the ceiling */
			{
				e->ys = FX(-10); e->ya = FX(2);
				e->anim = e->anim == 0 ? 1 : 3;
				e->state = PK_SPECIAL;
			}
			if (j == 2 || on)
			{
				e->state = PK_RUN;
				if (j == 2) sfx(SFX_LAND);
			}
		}
		break;
	case PK_SPECIAL:
		if (b1_pk_bottom(e) && e->ys > 0)
		{
			Ent *r = ent_new(E_FALLINGROCK, px, e->v2);
			if (r) r->active = 1;
			fx_add(&P_smoke, x + 112, y + 210, 3);
			sfx(SFX_BIGEXPLODE);
			e->ys = 0; e->ya = 0;
			e->t1 = 60;
			e->state = PK_REC;
			e->anim = e->anim == 1 ? 0 : 2;
		}
		break;
	case PK_SHOOT:                                       /* the eyes fire */
		if (--e->t1 <= 0)
		{
			b1_phlaser(x + 68, y + 77);
			b1_phlaser(x + 112, y + 77);
			sfx(SFX_ESHOT);
			e->t1 = skill == 0 ? 60 : skill == 1 ? 51 : 42;
			if (rnd(2) == 0) { e->anim = 2; e->state = PK_REC; }
		}
		break;
	case PK_EXPL:                                        /* crumbles */
		e->energy = 100;
		if ((e->t1 & 7) == 0) fx_add(&P_smoke, x + rnd(224), y + rnd(224), 3);
		if (--e->t1 <= 0) e->energy = 0;
		break;
	default:
		break;
	}
	return 0;
}
static int PHARAOKOPF_explode(Ent *e)
{
	const int x = e->x >> 8, y = e->y >> 8;
	fx_add(&P_explbig, x + 60, y + 80, 2);
	fx_add(&P_explbig, x + 160, y + 140, 3);
	fx_add(&P_smoke, x + 112, y + 112, 4);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int PHARAOKOPF_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ B1_PHLASER (helper) */
/*
 * The pharaoh's laser (Projectiles.cpp PHARAOLASER): starts aimed at the player at 40, then flies
 * on with its vertical speed held to 20 and turned towards his height (accel 5); bounces off the
 * floor, bursts at walls and the ceiling; 40 when it hits him. Left out: its smoke trail.
 */
static void B1_PHLASER_load(void) { }
static void B1_PHLASER_init(Ent *e) { (void)e; }
static int B1_PHLASER_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx;
#ifndef OJ32X_MARS
	if (getenv("HB_B1") && (e->t1 % 12) == 0) fprintf(stderr, "B1 laser at %d,%d ys %d\n", x, y, e->ys);
#endif
	anim_loop(e, 0, 3, 3);
	if (e->ys > FX(20)) e->ys = FX(20);
	if (e->ys < FX(-20)) e->ys = FX(-20);
	const int py = (pl.y >> 8) + 40;
	if (py < y + 24) e->ya = FX(-5);
	else if (py > y + 24) e->ya = FX(5);
	if ((bu & B_WALL) && e->ys > 0) e->ys = (int16_t)-e->ys;
	if (((bl | br | bo) & B_WALL) || --e->t1 <= 0) { e->energy = 0; return 1; }
	if (touching) { player_damage(DMG(40)); e->energy = 0; return 1; }
	return 0;
}
static int B1_PHLASER_explode(Ent *e)
{
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_HIT);
	return 1;
}
static int B1_PHLASER_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ BIGFISH (137) */
/*
 * The big fish (Boss_BigFish.cpp), stage 2's mid boss under water. When it comes into view the
 * screen goes to Value1, Value2 and the fish is set 400 to the right; once there, the boss song,
 * and it swims in to Value1 + 450. Its actions, never the same twice in a row: swim to three
 * random places (MoveTo: 10, braking 1 past the target), spit five piranhas one by one (fast jaw)
 * or open the jaw slowly and spit nine in a fan; first it swims to the player's x (at most
 * Value1 + 400) when he is to its right. Every 80 units a spiked ball (small, medium, then big
 * as its energy goes) or a swimming walker comes in from a side. It is hit only at its mouth (the
 * list's box); touching its body costs 8 per unit. At 100 energy the stage song and the screen
 * come back and it sinks 300; then it bursts into piranhas (Hurrican 20, here 10); 5000 points.
 * For the frame rate at most 12 of its piranhas and 4 of its balls / walkers live at once (Hurrican
 * has no limit; a long fight piles them up).
 * Drawn: the body, the jaw (opening by moving down instead of turning) and the tail; left out:
 * the three smaller fins, the glow, the blood.
 */
enum { BF_START, BF_NOTV, BF_IN, BF_RUN, BF_SHOOT, BF_SPECIAL, BF_EXPL };
#define B1_BF_EXTRAS 4
static struct {
	int32_t by, nx, ny;            /* yPos without the swimming wave; MoveTo's target (8.8) */
	int16_t maul, sinoff, kugel;   /* MaulWinkel, SinOff (1/64 degree), KugelCount (steps) */
	int8_t shots, last;            /* ShotCount, the last action */
	uint8_t moving, iskugel, shot;
	uint8_t npir;                  /* its piranhas alive (B1_PIRANHA) */
	int16_t extra[B1_BF_EXTRAS];   /* the balls and walkers it sent in (entity + 1, 0 = none) */
	uint8_t extra_t[B1_BF_EXTRAS]; /* and their kinds */
} b1_bf;
/* for the frame rate: at most this many of its piranhas, and of its balls and walkers, at once
 * (Hurrican: no limit - a long fight piles them up) */
#define B1_BF_PIRANHAS 12
static void b1_bf_moveto(Ent *e, int32_t nx, int32_t ny)
{
	b1_bf.moving = 1;
	b1_bf.nx = nx;
	b1_bf.ny = ny;
	e->xs = e->ys = 0;
	if (e->x < nx) e->xs = FX(10);
	if (e->x > nx) e->xs = FX(-10);
	if (b1_bf.by < ny) e->ys = FX(10);
	if (b1_bf.by > ny) e->ys = FX(-10);
}
static void b1_bf_domove(Ent *e)
{
	const int d = SYNC(FX(1));
	if (e->xs < 0 && b1_bf.nx > e->x) { e->xs = (int16_t)(e->xs + d); if (e->xs > 0) e->xs = 0; }
	if (e->xs > 0 && b1_bf.nx < e->x) { e->xs = (int16_t)(e->xs - d); if (e->xs < 0) e->xs = 0; }
	if (e->ys < 0 && b1_bf.ny > b1_bf.by) { e->ys = (int16_t)(e->ys + d); if (e->ys > 0) e->ys = 0; }
	if (e->ys > 0 && b1_bf.ny < b1_bf.by) { e->ys = (int16_t)(e->ys - d); if (e->ys < 0) e->ys = 0; }
	if (e->xs == 0 && e->ys == 0) b1_bf.moving = 0;
}
static void b1_bf_newaction(Ent *e)
{
	if (pl.x > e->x)
	{
		int32_t nx = pl.x;
		if (nx > FX(e->v1 + 400)) nx = FX(e->v1 + 400);
		b1_bf_moveto(e, nx, FX(e->v2 + 200));
	}
	int j = rnd(3);
	while (j == b1_bf.last) j = rnd(3);
	b1_bf.last = (int8_t)j;
	if (j == 0) { b1_bf.shots = 1; e->state = BF_SPECIAL; }
	else if (j == 1) { b1_bf.shots = 5; e->state = BF_SHOOT; }
	else { b1_bf.shots = 3; e->state = BF_RUN; }
}
/* a piranha: mode 0 spat (hunts the player in the water), 1 the fan (shot out at -20..-40,
 * i * 3, slowing down over 10 units, then hunting), 2 from the burst fish (hunts within 150) */
static void b1_piranha(int x, int y, int mode, int i)
{
	if (b1_bf.npir >= B1_BF_PIRANHAS) return;
	Ent *p = ent_new(E_B1_PIRANHA, x, y);
	if (!p) return;
	b1_bf.npir++;
	p->active = 1;
	if (mode == 1)
	{
		p->state = 0;
		p->dir = 0;
		p->v1 = (int16_t)FX(-20 - rnd(20));
		p->v2 = (int16_t)FX(i * 3);
		p->xs = p->v1; p->ys = p->v2;
		p->t1 = 60;
		p->t2 = 1;
	}
	else
	{
		p->state = 1;
		p->dir = (uint8_t)rnd(2);
		p->xs = p->dir ? FX(8) : FX(-8);
		p->t2 = (int16_t)(mode == 0 ? 1 : 2 + rnd(20));
	}
}
static void BIGFISH_load(void)
{
	b1_p_maul = pic("bigfishmaul.png");
	b1_p_tail = pic("bigfishflossebig");
}
static void BIGFISH_init(Ent *e)
{
	e->state = BF_START;
	memset(&b1_bf, 0, sizeof b1_bf);
	b1_bf.kugel = 900;
	b1_bf.last = -1;
	b1_bf.by = e->y;
}
static int BIGFISH_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	b1_hit(e);
	if (e->state == BF_START)                            /* comes into view: the screen to its arena */
	{
		boss_lock(e->v1, e->v2, 10);
		b1_bf.by = e->y;
		e->x += FX(400);
		e->state = BF_NOTV;
	}
	/* now and then a spiked ball or a swimming walker from a side */
	if (--b1_bf.kugel <= 0)
	{
		const int side = rnd(2) ? 1 : -1;
		const int art = e->energy > 1500 ? 0 : e->energy > 500 ? 1 : 2;
		b1_bf.kugel = 480;
		int k = 0;                                       /* a free place in its list */
		while (k < B1_BF_EXTRAS && b1_bf.extra[k] && ents[b1_bf.extra[k] - 1].type == b1_bf.extra_t[k]) k++;
		if (k < B1_BF_EXTRAS)
		{
			Ent *w;
			if (b1_bf.iskugel)
			{
				static const int16_t en[3] = { 20, 30, 40 };
				w = ent_new(E_KUGEL, cam_xoff() + 300 + 300 * side, cam_yoff() + 200);
				if (w) { w->size = (uint8_t)art; w->energy = en[art]; w->xs = (int16_t)FX(-5 * side); w->ya = FX(5); }
			}
			else
			{
				w = ent_new(E_SWIMWALKER, cam_xoff() + 300 + 320 * side, cam_yoff() + 350);
				if (w) { w->dir = side < 0; w->xs = side < 0 ? FX(7) : FX(-7); }
			}
			if (w)
			{
				w->active = 1;
				b1_bf.extra[k] = (int16_t)(w - ents + 1);
				b1_bf.extra_t[k] = w->type;
			}
		}
		b1_bf.iskugel ^= 1;
	}
	b1_bf.sinoff = (int16_t)(b1_bf.sinoff + 122);       /* 0.2 rad per unit */
	if (b1_bf.sinoff >= 360 * 64) b1_bf.sinoff = (int16_t)(b1_bf.sinoff - 360 * 64);
	if (e->state != BF_EXPL) { boss_bar(e->energy, 2000); b1_keep_player(); }
	if (e->energy <= 100 && e->state != BF_EXPL)
	{
		music_stage();
		boss_unlock();
		e->state = BF_EXPL;
		e->xs = 0; e->ys = FX(2); e->xa = 0; e->ya = FX(1);
		b1_bf.moving = 0;
	}
	if (b1_bf.moving) b1_bf_domove(e);
	else switch (e->state)
	{
	case BF_NOTV:                                        /* waits for the screen */
		if (boss_locked_there()) { music_boss(); e->state = BF_IN; }
		break;
	case BF_IN:                                          /* swims in */
		e->energy = 2000;
		b1_bf_newaction(e);
		b1_bf_moveto(e, FX(e->v1 + 450), b1_bf.by);
		break;
	case BF_RUN:                                         /* swims about */
		b1_bf_moveto(e, FX(e->v1 + 50 + rnd(300)), FX(e->v2 + 50 + rnd(200)));
		if (--b1_bf.shots <= 0) b1_bf_newaction(e);
		break;
	case BF_SHOOT:                                       /* one piranha per jaw movement */
	case BF_SPECIAL:                                     /* nine at once, the jaw slowly */
		b1_bf.maul = (int16_t)(b1_bf.maul + (e->state == BF_SHOOT ? 367 : 61));
		if (b1_bf.maul > 180 * 64 && !b1_bf.shot)
		{
			const int px = x + 80, py = (int)(b1_bf.by >> 8) + 90;
			b1_bf.shot = 1;
			if (e->state == BF_SHOOT) b1_piranha(px, py, 0, 0);
			else for (int i = 0; i < 9; i++) b1_piranha(px, py, 1, i - 4);
			b1_bf.shots--;
		}
		if (b1_bf.maul > 360 * 64)
		{
			b1_bf.shot = 0;
			b1_bf.maul = 0;
			sfx(SFX_LAND);
			if (b1_bf.shots <= 0) b1_bf_newaction(e);
		}
		break;
	case BF_EXPL:                                        /* sinks, then bursts */
		e->energy = 100;
		if (b1_bf.by > FX(e->v2 + 300)) e->energy = 0;
		break;
	default:
		break;
	}
	/* its body hurts (the list's box is only the mouth, where it can be hit) */
	{
		static const Box body = { 60, 28, 280, 156 };
		if (pl.state != PS_DEAD &&
		    boxes_meet(pl.x >> 8, pl.y >> 8, pl.state == PS_DUCK ? &player_box_duck : &player_box, e->x >> 8, e->y >> 8, &body))
			player_damage(DMG_T(8));
	}
	/* it moves itself (no walls); e->y follows the swimming wave (5) */
	e->xo = e->x; e->yo = e->y;
	e->xs = (int16_t)(e->xs + SYNC(e->xa));
	e->ys = (int16_t)(e->ys + SYNC(e->ya));
	e->x += SYNC(e->xs);
	b1_bf.by += SYNC(e->ys);
	e->y = b1_bf.by + sin_d(b1_bf.sinoff >> 6) * 5;
	return 1;
}
static int BIGFISH_explode(Ent *e)
{
	const int x = e->x >> 8, y = (int)(b1_bf.by >> 8);
	for (int i = 0; i < 10; i++) b1_piranha(x + 40 + rnd(200), y + 30 + rnd(80), 2, 0);
	fx_add(&P_explbig, x + 140, y + 80, 2);
	sfx(SFX_BIGEXPLODE);
	return 1;
}
static int BIGFISH_draw(const Ent *e, uint32_t t)
{
	(void)t;
	const int32_t mw = -cos_d(b1_bf.maul >> 6) * 35;    /* the jaw's angle, -35 (shut) .. 35 (open), 8.8 */
	draw_pic(&b1_p_maul, 0, e->x + FX(51), e->y + FX(94) + mw * 2 / 5, 0);
	draw_pic(&b1_p_tail, 0, e->x + FX(203), e->y - FX(9), 0);
	draw_pic(&P_BIGFISH, 0, e->x, e->y, 0);
	return 1;
}

/* ------------------------------------------------------------------ B1_PIRANHA (helper) */
/*
 * A piranha spat by the big fish (Gegner_Piranha.cpp with Value1 98 / 99): swims (4..13, turning
 * at walls); in the water near the player (or always, Value2 1) it hunts him - 14 towards him,
 * turning round when it has passed him, and up or down (4) to his height. Touching it costs 1 per
 * unit; 30 energy, 200 points. Left out: the blood and pieces.
 */
static void B1_PIRANHA_load(void) { }
static void B1_PIRANHA_init(Ent *e) { (void)e; }
static int B1_PIRANHA_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)touching;
	const int chase = pl.in_liquid && pl.state != PS_DEAD && (e->t2 == 1 || player_distance(e) <= 150);
	if (e->state == 2)                                   /* turning round */
	{
		if (++e->animt >= 4)
		{
			e->animt = 0;
			if (++e->anim >= 15)
			{
				e->anim = 0;
				e->state = 1;
				e->dir ^= 1;
				e->xs = e->dir ? FX(8) : FX(-8);
			}
		}
	}
	else if (e->state == 1) anim_loop(e, 0, 9, 4);
	if ((e->xs < 0 && (bl & (B_WALL | B_EWALL))) || (e->xs > 0 && (br & (B_WALL | B_EWALL))))
	{
		e->xs = 0;
		e->state = 2; e->anim = 10; e->animt = 0;
	}
	if (chase)
	{
		const int py = (pl.y >> 8) + 4 + e->t2;
		if (py < y && !(bo & (B_WALL | B_EWALL)) && (bo & B_LIQUID)) e->y -= SYNC(FX(4));
		if (py > y && !(bu & (B_WALL | B_EWALL))) e->y += SYNC(FX(4));
	}
	if (e->state == 0)                                   /* shot out of the mouth, slowing down */
	{
		if (--e->t1 < 0) { e->xs = 0; e->ys = 0; e->state = 1; }
		else { e->xs = (int16_t)(e->v1 * e->t1 / 60); e->ys = (int16_t)(e->v2 * e->t1 / 60); }
	}
	else if (e->state == 1)
	{
		if (chase)
		{
			e->xs = e->dir ? FX(14) : FX(-14);
			const int px = pl.x >> 8;
			if ((e->dir == 0 && px > x + 45 - 20) || (e->dir == 1 && px + player_box.r < x))
			{
				e->xs = 0;
				e->state = 2; e->anim = 10; e->animt = 0;
			}
		}
		else e->xs = (int16_t)(e->dir ? FX(4 + rnd(10)) : -FX(4 + rnd(10)));
	}
	return 0;
}
static int B1_PIRANHA_explode(Ent *e)
{
	if (b1_bf.npir) b1_bf.npir--;
	fx_add(&P_expl, ecx(e), ecy(e), 2);
	sfx(SFX_EXPLODE);
	return 1;
}
static int B1_PIRANHA_draw(const Ent *e, uint32_t t) { (void)e; (void)t; return 0; }

/* ------------------------------------------------------------------ FAHRSTUHLBOSS (125) */
/*
 * The elevator boss (Boss_FahrstuhlBoss.cpp) at the end of stage 3's shaft: it rides down above
 * the lift (temple.c's FAHRSTUHL: its yPos = the lift's - TempY). When it comes into view, the
 * boss song; it comes in (TempY 800 -> 500, 5) while the screen moves 80 up (the lift's offset
 * 70 -> 150). Then at random: down to 380 and fire both moving guns' lasers down (30, 8; every
 * 6 units, 10-19 times) or drop stilt walkers onto the lift (every 35 units); up to 600 and swing
 * its head from side to side, shaking a boulder loose at each side (5-12 times), or follow the
 * player with its head and fire its big laser at him (20, accel 10; 20 damage), or drop stilt
 * walkers; at 500 fire homing shots from both guns (20, 8; every 5 units, 10-29 times). It is hit
 * only at its head (the list's box, which moves with the head: the entity's x, y is the head's).
 * While it rides, the player in the air and its shots move down with the lift (40), and every
 * 2 x 24 tiles the shaft repeats: the lift, the player, the enemies near the screen, the shots
 * and the effects are all set 960 back up. At 100 energy it bursts, falling through the screen;
 * 9000 points, the stage song, the lift's screen again; the lift goes on to the shaft's end.
 * The screen: boss_lock follows the lift every step (Hurrican's lift sets the screen to it); the
 * boss is drawn and fights 40 lower than Hurrican's (this screen lacks the window's top 80 lines,
 * and at TempY 600 its head would be above it). Bursting, the lift's own screen comes back.
 * Left out: the smoke at its feet and its sore head, the muzzle flares, the screen's offset going
 * back to 20 (the lift's own screen stays at 70).
 */
enum { FB_NOTV, FB_IN2, FB_IN, FB_DOWN, FB_UP, FB_SHOOT, FB_BOMB, FB_SPECIAL, FB_SPIT, FB_CRUSH, FB_EXPL };
static struct {
	int32_t fx, fy;                 /* the frame's top-left (xPos, yPos), 8.8 */
	int32_t tempy, x1, y1, x2, y2, x3, dx1, dx2, canon, off;   /* 8.8 */
	int16_t delay, shots, lift;     /* ShotDelay (steps), Shots, the lift's entity (-1 none) */
	int16_t val1;                   /* the screen's y when it came + 100 */
	uint8_t activated;
} b1_fb;
#define B1_FB_CARRY SYNC(FX(40))
#define B1_FB_DY 40                 /* the boss 40 lower than Hurrican's, so its head stays on this screen */    /* the player in the air and the shots go down with the lift */
static void FAHRSTUHLBOSS_load(void)
{
	b1_p_head = pic("fahrstuhlboss_he");
	b1_p_canon = pic("fahrstuhlboss_ca");
	b1_p_klaser = pic("krabblerlaser1.p");
	b1_p_such = pic("suchschuss.png");
	b1_p_ufolaser = pic("ufolaser.png");
}
static void FAHRSTUHLBOSS_init(Ent *e)
{
	memset(&b1_fb, 0, sizeof b1_fb);
	e->state = FB_NOTV;
	b1_fb.fx = e->x;
	b1_fb.fy = e->y;
	b1_fb.tempy = FX(800);
	b1_fb.x1 = FX(140); b1_fb.y1 = FX(160);
	b1_fb.x2 = FX(25);  b1_fb.y2 = FX(80);
	b1_fb.x3 = FX(340);
	b1_fb.delay = 30;
	b1_fb.off = FX(70);
	b1_fb.lift = -1;
	e->x = b1_fb.fx + b1_fb.x1;
	e->y = b1_fb.fy + b1_fb.y1;
}
/* the shaft repeats: everything near the screen 960 back up (Hurrican: 2 * 24 * 20) */
static void b1_fb_wrap(Ent *self)
{
	const int32_t a = FX(960);
	for (int k = 0; k < nactive; k++)
	{
		Ent *o = &ents[active_list[k]];
		if (o == self || o->type == E_NONE) continue;
		o->y -= a; o->yo -= a;
	}
	temple_lift_y -= a;
	pl.y -= a; pl.yo -= a; pl.jumpstart -= a; pl.safe_y -= a; pl.ground_y -= a;
	for (int i = 0; i < MAX_SHOTS; i++) if (shots[i].on) shots[i].y -= a;
	for (int i = 0; i < MAX_FX; i++) if (fxs[i].p) fxs[i].y -= a;
	b1_fb.fy -= a;
	cam_y -= 480;                                    /* the screen too (TileEngine.YOffset -= A) */
#ifndef OJ32X_MARS
	if (getenv("HB_B1")) fprintf(stderr, "B1 wrap: lift %d at %d, player %d, boss %d, nactive %d\n", b1_fb.lift, (int)(temple_lift_y >> 8), (int)(pl.y >> 8), (int)(b1_fb.fy >> 8), nactive);
#endif
}
/* a gun's shot, moving down with the lift */
static void b1_fb_shot(const Pic *p, int x, int y, fx xs, fx ys, fx ya, int dmg)
{
	enemy_shot_pic(p, x, y, xs, ys + FX(40), ya, dmg, 0);
}
static int FAHRSTUHLBOSS_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)x; (void)y; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	const int fxi = b1_fb.fx >> 8, fyi = b1_fb.fy >> 8;
	b1_hit(e);
	if (b1_fb.x1 < 0) b1_fb.x1 = 0;
	if (e->state != FB_NOTV && e->state != FB_EXPL) boss_bar(e->energy, 6000);
	if (!b1_fb.activated)                                /* it comes: find the lift (once) */
	{
		b1_fb.activated = 1;
		b1_fb.val1 = (int16_t)(cam_yoff() + 100);
		for (int i = 0; i < nents; i++)
			if (ents[i].type == E_FAHRSTUHL && ents[i].v1 == 0) { b1_fb.lift = (int16_t)i; break; }
	}
	const Ent *lift = b1_fb.lift >= 0 && ents[b1_fb.lift].type == E_FAHRSTUHL ? &ents[b1_fb.lift] : 0;
	if (e->energy <= 100 && e->state != FB_EXPL)
	{
		e->state = FB_EXPL;
		b1_fb.delay = 6;
		b1_fb.dx1 = b1_fb.dx2 = 0;
		boss_unlock();                                   /* the lift's own screen again */
	}
	/* the head and the guns move */
	b1_fb.x1 += SYNC(b1_fb.dx1);
	b1_fb.x2 += SYNC(b1_fb.dx2);
	b1_fb.x3 -= SYNC(b1_fb.dx2);
	b1_fb.canon += SYNC(b1_fb.dx2);
	if (b1_fb.canon >= FX(6)) b1_fb.canon = 0;
	if (b1_fb.canon < 0) b1_fb.canon = FX(6) - 1;
	if (b1_fb.x2 > FX(340)) { b1_fb.x2 = FX(25); b1_fb.x3 = FX(340); }
	if (e->state == FB_IN || e->state == FB_DOWN || e->state == FB_UP)   /* the head back to the middle */
		if ((b1_fb.dx1 < 0 && b1_fb.x1 < FX(140)) || (b1_fb.dx1 > 0 && b1_fb.x1 > FX(140))) { b1_fb.x1 = FX(140); b1_fb.dx1 = 0; }
	const fx back = b1_fb.x1 < FX(140) ? FX(10) : FX(-10);
	switch (e->state)
	{
	case FB_NOTV:                                        /* the boss song */
		music_boss();
		e->state = FB_IN2;
		break;
	case FB_IN2:                                         /* comes in (once) */
	case FB_IN:
		if (e->state == FB_IN2) e->energy = 6000;
		b1_fb.tempy -= SYNC(FX(5));
		if (b1_fb.tempy < FX(500))
		{
			b1_fb.tempy = FX(500);
			if (e->state == FB_IN2) { e->state = FB_DOWN; b1_fb.dx2 = FX(5); }
			else switch (rnd(6))
			{
			case 0: e->state = FB_DOWN; b1_fb.dx2 = 0; break;
			case 1: case 2: e->state = FB_DOWN; b1_fb.dx2 = FX(5); break;
			case 3: e->state = FB_SPIT; b1_fb.shots = (int16_t)(1 + rnd(2)); break;
			default: e->state = FB_CRUSH; b1_fb.shots = (int16_t)(10 + rnd(20)); b1_fb.delay = 12; break;
			}
		}
		b1_fb.off += SYNC(FX(4));
		if (b1_fb.off > FX(150)) b1_fb.off = FX(150);
		break;
	case FB_DOWN:                                        /* comes down */
		b1_fb.tempy -= SYNC(FX(5));
		if (b1_fb.tempy < FX(380))
		{
			b1_fb.tempy = FX(380);
			if (b1_fb.dx2 != 0) { e->state = FB_SHOOT; b1_fb.delay = 6; b1_fb.shots = (int16_t)(rnd(10) + 10); }
			else { e->state = FB_SPIT; b1_fb.delay = 30; b1_fb.shots = (int16_t)(2 + rnd(2)); }
		}
		break;
	case FB_UP:                                          /* goes up */
		b1_fb.tempy += SYNC(FX(5));
		if (b1_fb.tempy > FX(600))
		{
			b1_fb.tempy = FX(600);
			switch (rnd(3))
			{
			case 0: e->state = FB_BOMB; b1_fb.dx1 = FX(40); b1_fb.delay = 6; b1_fb.shots = (int16_t)(rnd(8) + 5); break;
			case 1: e->state = FB_SPECIAL; b1_fb.dx1 = 0; b1_fb.delay = 30; b1_fb.shots = (int16_t)(rnd(8) + 5); break;
			default: e->state = FB_SPIT; b1_fb.dx1 = 0; b1_fb.delay = 30; b1_fb.shots = (int16_t)(rnd(4) + 2); break;
			}
		}
		break;
	case FB_SHOOT:                                       /* both guns' lasers */
		if (--b1_fb.delay < 0)
		{
			b1_fb.delay = 36;
			const int sy = fyi + (b1_fb.y2 >> 8) + 95;
			b1_fb_shot(&b1_p_klaser, fxi + (b1_fb.x2 >> 8) + 28, sy, 0, FX(30), 0, DMG(8));
			b1_fb_shot(&b1_p_klaser, fxi + (b1_fb.x3 >> 8) + 28, sy, 0, FX(30), 0, DMG(8));
			b1_fb.y2 = FX(75);
			if (--b1_fb.shots <= 0) { b1_fb.dx2 = 0; e->state = FB_UP; }
		}
		break;
	case FB_BOMB:                                        /* the head swings: boulders */
		if ((b1_fb.x1 > FX(280) && b1_fb.dx1 > 0) || (b1_fb.x1 < 0 && b1_fb.dx1 < 0))
		{
			b1_fb.dx1 = -b1_fb.dx1;
			Ent *s = ent_new(E_BOULDER, fxi + 10 + rnd(360), fyi + 480);
			if (s) { s->active = 1; s->v1 = (int16_t)(40 + rnd(20)); s->ys = (int16_t)(temple_lift_ys + FX(s->v1)); }
			sfx(SFX_LAND);
			if (--b1_fb.shots <= 0) { e->state = FB_IN; b1_fb.dx1 = b1_fb.x1 < FX(140) ? FX(10) : FX(-10); }
		}
		break;
	case FB_SPECIAL:                                     /* the head follows him: the big laser */
		{
			if (b1_fb.delay > 0) b1_fb.delay--;
			const int32_t hx = b1_fb.fx + b1_fb.x1, pc = pl.x + FX(35);
			if (pc < hx + FX(60)) b1_fb.x1 -= SYNC(FX(8));
			if (pc > hx + FX(80)) b1_fb.x1 += SYNC(FX(8));
			if (pc < hx + FX(100) && pc > hx + FX(40) && b1_fb.delay <= 0)
			{
				b1_fb.delay = 48;
				b1_fb_shot(&b1_p_ufolaser, fxi + (b1_fb.x1 >> 8) + 80 - 28, fyi + (b1_fb.y1 >> 8) + 155, 0, FX(20), FX(10) / 6, DMG(20));
				b1_fb.y1 = FX(155);
				if (--b1_fb.shots <= 0) { e->state = FB_IN; b1_fb.dx1 = back; }
			}
		}
		break;
	case FB_SPIT:                                        /* stilt walkers onto the lift */
		if (--b1_fb.delay < 0)
		{
			b1_fb.delay = 210;
			Ent *s = ent_new(E_STELZSACK, fxi + 10 + rnd(360), fyi + 480);
			if (s) s->active = 1;
			if (--b1_fb.shots <= 0)
			{
				b1_fb.dx2 = 0;
				e->state = b1_fb.tempy != FX(380) ? FB_IN : FB_UP;
				b1_fb.dx1 = back;
			}
		}
		break;
	case FB_CRUSH:                                       /* homing shots from both guns */
		if (--b1_fb.delay < 0)
		{
			b1_fb.delay = 30;
			const int sy = fyi + (b1_fb.y2 >> 8) + 100;
			for (int k = 0; k < 2; k++)
			{
				const int sx = fxi + ((k ? b1_fb.x3 : b1_fb.x2) >> 8) + 28;
				fx xs, ys;
				aim_at(sx, sy, 20, &xs, &ys);
				b1_fb_shot(&b1_p_such, sx, sy, xs, ys, 0, DMG(8));
			}
			b1_fb.y2 = FX(77);
			if (--b1_fb.shots <= 0) { b1_fb.dx2 = FX(5); e->state = FB_DOWN; b1_fb.dx1 = back; }
		}
		break;
	case FB_EXPL:                                        /* bursts, falling through the screen */
		if (--b1_fb.delay < 0)
		{
			b1_fb.delay = 6;
			fx_add(rnd(2) ? &P_explbig : &P_expl, fxi + rnd(400), fyi + rnd(280), 2);
			sfx(SFX_EXPLODE);
		}
		e->energy = 100;
		b1_fb.tempy -= SYNC(FX(10));
		if (b1_fb.tempy < FX(-400)) e->energy = 0;
		break;
	default:
		break;
	}
	/* its place above the lift (B1_FB_DY lower: this screen lacks Hurrican's top 80 lines) */
	b1_fb.fy = temple_lift_y - b1_fb.tempy + FX(B1_FB_DY);
	if (b1_fb.fy < 0) b1_fb.fy = 0;
	if (b1_fb.y2 < FX(80)) b1_fb.y2 += SYNC(FX(2)); else b1_fb.y2 = FX(80);
	if (b1_fb.y1 < FX(160)) b1_fb.y1 += SYNC(FX(2)); else b1_fb.y1 = FX(160);
	/* the player in the air goes down with the lift */
	if (!pl.platform && pl.state != PS_DEAD)
	{
		pl.y += B1_FB_CARRY; pl.yo += B1_FB_CARRY; pl.jumpstart += B1_FB_CARRY;
	}
	/* the shaft repeats */
	if (b1_fb.fy > FX(b1_fb.val1 + 960)) b1_fb_wrap(e);
	/* the screen on the lift (Hurrican's lift: XOffset = x + 200 - 320, YOffset = y - 320 - offset) */
	if (lift && e->state != FB_EXPL) boss_lock((lift->x >> 8) + 200 - 320, (int)(temple_lift_y >> 8) - 320 - (b1_fb.off >> 8), 12 * 640);
	e->xo = e->x; e->yo = e->y;
	e->x = b1_fb.fx + b1_fb.x1;
	e->y = b1_fb.fy + b1_fb.y1;
	if (e->state == FB_EXPL) e->y = b1_fb.fy;           /* falling: kept running and drawn by its top */
	return 1;
}
static int FAHRSTUHLBOSS_explode(Ent *e)
{
	(void)e;
	fx_add(&P_explbig, (b1_fb.fx >> 8) + 210, (b1_fb.fy >> 8) + 140, 2);
	sfx(SFX_BIGEXPLODE);
	boss_unlock();
	music_stage();
	return 1;
}
static int FAHRSTUHLBOSS_draw(const Ent *e, uint32_t t)
{
	(void)e; (void)t;
	const int c = b1_fb.canon >> 8;
	draw_pic(&P_FAHRSTUHLBOSS, 0, b1_fb.fx, b1_fb.fy, 0);
	draw_pic(&b1_p_head, 0, b1_fb.fx + b1_fb.x1, b1_fb.fy + b1_fb.y1, 0);
	draw_pic(&b1_p_canon, c, b1_fb.fx + b1_fb.x2, b1_fb.fy + b1_fb.y2, 0);
	draw_pic(&b1_p_canon, 5 - c, b1_fb.fx + b1_fb.x3, b1_fb.fy + b1_fb.y2, 0);
	return 1;
}
