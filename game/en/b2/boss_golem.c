/*
 * GOLEM - the lava golem (bosses/Boss_Golem.cpp): walks to and fro in its lava cave, throws
 * boulders, beats the wall so lava balls come up, spits homing fire balls and beats the lava
 * so a fire column rises.
 * Own members: f[0] = Wackel (the body's tilt, radians), f[1] = WackelDir, f[2] = rotarm1,
 *   f[3] = rotarm2 (the arms' angles, radians), f[4] = yoff, f[5] = ShotDelay, f[6] = count;
 *   i[0] = state2 (ArmState), i[1] = StoneCount, i[2] = last.
 * TileEngine.ScrolltoX / ScrolltoY are Value1 / Value2 (the screen it locks to).
 * Drawing: Hurrican rotates the body by Wackel and the arms by rotarm1 / rotarm2 around their
 * shoulders; here the three parts (back arm, body, front arm) are drawn upright (no rotation on
 * the 32X) with their middles where Hurrican's rotations put them, so the arms swing in place
 * but do not turn.
 * Left out: screen shaking, rumble. Hurrican fades the music: here it stops / starts.
 * The boulders (BOULDER), lava balls (LAVABALL) and the platform it leaves (PLATTFORM) belong
 * to other groups.
 */
enum { GOLEM_HEBEN, GOLEM_SENKEN, GOLEM_SENKEN2, GOLEM_UEBERKOPF, GOLEM_WERFEN };
#define GOLEM_WACKELMAX FX(0.1)
#define GOLEM_WACKEL    e->f[0]
#define GOLEM_WDIR      e->f[1]
#define GOLEM_ROT1      e->f[2]
#define GOLEM_ROT2      e->f[3]
#define GOLEM_YOFF      e->f[4]
#define GOLEM_SHOT      e->f[5]
#define GOLEM_COUNT     e->f[6]
#define GOLEM_STATE2    e->i[0]
#define GOLEM_STONES    e->i[1]
#define GOLEM_LAST      e->i[2]

static Pic golem_arm[2];

static void golem_init(Ent *e)
{
	GOLEM_LAST = 0;
	GOLEM_WACKEL = 0;
	GOLEM_WDIR = FX(1);
	GOLEM_ROT1 = 0;
	GOLEM_ROT2 = 0;
	GOLEM_YOFF = 0;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(8000);
	e->AnimPhase = 0;
	e->Destroyable = 1;
	e->OwnDraw = 1;
	GOLEM_STONES = 0;
}

/* the angle to the player (90 + atan(dy / dx) in degrees) */
static int golem_winkel_to_player(Ent *e)
{
	const int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + GOLEM_YOFF + FX(50)));
	const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(50)));
	int a;
	if (xdiv == 0) a = ydiv < 0 ? -90 : 90;
	else
	{
		a = atan2_d(ydiv == 0 ? 0 : ydiv, xdiv);    /* atan of the quotient: -90..90 */
		if (a > 90) a -= 180;
		if (a < -90) a += 180;
	}
	return 90 + a;
}

/* rock while walking */
static void golem_wackeln(Ent *e)
{
	GOLEM_YOFF = GOLEM_WACKEL > 0 ? GOLEM_WACKEL * 8 : -GOLEM_WACKEL * 8;
	if ((GOLEM_WDIR > 0 && GOLEM_WACKEL > GOLEM_WACKELMAX) || (GOLEM_WDIR < 0 && GOLEM_WACKEL < -GOLEM_WACKELMAX))
	{
		GOLEM_WDIR = -GOLEM_WDIR;
		GOLEM_WACKEL = GOLEM_WDIR > 0 ? -GOLEM_WACKELMAX : GOLEM_WACKELMAX;
		if (e->Handlung == GEGNER_EINFLIEGEN || e->Handlung == GEGNER_LAUFEN || e->Handlung == GEGNER_LAUFEN2 ||
		    e->Handlung == GEGNER_LAUFEN3 || e->Handlung == GEGNER_LAUFEN4)
		{
			shake_screen(4);
			snd(SOUND_DOORSTOP);
		}
	}
}

/* a value towards zero at speed; 1 when it is there */
static int golem_go_to_zero(fx *v, fx speed)
{
	int fertig = 1;
	if (*v > 0)
	{
		fertig = 0;
		*v -= SYNC(speed);
		if (*v < 0) *v = 0;
	}
	if (*v < 0)
	{
		fertig = 0;
		*v += SYNC(speed);
		if (*v > 0) *v = 0;
	}
	return fertig;
}

/* one step of walking: tilt and swing */
static void golem_walk(Ent *e, fx dx)
{
	e->xPos += SYNC(dx);
	GOLEM_WACKEL += SYNC(FMUL(GOLEM_WDIR, FX(0.025)));
	GOLEM_ROT1 -= SYNC(FMUL(GOLEM_WDIR, FX(0.2)));
	GOLEM_ROT2 += SYNC(FMUL(GOLEM_WDIR, FX(0.2)));
	golem_wackeln(e);
}

/* (px, py) turned by rad (radians, fx) as glm::rotate about z does on the screen */
static void golem_rot(fx px, fx py, fx rad, fx *ox, fx *oy)
{
	const int deg = FINT(FMUL(rad, FX(57.29578)));
	const fx c = cos_d(deg), s = sin_d(deg);
	*ox = FMUL(px, c) - FMUL(py, s);
	*oy = FMUL(px, s) + FMUL(py, c);
}

/* Hurrican turns the arms around their shoulders and everything around the body's middle by
 * Wackel; here each part is drawn upright with its middle where Hurrican's turned middle is */
static void golem_part(Ent *e, const Pic *p, fx cx, fx cy, fx px, fx py, fx qx, fx qy, fx rot)
{
	fx x, y, bx, by;
	const fx mx = e->xPos + FX(48) + GOLEM_YOFF / 2, my = e->yPos + FX(215.5);   /* the body's turning point */
	golem_rot(cx - px, cy - py, rot, &x, &y);
	x += qx;
	y += qy;
	golem_rot(x - mx, y - my, GOLEM_WACKEL, &bx, &by);
	draw_sprite(p, 0, mx + bx - FX(p->fw), my + by - FX(p->fh), 0);
}

static void golem_draw(Ent *e)
{
	fx wackel = 0;
	if (e->Handlung == GEGNER_EXPLODIEREN) wackel = FX(FINT(GOLEM_SHOT) % 2);
	const fx x = e->xPos + wackel, y = e->yPos + GOLEM_YOFF;
	const Pic *a0 = &golem_arm[0], *a1 = &golem_arm[1], *b = &GegnerPic[e->GegnerArt];
	/* the arm behind: turned by rotarm2 - 0.5 around (x + ARMX + 205, y + ARMY + 10), moved to (.. + 120, .. + 80) */
	golem_part(e, a0, x + FX(a0->fw), y + FX(a0->fh), e->xPos + FX(150), e->yPos + FX(30),
	           e->xPos + FX(65), e->yPos + FX(100), GOLEM_ROT2 - FX(0.5));
	/* the body */
	golem_part(e, b, x + FX(b->fw), y + FX(b->fh), 0, 0, 0, 0, 0);
	/* the arm in front: turned by rotarm1 - 0.3 around (x + ARMX + 180, y + ARMY + 50), moved to (.. + 175, .. + 95) */
	golem_part(e, a1, x + FX(a1->fw), y + FX(a1->fh), e->xPos + FX(125), e->yPos + FX(70),
	           e->xPos + FX(120), e->yPos + FX(115), GOLEM_ROT1 - FX(0.3));
}

static void golem_ki(Ent *e)
{
	const fx sx = FX(e->Value1), sy = FX(e->Value2);   /* ScrolltoX / ScrolltoY */
	(void)sy;

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
	{
		if (e->Handlung != GEGNER_EINFLIEGEN) b2test_hurt(e);
		boss_bar(e->Energy, FX(8000));
	}

	/* lock the screen on the golem as soon as it is seen */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		e->xPos += FX(400);                       /* hide */
		scroll_level(sx, sy, TS_SCROLLTOLOCK);
		music_stop();
	}

	/* the mid boss does not flash as long */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* no energy left? then it explodes */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Destroyable = 0;
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		e->AnimCount = FX(1);
		GOLEM_SHOT = FX(30);
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:        /* wait until the screen is in place */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
			snd(SOUND_MUTANT);
		}
		break;

	case GEGNER_EINFLIEGEN:        /* walks in slowly to the left */
		e->Energy = FX(8000);
		e->DamageTaken = 0;
		golem_walk(e, FX(-10));
		if (e->xPos <= sx + FX(380))
		{
			e->xPos = sx + FX(380);
			e->Handlung = GEGNER_SPECIAL;
		}
		break;

	case GEGNER_LAUFEN:            /* to the middle */
		golem_walk(e, FX(-10));
		if (e->xPos <= sx + FX(380))
		{
			e->xPos = sx + FX(380);
			e->Handlung = GEGNER_SPECIAL;
		}
		break;

	case GEGNER_LAUFEN2:           /* to the right */
		golem_walk(e, FX(10));
		if (e->xPos >= sx + FX(540))
		{
			e->xPos = sx + FX(540);
			e->Handlung = GEGNER_SPECIAL;
		}
		break;

	case GEGNER_LAUFEN3:           /* all the way left to the wall */
		golem_walk(e, FX(-10));
		if (e->xPos <= sx + FX(100))
		{
			e->xPos = sx + FX(100);
			e->Handlung = GEGNER_SPECIAL2;
			GOLEM_LAST = GEGNER_SPECIAL2;
			GOLEM_STATE2 = GOLEM_SENKEN;
			GOLEM_STONES = 3 + GetRandom(2);
		}
		break;

	case GEGNER_LAUFEN4:           /* from the left to the middle */
		golem_walk(e, FX(10));
		if (e->xPos >= sx + FX(380))
		{
			e->xPos = sx + FX(380);
			e->Handlung = GEGNER_SPECIAL;
		}
		break;

	case GEGNER_SPECIAL2:          /* beat the wall: lava balls come up */
		switch (GOLEM_STATE2)
		{
		case GOLEM_SENKEN:
			if (GOLEM_WACKEL > FX(-0.2)) GOLEM_WACKEL -= SYNC(FX(0.03));
			else GOLEM_WACKEL = FX(-0.2);
			GOLEM_ROT1 -= SYNC(FX(0.25));
			GOLEM_ROT2 += SYNC(FX(0.25));
			if (GOLEM_ROT2 > FX(1))
			{
				GOLEM_STATE2 = GOLEM_HEBEN;
				shake_screen(3);
				snd(SOUND_DOORSTOP);
				gegner_push(cam_xoff() + FX(100) + FX(GetRandom(540)), cam_yoff() + FX(RENDERHEIGHT), LAVABALL,
				            50 + GetRandom(10), 0);
			}
			break;
		case GOLEM_HEBEN:
			if (GOLEM_WACKEL > FX(-0.2)) GOLEM_WACKEL -= SYNC(FX(0.03));
			else GOLEM_WACKEL = FX(-0.2);
			GOLEM_ROT1 += SYNC(FX(0.25));
			GOLEM_ROT2 -= SYNC(FX(0.25));
			if (GOLEM_ROT1 > FX(1))
			{
				GOLEM_STATE2 = GOLEM_SENKEN;
				shake_screen(3);
				snd(SOUND_DOORSTOP);
				gegner_push(pl.xpos - FX(20) + FX(GetRandom(70)), cam_yoff() + FX(RENDERHEIGHT), LAVABALL,
				            50 + GetRandom(10), 0);
				GOLEM_STONES--;
				if (GOLEM_STONES <= 0) e->Handlung = GEGNER_SPECIAL;
			}
			break;
		}
		break;

	case GEGNER_SPECIAL:           /* back to the start position, then the next action */
	{
		golem_wackeln(e);
		const int f0 = golem_go_to_zero(&GOLEM_WACKEL, FX(0.1));
		const int f1 = golem_go_to_zero(&GOLEM_ROT1, FX(0.1));
		const int f2 = golem_go_to_zero(&GOLEM_ROT2, FX(0.1));
		if (f0 && f1 && f2)
		{
			if (e->xPos > sx + FX(400))           /* on the right */
			{
				if (GetRandom(2) == 0)               /* throw boulders */
				{
					e->Handlung = GEGNER_CRUSHEN;
					GOLEM_LAST = GEGNER_CRUSHEN;
					GOLEM_STATE2 = GOLEM_SENKEN;
					GOLEM_STONES = GetRandom(2) + 2;
				}
				else                                 /* or beat the lava */
				{
					e->Handlung = GEGNER_BOMBARDIEREN;
					GOLEM_STATE2 = GOLEM_HEBEN;
					GOLEM_COUNT = FX(GetRandom(3) + 2);
				}
			}
			else if (e->xPos > sx + FX(120))      /* in the middle */
			{
				if (GetRandom(3) > 0)
					e->Handlung = GOLEM_LAST == GEGNER_SPECIAL2 ? GEGNER_LAUFEN2 : GEGNER_LAUFEN3;
				else
				{
					e->Handlung = GEGNER_AUSSPUCKEN;
					GOLEM_SHOT = FX(80);
					GOLEM_COUNT = FX(4 + GetRandom(4));
					GOLEM_WACKEL = 0;
				}
			}
			else e->Handlung = GEGNER_LAUFEN4;     /* on the left: back to the middle */
		}
	}
	break;

	case GEGNER_AUSSPUCKEN:        /* homing fire balls */
		if (GOLEM_SHOT > FX(10))
		{
			GOLEM_WACKEL += SYNC(FX(0.03));
			GOLEM_ROT1 -= SYNC(FX(0.054));
			GOLEM_ROT2 -= SYNC(FX(0.054));
		}
		else
		{
			GOLEM_WACKEL -= SYNC(FX(0.2));
			GOLEM_ROT1 += SYNC(FX(0.4));
			GOLEM_ROT2 += SYNC(FX(0.4));
		}
		GOLEM_SHOT -= SYNC(FX(10));
		if (GOLEM_SHOT < 0)
		{
			proj_winkel = FX(golem_winkel_to_player(e) + 180);
			proj_push(e->xPos + FX(35), e->yPos + GOLEM_YOFF + FX(35), FIREBALL_BIG);
			part_push(e->xPos - FX(60), e->yPos + GOLEM_YOFF - FX(70), GRENADEFLARE);
			snd(SOUND_FIREBALL);
			GOLEM_SHOT = FX(80);
			GOLEM_COUNT -= FX(1);
			if (GOLEM_COUNT <= 0)
			{
				e->Handlung = GEGNER_WARTEN;
				GOLEM_SHOT = FX(10);
			}
			GOLEM_WACKEL = 0;
			GOLEM_ROT1 = 0;
			GOLEM_ROT2 = 0;
		}
		break;

	case GEGNER_WARTEN:            /* a short wait after shooting */
		GOLEM_SHOT -= SYNC(FX(1));
		if (GOLEM_SHOT <= 0) e->Handlung = GEGNER_SPECIAL;
		break;

	case GEGNER_CRUSHEN:           /* fetch a boulder and throw it */
		switch (GOLEM_STATE2)
		{
		case GOLEM_SENKEN:             /* down for a boulder */
			GOLEM_WACKEL -= SYNC(FX(0.05));
			GOLEM_ROT1 += SYNC(FX(0.1));
			GOLEM_YOFF += SYNC(FX(5));
			if (GOLEM_WACKEL < FX(-0.5)) GOLEM_STATE2 = GOLEM_HEBEN;
			break;
		case GOLEM_HEBEN:              /* up again */
			GOLEM_YOFF -= SYNC(FX(5));
			GOLEM_WACKEL += SYNC(FX(0.05));
			GOLEM_ROT1 -= SYNC(FX(0.1));
			if (GOLEM_WACKEL >= 0)
			{
				GOLEM_STATE2 = GOLEM_UEBERKOPF;
				GOLEM_WACKEL = 0;
				GOLEM_ROT1 = 0;
				GOLEM_YOFF = 0;
			}
			break;
		case GOLEM_UEBERKOPF:          /* the boulder back over the head */
			GOLEM_WACKEL += SYNC(FX(0.04));
			GOLEM_ROT1 += SYNC(FX(0.4));
			if (GOLEM_WACKEL >= FX(0.35))
			{
				GOLEM_STATE2 = GOLEM_WERFEN;
				snd(SOUND_MUTANT);
			}
			break;
		case GOLEM_WERFEN:             /* throw */
			if (GOLEM_WACKEL > 0) GOLEM_WACKEL -= SYNC(FX(0.1));
			else GOLEM_WACKEL = 0;
			GOLEM_ROT1 -= SYNC(FX(0.6));
			if (GOLEM_ROT1 <= FX(2))
			{
				gegner_push(e->xPos - FX(40), e->yPos + FX(30), BOULDER, -(GetRandom(80) + 10), -(GetRandom(20) + 10));
				gegner_push(e->xPos - FX(40), e->yPos + FX(30), BOULDER, -(GetRandom(40) + 10), -(GetRandom(10) + 10));
				snd(SOUND_STONEFALL);
				GOLEM_STATE2 = GOLEM_SENKEN2;
			}
			break;
		case GOLEM_SENKEN2:            /* back to the throwing position */
		{
			golem_wackeln(e);
			const int f0 = golem_go_to_zero(&GOLEM_WACKEL, FX(0.5));
			const int f1 = golem_go_to_zero(&GOLEM_ROT1, FX(0.5));
			const int f2 = golem_go_to_zero(&GOLEM_ROT2, FX(0.5));
			if (f0 && f1 && f2)
			{
				GOLEM_STONES--;
				if (GOLEM_STONES == 0) e->Handlung = GEGNER_LAUFEN;
				else GOLEM_STATE2 = GOLEM_SENKEN;
			}
		}
		break;
		}
		break;

	case GEGNER_SCHIESSEN:         /* spit fire (not used by Hurrican's golem either) */
		GOLEM_COUNT += SYNC(FX(1));
		if (GOLEM_COUNT < FX(10)) GOLEM_WACKEL += SYNC(FX(1));
		else
		{
			if (GOLEM_SHOT == FX(3)) snd(SOUND_FEUERFALLE);
			GOLEM_WACKEL = FX(10);
			GOLEM_SHOT -= SYNC(FX(1));
		}
		if (GOLEM_SHOT <= 0)
		{
			GOLEM_SHOT = FX(0.5);
			proj_push(e->xPos - FX(10), e->yPos + FX(80), WALKERFIRE);
		}
		if (GOLEM_COUNT > FX(30)) e->Handlung = GEGNER_SPECIAL;
		break;

	case GEGNER_BOMBARDIEREN:      /* beat the lava: a fire column */
		switch (GOLEM_STATE2)
		{
		case GOLEM_HEBEN:              /* up fast */
			GOLEM_ROT1 += SYNC(FX(0.3));
			GOLEM_ROT2 += SYNC(FX(0.2));
			if (GOLEM_ROT1 > FX(3))
			{
				GOLEM_ROT1 = FX(3);
				GOLEM_STATE2 = GOLEM_SENKEN;
			}
			break;
		case GOLEM_SENKEN:             /* down fast */
			GOLEM_ROT1 -= SYNC(FX(0.8));
			GOLEM_ROT2 -= SYNC(FX(0.7));
			if (GOLEM_ROT1 < FX(0.7))         /* in the lava */
			{
				GOLEM_ROT1 = FX(0.7);
				GOLEM_ROT2 = FX(0.7);
				GOLEM_STATE2 = GOLEM_SENKEN2;
				for (int i = 0; i < 48; i++)
					part_push(e->xPos - FX(80) + FX(GetRandom(50)), e->yPos + FX(245), LAVA_SPRITZER2);
				snd(SOUND_WATERIN);
				proj_push(e->xPos - FX(90), e->yPos + FX(300), GOLEMSAEULE);
			}
			break;
		case GOLEM_SENKEN2:            /* spring back up a little */
			GOLEM_ROT1 += SYNC(FX(0.25));
			GOLEM_ROT2 += SYNC(FX(0.25));
			if (GOLEM_ROT1 > FX(1.5))
			{
				GOLEM_ROT1 = FX(1.5);
				GOLEM_ROT2 = FX(1.5);
				GOLEM_COUNT -= FX(1);
				if (GOLEM_COUNT < 0) e->Handlung = GEGNER_SPECIAL;
				else GOLEM_STATE2 = GOLEM_HEBEN;
			}
			break;
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = FX(100);
		e->AnimCount -= SPEEDFACTOR;
		if (e->AnimCount <= 0)
		{
			e->AnimCount = FX(0.5);
			part_push(e->xPos + FX(GetRandom(160)), e->yPos + FX(GetRandom(300)) + GOLEM_YOFF, EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
			if (GetRandom(2) == 0)
				part_push(e->xPos + FX(GetRandom(160)), e->yPos + FX(GetRandom(300)) + GOLEM_YOFF, SMOKEBIG);
		}
		GOLEM_SHOT -= SYNC(FX(1));
		if (GOLEM_SHOT <= 0) e->Energy = 0;
		break;
	}

	if (e->Handlung != GEGNER_EXPLODIEREN) TestDamagePlayers(e, SYNC(FX(5)), 0);

	/* the collision box (Hurrican sets it in DoDraw) */
	GegnerRect[GOLEM].l = 42;
	GegnerRect[GOLEM].r = (int16_t)(138 + FINT(GOLEM_YOFF));
	GegnerRect[GOLEM].t = 29;
	GegnerRect[GOLEM].b = 300;
}

static void golem_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION2);
	for (i = 0; i < 100; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(200)), e->yPos + GOLEM_YOFF + FX(GetRandom(300)), ROCKSPLITTER);
	for (i = 0; i < 80; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(200)), e->yPos + GOLEM_YOFF + FX(GetRandom(300)), SMOKEBIG);
	for (i = 0; i < 40; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(200)), e->yPos + GOLEM_YOFF + FX(GetRandom(300)), EXPLOSION_MEDIUM2);
	for (i = 0; i < 10; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(200)), e->yPos + GOLEM_YOFF + FX(GetRandom(300)), EXPLOSION_BIG);
	shake_screen(4);
	pl.Score += 8000;
	/* a platform, so the player gets on */
	gegner_push(FX(6760), FX(2500), PLATTFORM, 6, 0);
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

static void golem_load(void)
{
	golem_arm[0] = pic("golemarm_hinten.png");
	golem_arm[1] = pic("golemarm_vorne.png");
}

TYPEDEF(GOLEM) = { golem_init, golem_ki, golem_explode, golem_draw, golem_load };
