/*
 * FAHRSTUHLBOSS - the boss of the elevator stage (bosses/Boss_FahrstuhlBoss.cpp): rides down
 * above the lift (g_Fahrstuhl_yPos, set by the lift FAHRSTUHL), comes down into the screen,
 * shoots with its two moving cannons (lasers, homing shots), aims its big laser at the player,
 * shakes boulders loose, calls stilt walkers; hit only at its head. Out of energy it bursts and
 * flies off upwards. While it rides, the shots, the particles and the player in the air move
 * with the lift, and the level repeats: every 2 * 24 tiles everything is set back up (the
 * screen, the player, the objects, shots and particles).
 * Parts (DoDraw): the frame (fahrstuhlboss_rahmen.png), the head, the two cannons.
 * Own members: the static struct below (one at a time); i[0] = Value1 (the screen's y when it
 * started + 100, Hurrican keeps it in Value1).
 * Left out: the music fades (stage music stops when it comes, boss music at once, the stage
 * music again when it bursts), screen shaking.
 * Engine notes: moving all shots / particles uses proj.c's pj[] and part.c's parts[] (no
 * function in hur.h does it; left out if those files do not define PJ_MAX / PART_MAX); the
 * repeat moves the screen with p1.c's hur_yoff, and also g_Fahrstuhl_yPos (so the player on
 * the lift does not fall back to the old place before the lift runs again); the screen goes
 * back to the player with p1.c's scroll_level_speed (Hurrican: speed 10, 50).
 *
 * PC check (not on the 32X): HB_B3_LIFT=1 - when there is no FAHRSTUHL type, the boss itself
 * plays the lift: g_Fahrstuhl_yPos goes down at 40 under the player, who is held on it, and
 * the screen follows it as the lift's does.
 */
static struct {
	int Shots, Activated;
	fx ShotDelay, TempY, x1, y1, x2, y2, x3, dx1, dx2, CanonAnim;
	fx smokecount;
} fsb;
static Pic fahrstuhlboss_head, fahrstuhlboss_canon;

static void fahrstuhlboss_load(void)
{
	fahrstuhlboss_head = pic("fahrstuhlboss_head.png");
	fahrstuhlboss_canon = pic("fahrstuhlboss_canon.png");
}

static void fahrstuhlboss_init(Ent *e)
{
	memset(&fsb, 0, sizeof fsb);
	fsb.ShotDelay = FX(5);
	fsb.TempY = FX(800);
	fsb.x1 = FX(140);
	fsb.y1 = FX(160);
	fsb.x2 = FX(25);
	fsb.y2 = FX(80);
	fsb.x3 = FX(340);
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(6000);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->yPos = 0;
}

static void fahrstuhlboss_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
	draw_sprite(&fahrstuhlboss_head, 0, e->xPos + fsb.x1, e->yPos + fsb.y1, 0);
	draw_sprite(&fahrstuhlboss_canon, FINT(fsb.CanonAnim), e->xPos + fsb.x2, e->yPos + fsb.y2, 0);
	draw_sprite(&fahrstuhlboss_canon, 5 - FINT(fsb.CanonAnim), e->xPos + fsb.x3, e->yPos + fsb.y2, 0);
	GegnerRect[FAHRSTUHLBOSS].l = (int16_t)FINT(fsb.x1 + FX(60));
	GegnerRect[FAHRSTUHLBOSS].r = (int16_t)FINT(fsb.x1 + FX(60) + FX(25));
	GegnerRect[FAHRSTUHLBOSS].t = (int16_t)FINT(fsb.y1 + FX(46));
	GegnerRect[FAHRSTUHLBOSS].b = (int16_t)FINT(fsb.y1 + FX(46) + FX(34));
}

/* the shots and particles moved by dy (all = 0: those Hurrican moves with the lift) */
static void fahrstuhlboss_shift(fx dy, int all)
{
#ifdef PJ_MAX
	for (int i = 0; i < PJ_MAX; i++)
		if (pj[i].ShotArt >= 0 && (all || pj[i].ShotArt != STELZLASER))
		{
			pj[i].yPos += dy;
			pj[i].yPosOld += dy;
		}
#endif
#ifdef PART_MAX
	for (int i = 0; i < PART_MAX; i++)
		if (parts[i].live &&
		    (all || (parts[i].PartikelArt != FUNKE && parts[i].PartikelArt != BULLET && parts[i].PartikelArt != SMOKE)))
		{
			parts[i].yPos += dy;
			parts[i].yPosOld += dy;
		}
#endif
	(void)dy; (void)all;
}

#ifndef OJ32X_MARS
static void fahrstuhlboss_test_lift(void)
{
	if (!getenv("HB_B3_LIFT") || gegner_known(FAHRSTUHL)) return;
	if (g_Fahrstuhl_yPos <= 0)
	{
		g_Fahrstuhl_yPos = pl.ypos + FX(pl.CollideRect.b) + FX(1);
		g_Fahrstuhl_Offset = FX(70);
	}
	g_Fahrstuhl_Speed = FX(40);
	g_Fahrstuhl_yPos += SYNC(g_Fahrstuhl_Speed);
	pl.ypos = pl.yposold = g_Fahrstuhl_yPos - FX(pl.CollideRect.b) - FX(1);
	pl.yspeed = 0;
	pl.JumpStart = pl.ypos;
	hur_xoff = FX(390 + 200 - 320);
	hur_yoff = g_Fahrstuhl_yPos - FX(320) - g_Fahrstuhl_Offset;
	if (tile_zustand() != TS_LOCKED) scroll_level(hur_xoff, hur_yoff, TS_LOCKED);
}
#endif

static void fahrstuhlboss_ki(Ent *e)
{
#ifndef OJ32X_MARS
	fahrstuhlboss_test_lift();
#endif
	if (fsb.x1 < 0) fsb.x1 = 0;

	/* the energy bar */
	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN) boss_bar(e->Energy, FX(6000));

	/* the boss comes: music */
	if (e->Active && !fsb.Activated)
	{
		music_stop();                       /* FadeSong(STAGEMUSIC, -2, 0, pause) */
		fsb.Activated = 1;
		e->i[0] = FINT(cam_yoff()) + 100;
	}

	/* a boss does not flash as long as the others */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(50));
	else e->DamageTaken = 0;

	/* no energy: it bursts */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		fsb.ShotDelay = FX(1);
		g_Fahrstuhl_Speed = FX(-1);
		fsb.dx1 = 0;
		fsb.dx2 = 0;
		/* the screen back to the player */
		scroll_level_speed(pl.xpos - FX(300), pl.ypos - FX(280), TS_SCROLLTOPLAYER, FX(10), FX(50));
		music_stop();                       /* FadeSong(BOSS, -2, 0) */
	}

	/* the feet at the sides smoke */
	fsb.smokecount -= SYNC(FX(1));
	if (fsb.smokecount < 0)
	{
		fsb.smokecount = FX(1);
		part_push(e->xPos - FX(15) + FX(GetRandom(10)), e->yPos, SMOKEBIG);
		part_push(e->xPos - FX(15) + FX(GetRandom(10)), e->yPos + FX(210), SMOKEBIG);
		part_push(e->xPos + FX(375) + FX(GetRandom(10)), e->yPos, SMOKEBIG);
		part_push(e->xPos + FX(375) + FX(GetRandom(10)), e->yPos + FX(210), SMOKEBIG);
		/* the sore point smokes when it is low */
		if (e->Energy <= FX(1500))
		{
			part_push(e->xPos + fsb.x1 + FX(35) + FX(GetRandom(20)), e->yPos + fsb.y1 + FX(GetRandom(15)), SMOKEBIG);
			if (GetRandom(10) == 0)
				part_push(e->xPos + fsb.x1 + FX(50) + FX(GetRandom(20)), e->yPos + fsb.y1 + FX(35) + FX(GetRandom(15)), LASERFLAME);
		}
	}

	/* the cannons move */
	fsb.x1 += SYNC(fsb.dx1);
	fsb.x2 += SYNC(fsb.dx2);
	fsb.x3 -= SYNC(fsb.dx2);
	fsb.CanonAnim += SYNC(fsb.dx2);
	if (fsb.CanonAnim >= FX(6)) fsb.CanonAnim = 0;
	if (fsb.CanonAnim < 0) fsb.CanonAnim = FX(6);
	if (fsb.x2 > FX(340)) { fsb.x2 = FX(25); fsb.x3 = FX(340); }

	/* the big gun back to the middle */
	if (e->Handlung == GEGNER_EINFLIEGEN || e->Handlung == GEGNER_ABSENKEN || e->Handlung == GEGNER_AUFRICHTEN)
	{
		if ((fsb.dx1 < 0 && fsb.x1 < FX(140)) || (fsb.dx1 > 0 && fsb.x1 > FX(140)))
		{
			fsb.x1 = FX(140);
			fsb.dx1 = 0;
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:                 /* the stage music is off: the boss comes */
		music_boss();
		e->Handlung = GEGNER_EINFLIEGEN2;
		break;

	case GEGNER_EINFLIEGEN2:                /* comes into the screen (once, at the start) */
		e->Energy = FX(6000);
		e->DamageTaken = 0;
		fsb.TempY -= SYNC(FX(5));
		if (fsb.TempY < FX(500))
		{
			e->Handlung = GEGNER_ABSENKEN;
			fsb.TempY = FX(500);
			fsb.dx2 = FX(5);
		}
		/* the screen further down to show more of the boss */
		if (g_Fahrstuhl_Offset < FX(150)) g_Fahrstuhl_Offset += SYNC(FX(4));
		else g_Fahrstuhl_Offset = FX(150);
		break;

	case GEGNER_EINFLIEGEN:                 /* comes into the screen */
		fsb.TempY -= SYNC(FX(5));
		if (fsb.TempY < FX(500))
		{
			fsb.TempY = FX(500);
			switch (GetRandom(6))
			{
			case 0: e->Handlung = GEGNER_ABSENKEN; fsb.dx2 = 0; break;
			case 1:
			case 2: e->Handlung = GEGNER_ABSENKEN; fsb.dx2 = FX(5); break;
			case 3: e->Handlung = GEGNER_AUSSPUCKEN; fsb.Shots = 1 + GetRandom(2); break;
			case 4:
			case 5: e->Handlung = GEGNER_CRUSHEN; fsb.Shots = 10 + GetRandom(20); fsb.ShotDelay = FX(2); break;
			}
		}
		if (g_Fahrstuhl_Offset < FX(150)) g_Fahrstuhl_Offset += SYNC(FX(4));
		else g_Fahrstuhl_Offset = FX(150);
		break;

	case GEGNER_ABSENKEN:                   /* comes down */
		fsb.TempY -= SYNC(FX(5));
		if (fsb.TempY < FX(380))
		{
			fsb.TempY = FX(380);
			if (fsb.dx2 != 0)
			{
				e->Handlung = GEGNER_SCHIESSEN;
				fsb.ShotDelay = FX(1);
				fsb.Shots = GetRandom(10) + 10;
			}
			else
			{
				e->Handlung = GEGNER_AUSSPUCKEN;
				fsb.ShotDelay = FX(5);
				fsb.Shots = 2 + GetRandom(2);
			}
		}
		break;

	case GEGNER_AUFRICHTEN:                 /* goes up again */
		fsb.TempY += SYNC(FX(5));
		if (fsb.TempY > FX(600))
		{
			fsb.TempY = FX(600);
			switch (GetRandom(3))
			{
			case 0: e->Handlung = GEGNER_BOMBARDIEREN; fsb.dx1 = FX(40); fsb.ShotDelay = FX(1); fsb.Shots = GetRandom(8) + 5; break;
			case 1: e->Handlung = GEGNER_SPECIAL; fsb.dx1 = 0; fsb.ShotDelay = FX(5); fsb.Shots = GetRandom(8) + 5; break;
			case 2: e->Handlung = GEGNER_AUSSPUCKEN; fsb.dx1 = 0; fsb.ShotDelay = FX(5); fsb.Shots = GetRandom(4) + 2; break;
			}
		}
		break;

	case GEGNER_SCHIESSEN:                  /* two lasers */
		fsb.ShotDelay -= SYNC(FX(1));
		if (fsb.ShotDelay < 0)
		{
			fsb.ShotDelay = FX(6);
			snd(SOUND_LASERSHOT);
			proj_push(e->xPos + fsb.x2 + FX(28), e->yPos + fsb.y2 + FX(95), KRABBLERLASER1);
			part_push(e->xPos + fsb.x2 + FX(28 - 25), e->yPos + fsb.y2 + FX(95 - 4), LASERFLAME);
			proj_push(e->xPos + fsb.x3 + FX(28), e->yPos + fsb.y2 + FX(95), KRABBLERLASER1);
			part_push(e->xPos + fsb.x3 + FX(28 - 25), e->yPos + fsb.y2 + FX(95 - 4), LASERFLAME);
			fsb.y2 = FX(75);
			if (--fsb.Shots <= 0)
			{
				fsb.dx2 = 0;
				e->Handlung = GEGNER_AUFRICHTEN;
			}
		}
		break;

	case GEGNER_BOMBARDIEREN:               /* shakes boulders loose */
		if ((fsb.x1 > FX(280) && fsb.dx1 > 0) || (fsb.x1 < 0 && fsb.dx1 < 0))
		{
			shake_screen(3);
			fsb.dx1 = -fsb.dx1;
			gegner_push(e->xPos + FX(10) + FX(GetRandom(360)), e->yPos + FX(RENDERHEIGHT), BOULDER, 40 + GetRandom(20), 0);
			if (--fsb.Shots <= 0)
			{
				e->Handlung = GEGNER_EINFLIEGEN;
				fsb.dx1 = fsb.x1 < FX(140) ? FX(10) : FX(-10);
			}
		}
		break;

	case GEGNER_SPECIAL:                    /* aims at the player and fires the big laser */
		if (fsb.ShotDelay > 0) fsb.ShotDelay -= SYNC(FX(1));
		if (pl.xpos + FX(35) < e->xPos + fsb.x1 + FX(60)) fsb.x1 -= SYNC(FX(8));
		if (pl.xpos + FX(35) > e->xPos + fsb.x1 + FX(80)) fsb.x1 += SYNC(FX(8));
		if (pl.xpos + FX(35) < e->xPos + fsb.x1 + FX(100) && pl.xpos + FX(35) > e->xPos + fsb.x1 + FX(40) && fsb.ShotDelay <= 0)
		{
			fsb.ShotDelay = FX(8);
			snd(SOUND_LASERSHOT);
			proj_push(e->xPos + fsb.x1 + FX(80 - 28), e->yPos + fsb.y1 + FX(155), UFOLASER);
			part_push(e->xPos + fsb.x1 + FX(80 - 90), e->yPos + fsb.y1 + FX(110), UFOLASERFLARE);
			fsb.y1 = FX(155);
			if (--fsb.Shots <= 0)
			{
				e->Handlung = GEGNER_EINFLIEGEN;
				fsb.dx1 = fsb.x1 < FX(140) ? FX(10) : FX(-10);
			}
		}
		break;

	case GEGNER_AUSSPUCKEN:                 /* calls stilt walkers */
		fsb.ShotDelay -= SYNC(FX(1));
		if (fsb.ShotDelay < 0)
		{
			fsb.ShotDelay = FX(35);
			gegner_push(e->xPos + FX(10) + FX(GetRandom(360)), e->yPos + FX(RENDERHEIGHT), STELZSACK, 80, 0);
			if (--fsb.Shots <= 0)
			{
				fsb.dx2 = 0;
				e->Handlung = fsb.TempY != FX(380) ? GEGNER_EINFLIEGEN : GEGNER_AUFRICHTEN;
				fsb.dx1 = fsb.x1 < FX(140) ? FX(10) : FX(-10);
			}
		}
		break;

	case GEGNER_CRUSHEN:                    /* homing shots */
		fsb.ShotDelay -= SYNC(FX(1));
		if (fsb.ShotDelay < 0)
		{
			fsb.ShotDelay = FX(5);
			snd(SOUND_CANON);
			proj_push(e->xPos + fsb.x2 + FX(28), e->yPos + fsb.y2 + FX(95 + 5), SUCHSCHUSS);
			part_push(e->xPos + fsb.x2 + FX(28 - 10), e->yPos + fsb.y2 + FX(95), SMOKE);
			proj_push(e->xPos + fsb.x3 + FX(28), e->yPos + fsb.y2 + FX(95 + 5), SUCHSCHUSS);
			part_push(e->xPos + fsb.x3 + FX(28 - 10), e->yPos + fsb.y2 + FX(95), SMOKE);
			fsb.y2 = FX(77);
			if (--fsb.Shots <= 0)
			{
				fsb.dx2 = FX(5);
				e->Handlung = GEGNER_ABSENKEN;
				fsb.dx1 = fsb.x1 < FX(140) ? FX(10) : FX(-10);
			}
		}
		break;

	case GEGNER_EXPLODIEREN:                /* bursts and flies off */
		fsb.ShotDelay -= SYNC(FX(1));
		if (fsb.ShotDelay < 0)
		{
			fsb.ShotDelay = FX(0.5);
			snd(SOUND_EXPLOSION1);
			part_push(e->xPos - FX(30) + FX(GetRandom(400)), e->yPos + FX(GetRandom(280)), EXPLOSION_MEDIUM2);
			part_push(e->xPos - FX(50) + FX(GetRandom(400)), e->yPos + FX(GetRandom(280)), EXPLOSION_BIG);
			if (GetRandom(2) == 0) part_push(e->xPos + FX(GetRandom(400)), e->yPos + FX(GetRandom(280)), SPLITTER);
		}
		e->Energy = FX(100);
		/* the screen up again */
		if (g_Fahrstuhl_Offset > FX(20)) g_Fahrstuhl_Offset -= SYNC(FX(4));
		else g_Fahrstuhl_Offset = FX(20);
		fsb.TempY -= SYNC(FX(10));
		if (fsb.TempY < FX(-400)) e->Energy = 0;   /* out of the screen: gone */
		break;

	default:
		break;
	}

	/* its place above the lift */
	e->yPos = g_Fahrstuhl_yPos - fsb.TempY;
	if (e->yPos < 0) e->yPos = 0;

	/* the parts back in place */
	if (fsb.y2 < FX(80)) fsb.y2 += SYNC(FX(2));
	else fsb.y2 = FX(80);
	if (fsb.y1 < FX(160)) fsb.y1 += SYNC(FX(2));
	else fsb.y1 = FX(160);

	/* the player in the air, the shots and particles go down with the lift */
	if (pl.AufPlattform == 0)
	{
		pl.ypos += SYNC(FX(40));
		pl.yposold += SYNC(FX(40));
		pl.JumpStart += SYNC(FX(40));
	}
	fahrstuhlboss_shift(SYNC(FX(40)), 0);

	/* the level repeats: everything back up */
	enum { A = 2 * 24 * 20 };
	if (e->yPos > FX(e->i[0] + A))
	{
		hur_yoff -= FX(A);
		pl.ypos -= FX(A);
		for (int k = 0; k < MAX_LIVE; k++)
		{
			Ent *o = &ents[k];
			if (o->GegnerArt >= SPITTER && o->GegnerArt <= FAHRSTUHL)
			{
				o->yPos -= FX(A);
				o->yPosOld -= FX(A);
			}
		}
		g_Fahrstuhl_yPos -= FX(A);
		fahrstuhlboss_shift(FX(-A), 1);
	}
}

static void fahrstuhlboss_explode(Ent *e)
{
	shake_screen(10);
	snd(SOUND_EXPLOSION2);
	pl.Score += 9000;
	music_stage();                          /* the stage music again */
	gegner_default_explode(e);
}

TYPEDEF(FAHRSTUHLBOSS) = { fahrstuhlboss_init, fahrstuhlboss_ki, fahrstuhlboss_explode, fahrstuhlboss_draw, fahrstuhlboss_load };
