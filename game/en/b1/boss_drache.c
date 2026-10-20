/*
 * DRACHE - the giant steel dragon (bosses/Boss_Drache.cpp).
 *  Value2 == 0 (top of the tower level): the boss. The screen locks, a small dragon flies past in
 *   the background (CDragonHack), then the dragon flies over the player spitting fire or laying
 *   mini dragons (MINIDRAGON), races past dropping rockets (FETTERAKETE) with a wind pushing the
 *   player, or comes in from the side and shoots its head out on its neck. Shot down it falls,
 *   lies on the floor and waits for the player to jump on; then it flies off to the left and
 *   the stage is clear.
 *  Value2 != 0 (start of the ice level): the player stands on its back while it flies, crashes
 *   and slides; then the stage goes on (the stage music plays again).
 * Own members: the static struct drache_s (one dragon boss at a time): Attack, mirrorOffset,
 *  ShotCount, Position, SmokeCount, TailSinus, HeadWinkel (degrees), AnimWinkel, KieferWinkel
 *  (radians), HeadX, HeadY, HeadXSpeed, HeadYSpeed, ShotDelay, StartPosY, DrawYOffset,
 *  ArrowCount, HeadLocked, the background dragon's slot; e->AnimCount as Hurrican's.
 *  Function statics: drache_off (LAUFEN's "off"), drache_vorbei (EINFLIEGEN2's "drachevorbei").
 * CDragonHack (TileEngine's small dragon in the background, made when the level loads a dragon
 *  boss): here a second DRACHE object pushed by the boss's constructor with Value2 =
 *  DRACHE_HACK (BackGround, not hit, not moved by the base Run), running CDragonHack::Run and
 *  drawing drache_klein.png; the boss reads its state and removes it as Hurrican deletes it.
 * Hurrican's DoDraw sets the facing, mirrorOffset, the rectangle (hit and stand-on) and moves
 *  the arrow counter; DoKI uses them: here that part runs at the end of DoKI under DoDraw's
 *  conditions (on the screen, the player nearer than 1000).
 * Drawing: no rotation (legs, arms, head, jaw upright), the tail segments unscaled (Hurrican
 *  scales them from 12 x 29 down), the flares (LavaFlare, additive glows) left out, the arrow
 *  (hudarrow.png) without its green tint. Left out: the sound pitches, ShakeScreen, the music
 *  fades (stopped / started at once).
 */
#define DRACHE_HACK      (-77)          /* Value2 of the background dragon */
#define DRACHE_NUM_TAILS 20
#define DRACHE_PI        FX(3.14159265)
#define DRACHE_HALF_PI   FX(1.57079633)
#define DRACHE_TWO_PI    FX(6.28318531)

static struct {
	int Attack, mirrorOffset, ShotCount, Position, HeadLocked, hack, music_off;
	fx SmokeCount, TailSinus, HeadWinkel, AnimWinkel, KieferWinkel, HeadX, HeadY, HeadXSpeed, HeadYSpeed;
	fx ShotDelay, StartPosY, DrawYOffset, ArrowCount;
} drache_s;
static fx drache_off;
static int drache_vorbei;
static Pic drache_Arm, drache_Leg, drache_Head0, drache_Head1, drache_Tail, drache_Spitze, drache_Klein, drache_Arrow;

/* sin of radians (fx), interpolated between whole degrees */
static fx drache_sinr(fx rad)
{
	const fx deg = FMUL(rad, FX(57.2957795));
	const int d = deg >> 8, frac = deg & 255;
	const int a = sin_d(d), b = sin_d(d + 1);
	return a + (((b - a) * frac) >> 8);
}

/* atan(y / x) in degrees (fx), -90..90 */
static fx drache_atan_q(int y, int x)
{
	if (x == 0) return y > 0 ? FX(90) : y < 0 ? FX(-90) : 0;
	int a = atan2_d(y, x);
	if (a > 90) a -= 180;
	if (a < -90) a += 180;
	return FX(a);
}

static void drache_load(void)
{
	drache_Arm = pic("drache_arm.png");
	drache_Leg = pic("drache_bein.png");
	drache_Head0 = pic("drache_head.png");
	drache_Head1 = pic("drache_head2.png");
	drache_Tail = pic("drache_tail.png");
	drache_Spitze = pic("drache_spitze.png");
	drache_Klein = pic("drache_klein.png");
	drache_Arrow = pic("hudarrow.png");
}

/* ---- CDragonHack: the small dragon in the background
 * own members: i[0] = m_State (0 WAIT, 1 FLY), f[0] = m_AppearCount, AnimCount = m_AnimCount,
 * xPos / yPos / xSpeed / ySpeed = m_xPos ... */
static void drache_hack_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(1);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->DontMove = 1;
	e->BackGround = 1;
	e->Active = 1;
	e->i[0] = 0;
	e->f[0] = FX(5.0);
}

static void drache_hack_ki(Ent *e)
{
	e->AnimCount += SYNC(FX(3.0));
	if (e->AnimCount >= FX(4.0)) e->AnimCount = 0;
	switch (e->i[0])
	{
	case 0:                                /* not to be seen */
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1.0));
		else
		{
			e->f[0] = FX(20.0);
			e->i[0] = 1;
			if (GetRandom(2) == 0)
			{
				e->xPos = cam_xoff() - FX(160);
				e->yPos = cam_yoff() + FX(200);
				e->xSpeed = FX(10.0);
			}
			else
			{
				e->xPos = cam_xoff() + FX(RENDERWIDTH + 160);
				e->yPos = cam_yoff() + FX(200);
				e->xSpeed = FX(-10.0);
			}
			e->ySpeed = FX(GetRandom(9) - 4) / 5;
		}
		break;
	case 1:                                /* flies until out of the picture */
		e->xPos += SYNC(e->xSpeed);
		e->yPos += SYNC(e->ySpeed);
		if (e->xPos + FX(260) < cam_xoff() || e->xPos - FX(260) > cam_xoff() + FX(RENDERWIDTH) ||
		    e->yPos + FX(240) < cam_yoff() || e->yPos - FX(240) > cam_yoff() + FX(RENDERHEIGHT))
		{
			e->i[0] = 0;
			e->f[0] = FX(GetRandom(100) + 50);
		}
		break;
	}
}

static void drache_hack_draw(Ent *e)
{
	if (e->i[0] == 1) draw_sprite(&drache_Klein, FINT(e->AnimCount), e->xPos, e->yPos, e->xSpeed > 0);
}

static Ent *drache_hack(void)
{
	const int k = drache_s.hack;
	if (k < 0 || k >= MAX_LIVE || ents[k].GegnerArt != DRACHE || ents[k].Value2 != DRACHE_HACK) return 0;
	return &ents[k];
}

/* ---- the dragon */
static void drache_init(Ent *e)
{
	if (e->Value2 == DRACHE_HACK) { drache_hack_init(e); return; }
	memset(&drache_s, 0, sizeof drache_s);
	drache_s.Position = -1;
	drache_s.hack = -1;
	drache_off = 0;
	drache_vorbei = 0;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(7000);
	e->AnimPhase = 0;
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;

	/* Value2 != 0: the player starts on the dragon's back */
	if (e->Value2 != 0)
	{
		e->Handlung = GEGNER_EINFLIEGEN;
		drache_s.Attack = GEGNER_LAUFEN;
		e->xSpeed = FX(-40.0);
		/* (Hurrican puts the player at the dragon here too: done by DoKI from the first step) */
		pl.AufPlattform = e;
		pl.DoFesteAktion = 1;
		pl.FesteAktion = -1;
		e->Active = 1;
		e->Destroyable = 0;
		drache_s.music_off = 1;          /* StopStageMusicAtStart */
		drache_s.HeadLocked = 1;
	}
	else
	{
		/* TileEngine's background dragon */
		Ent *h = gegner_push(0, 0, DRACHE, 0, DRACHE_HACK);
		if (h->GegnerArt == DRACHE) drache_s.hack = (int)(h - ents);
	}
}

/* the head's angle to the player */
static void drache_ComputeHeadWinkel(Ent *e)
{
	if (drache_s.HeadLocked) return;
	int ydiv = FINT(pl.ypos + FX(40) - e->yPos);
	if (ydiv == 0) ydiv = 1;
	if (e->blick == 1)
	{
		const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(210)));
		drache_s.HeadWinkel = drache_atan_q(xdiv, ydiv) - FX(90);
	}
	else
	{
		const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos - FX(80)));
		drache_s.HeadWinkel = -drache_atan_q(xdiv, ydiv) - FX(82);
	}
	if (drache_s.HeadWinkel < FX(-90)) drache_s.HeadWinkel = FX(-90);
	if (drache_s.HeadWinkel > FX(90)) drache_s.HeadWinkel = FX(90);
}

/* the part of Hurrican's DoDraw that sets the state (facing, rectangle, arrow) */
static void drache_draw_state(Ent *e)
{
	Box *r = &GegnerRect[DRACHE];
	int mirrored;
	if (e->xSpeed < 0) { mirrored = -1; drache_s.mirrorOffset = -1; r->l = -50; r->r = 161; }
	else if (e->xSpeed > 0) { mirrored = 1; drache_s.mirrorOffset = 0; r->l = 40; r->r = 250; }
	else if (drache_s.Position == -1) { mirrored = -1; drache_s.mirrorOffset = -1; r->l = -50; r->r = 161; }
	else { mirrored = 1; drache_s.mirrorOffset = 0; r->l = 40; r->r = 250; }
	if (!e->Destroyable)
	{
		drache_s.Position = -1;
		e->blick = -1;
		mirrored = -1;
		drache_s.mirrorOffset = -1;
	}
	if (e->Handlung == GEGNER_LAUFEN3)          /* the head can be hit */
	{
		r->t = (int16_t)(20 + FINT(drache_s.HeadY));
		r->b = (int16_t)(130 + FINT(drache_s.HeadY));
		if (drache_s.Position == -1) { r->l = (int16_t)(FINT(drache_s.HeadX) - 60); r->r = (int16_t)(FINT(drache_s.HeadX) + 20); }
		else { r->l = (int16_t)(180 + FINT(drache_s.HeadX)); r->r = (int16_t)(270 + FINT(drache_s.HeadX)); }
	}
	else if (e->Handlung == GEGNER_WARTEN || e->Handlung == GEGNER_AUSFAHREN || e->Handlung == GEGNER_EINFLIEGEN)
		*r = (Box){ 50, 15, 150, 135 };        /* the player can stand on it */
	else { r->t = 0; r->b = 135; }
	e->blick = (int8_t)(mirrored < 0 ? -1 : 1);
	if (e->Handlung == GEGNER_WARTEN)
	{
		drache_s.ArrowCount -= SYNC(FX(0.2));
		if (drache_s.ArrowCount < 0) drache_s.ArrowCount = FX(2.0);
	}
}

static void drache_draw(Ent *e)
{
	if (e->Value2 == DRACHE_HACK) { drache_hack_draw(e); return; }
	if (PlayerAbstand(e) > 1000) return;
	const int mo = drache_s.mirrorOffset;
	const int mir = e->blick == -1;
	const int mirrored = mir ? -1 : 1;
	const fx x = e->xPos, D = drache_s.DrawYOffset;
	const fx y = e->yPos + D;
	int i;

	/* the back leg and arm */
	draw_sprite(&drache_Leg, 0, x - FX(140) - FX(mo * 270), y + FX(95), mir);
	draw_sprite(&drache_Arm, 0, x + FX(140) + FX(mo * 210), y + FX(100), mir);

	/* the neck's vertebrae */
	if ((e->Handlung == GEGNER_LAUFEN3 || e->Handlung == GEGNER_EXPLODIEREN) && drache_s.Attack != GEGNER_EINFLIEGEN &&
	    drache_s.Attack != GEGNER_SCHIESSEN)
	{
		const int hx = FINT(drache_s.HeadX), hy = FINT(drache_s.HeadY);
		const int dist = isqrt(hx * hx + hy * hy);
		int render = dist / 10;
		if (dist > 0)
		{
			const fx xadd = FX(hx) * 10 / dist, yadd = FX(hy) * 10 / dist;
			fx xoff = 0, yoff = 0;
			while (render-- > 0)
			{
				draw_sprite(&drache_Tail, 0, x + FX(200) + FX(mo * 220) + xoff * drache_s.Position, y + FX(60) + yoff, 0);
				if (drache_s.Position == -1) xoff -= xadd;
				else xoff += xadd;
				yoff += yadd;
				if (xoff > drache_s.HeadX && yoff > drache_s.HeadY) render = 0;
			}
		}
	}

	/* the body, the knob on the neck */
	draw_sprite(&GegnerPic[DRACHE], 0, x, y, mir);
	draw_sprite(&GegnerPic[MINIDRAGON], 4, x + FX(170) + FX(mo * 210), y + FX(55), 0);

	/* the tail */
	fx tempx = x + FX(10) - FX(mo * 165);
	for (i = 0; i < DRACHE_NUM_TAILS; i++)
	{
		const fx s = drache_sinr(drache_s.TailSinus + FX(i) / 7);
		draw_sprite(&drache_Tail, 0, tempx, y + FX(20) + s * i + FX(42), 0);
		tempx -= FX(12 - i / 4) * mirrored;
	}
	{
		const fx s = drache_sinr(drache_s.TailSinus + FX(i) / 7);
		draw_sprite(&drache_Spitze, 0, tempx - FX(37) - FX(mo * 37), y + FX(17) + s * i + FX(36), mir);
	}

	/* the lower jaw, the head */
	draw_sprite(&drache_Head1, 0, x + FX(190) + FX(mo * 276) + drache_s.HeadX, y + FX(50) + drache_s.HeadY, mir);
	draw_sprite(&drache_Head0, 0, x + FX(180) + FX(mo * 276) + drache_s.HeadX, y + FX(15) + drache_s.HeadY, mir);

	/* the front leg and arm */
	draw_sprite(&drache_Leg, 0, x - FX(140) - FX(mo * 270), y + FX(85), mir);
	draw_sprite(&drache_Arm, 0, x + FX(140) + FX(mo * 210), y + FX(90), mir);

	/* the arrow: jump on */
	if (e->Handlung == GEGNER_WARTEN && FINT(drache_s.ArrowCount) % 2 == 0)
		draw_sprite_flipv(&drache_Arrow, 0, x + FX(50), e->yPos - FX(30), 0);
}

#ifndef OJ32X_MARS
/* PC check: HB_B1LOG=1 prints the state every half second */
static void drache_log(const Ent *e)
{
	static int n;
	if (getenv("HB_B1LOG") && n++ % (getenv("HB_B1LOGN") ? atoi(getenv("HB_B1LOGN")) : 30) == 0)
		fprintf(stderr, "drache: A %d head %d,%d hs %d,%d pos %d sc %d H %d E %d pos %d,%d speed %d,%d anim %d dest %d cam %d,%d state %d player %d,%d\n", drache_s.Attack, FINT(drache_s.HeadX), FINT(drache_s.HeadY),
		        (int)drache_s.HeadXSpeed, (int)drache_s.HeadYSpeed, drache_s.Position, drache_s.ShotCount, e->Handlung,
		        FINT(e->Energy), FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->AnimPhase, e->Destroyable,
		        FINT(cam_xoff()), FINT(cam_yoff()), tile_zustand(), FINT(pl.xpos), FINT(pl.ypos));
}
#else
#define drache_log(e) ((void)0)
#endif

static void drache_ki(Ent *e)
{
	int i;
	if (e->Value2 == DRACHE_HACK) { drache_hack_ki(e); return; }
	drache_log(e);

	if (drache_s.music_off) { drache_s.music_off = 0; music_stop(); }

	if (e->Value2 == 0 && e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(7000));

	/* lock the screen on the dragon as soon as it is seen */
	if (e->Value2 == 0 && e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), e->yPos, TS_SCROLLTOLOCK);
		e->xPos = FX(e->Value1 - 500);      /* out of the screen */
		music_stop();
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100.0));
	else e->DamageTaken = 0;

	/* no energy left: it explodes */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN && e->Destroyable)
	{
		pl.Score += 10000;
		e->Handlung = GEGNER_EXPLODIEREN;
		drache_s.Attack = GEGNER_LAUFEN;
		e->yAcc = FX(-1.0);
		e->AnimCount = FX(50.0);
		drache_s.ShotDelay = FX(1.0);
		e->Energy = FX(100);
		drache_s.HeadLocked = 1;
		for (int k = 0; k < MAX_LIVE; k++)
			if (ents[k].GegnerArt == MINIDRAGON) ents[k].Energy = 0;
		music_stop();
	}

	if (!(e->Handlung == GEGNER_EINFLIEGEN && drache_s.Attack == GEGNER_STEHEN)) drache_s.AnimWinkel += SYNC(FX(0.2));
	while (drache_s.AnimWinkel > DRACHE_TWO_PI) drache_s.AnimWinkel -= DRACHE_TWO_PI;

	drache_ComputeHeadWinkel(e);

	switch (e->Handlung)
	{
	case GEGNER_WARTEN:                        /* wait for the player to jump on */
	{
		if (drache_s.KieferWinkel > DRACHE_HALF_PI) drache_s.KieferWinkel -= SYNC(FX(5.0));
		else drache_s.KieferWinkel = DRACHE_HALF_PI;
		if (drache_s.HeadWinkel > FX(-50)) drache_s.HeadWinkel -= SYNC(FX(25.0));
		else drache_s.HeadWinkel = FX(-50);
		e->AnimCount -= FX(1.0);
		if (e->AnimCount < 0)
		{
			e->AnimCount = FX(5.0);
			part_push(e->xPos - FX(100) + FX(GetRandom(300)), e->yPos + drache_s.DrawYOffset + FX(GetRandom(100)), SMOKEBIG);
		}
		drache_s.AnimWinkel = 0;
		e->blick = -1;
		PlattformTest(e, &GegnerRect[e->GegnerArt]);
		if (pl.Handlung == PA_TOT ||
		    (pl.AufPlattform == e && pl.Handlung != PA_RADELN && pl.Handlung != PA_RADELN_FALL))
		{
			snd(SOUND_EXPLOSION1);
			snd(SOUND_ROCKET);
			e->Handlung = GEGNER_AUSFAHREN;
			e->xAcc = FX(-2.0);
			e->yAcc = FX(-0.2);
			pl.DoFesteAktion = 1;
			pl.FesteAktion = -1;
			pl.CanBeDamaged = 0;
		}
		break;
	}

	case GEGNER_AUSFAHREN:                     /* flies off with the player */
		PlattformTest(e, &GegnerRect[e->GegnerArt]);
		if (drache_s.HeadWinkel < FX(30)) drache_s.HeadWinkel += SYNC(FX(3.0));
		if (drache_s.KieferWinkel < DRACHE_PI) drache_s.KieferWinkel += SYNC(FX(0.5));
		if (e->xPos < FX(e->Value1 - 500))
		{
			e->xPos = FX(e->Value1 - 500);
			e->xSpeed = 0;
			e->ySpeed = 0;
			e->xAcc = 0;
			e->yAcc = 0;
			e->Handlung = -1;
			pl.FesteAktion = -1;
			pl.DoFesteAktion = 1;
			stage_clear();                     /* (Hurrican: NewStage = Stage + 1) */
		}
		break;

	case GEGNER_EINFLIEGEN:                    /* the player on its back (Value2 != 0) */
		if (drache_s.Attack != GEGNER_STEHEN)
		{
			pl.xpos = e->xPos + FX(40);
			pl.ypos = e->yPos - FX(65) + drache_s.DrawYOffset;
			pl.Blickrichtung = -1;
			pl.AufPlattform = e;
		}
		if (PlayerAbstand(e) < 1000)
		{
			e->AnimCount -= FX(1.0);
			if (drache_s.Attack == GEGNER_STEHEN) PlattformTest(e, &GegnerRect[e->GegnerArt]);
		}
		if (e->AnimCount < 0)
		{
			e->AnimCount = FX(15.0);
			part_push(e->xPos - FX(120) + FX(GetRandom(300)), e->yPos + FX(GetRandom(100)) + drache_s.DrawYOffset, SMOKEBIG);
			if (drache_s.Attack != GEGNER_STEHEN && GetRandom(4) == 0)
			{
				part_push(e->xPos - FX(120) + FX(GetRandom(300)), e->yPos + FX(GetRandom(100)) + drache_s.DrawYOffset,
				          EXPLOSION_MEDIUM2);
				snd(SOUND_EXPLOSION1);
			}
		}
		switch (drache_s.Attack)
		{
		case GEGNER_LAUFEN:                    /* flying */
			drache_off += SYNC(FX(0.5));
			if (drache_off > DRACHE_TWO_PI) drache_off -= DRACHE_TWO_PI;
			drache_s.DrawYOffset = drache_sinr(drache_off) * 8;
			if (e->Energy > FX(100)) e->Energy -= SYNC(FX(85.0));
			else
			{
				e->yAcc = FX(2.0);
				drache_s.Attack = GEGNER_LAUFEN2;
			}
			break;
		case GEGNER_LAUFEN2:                   /* crashing */
			if (drache_s.DrawYOffset > 0) drache_s.DrawYOffset -= SYNC(FX(1.0));
			else drache_s.DrawYOffset = 0;
			if (e->ySpeed > 0)
			{
				fx yy = e->yPos, yo = e->yPosOld;
				if (BlockUnten(e->xPos, &yy, e->xPosOld, &yo, &GegnerRect[e->GegnerArt], 0) & B_WAND)
				{
					e->ySpeed = FMUL(e->ySpeed, FX(-0.3));
					for (i = 0; i < 30; i++)
						part_push(e->xPos - FX(100) + FX(GetRandom(260)), e->yPos + FX(120) - FX(GetRandom(10)), SNOWFLUSH);
					if (e->ySpeed > FX(-0.5))
					{
						e->ySpeed = 0;
						e->yAcc = 0;
						drache_s.Attack = GEGNER_LAUFEN3;
						e->xAcc = FX(2.0);
						drache_s.SmokeCount = 0;
						drache_s.HeadLocked = 1;
					}
				}
			}
			break;
		case GEGNER_LAUFEN3:                   /* sliding */
			if (drache_s.KieferWinkel < DRACHE_HALF_PI) drache_s.KieferWinkel += SYNC(FX(0.1));
			else drache_s.KieferWinkel = DRACHE_HALF_PI;
			if (drache_s.HeadWinkel > FX(-70)) drache_s.HeadWinkel -= SYNC(FX(2.0));
			else drache_s.HeadWinkel = FX(-70);
			drache_s.SmokeCount -= SYNC(FX(1.0));
			if (drache_s.SmokeCount < 0)
			{
				drache_s.SmokeCount = FX(0.2);
				part_push(e->xPos - FX(100) + FX(GetRandom(260)), e->yPos + FX(120), SNOWFLUSH);
			}
			if (e->xSpeed > 0)                 /* stopped */
			{
				e->xSpeed = 0;
				e->xAcc = 0;
				drache_s.Attack = GEGNER_STEHEN;
				pl.DoFesteAktion = 0;
				music_stage();
			}
			break;
		case GEGNER_STEHEN:                    /* lies there and smokes */
			if (drache_s.KieferWinkel > DRACHE_HALF_PI) drache_s.KieferWinkel -= SYNC(FX(0.1));
			else drache_s.KieferWinkel = DRACHE_HALF_PI;
			if (drache_s.HeadWinkel > FX(-70)) drache_s.HeadWinkel -= SYNC(FX(2.0));
			else drache_s.HeadWinkel = FX(-70);
			break;
		}
		break;

	case GEGNER_EXPLODIEREN:
		drache_s.AnimWinkel = 0;
		e->Energy = FX(100);
		drache_s.ShotDelay -= SYNC(FX(1.0));
		if (drache_s.ShotDelay < 0)
		{
			drache_s.ShotDelay = FX(0.5);
			snd(SOUND_EXPLOSION1);
			part_push(e->xPos - FX(100) + FX(GetRandom(500)), e->yPos + FX(20) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
			if (GetRandom(3) == 0)
				part_push(e->xPos - FX(100) + FX(GetRandom(500)), e->yPos + FX(20) + FX(GetRandom(80)), EXPLOSION_BIG);
		}
		switch (drache_s.Attack)
		{
		case GEGNER_LAUFEN:
			if (drache_s.HeadWinkel > FX(-70)) drache_s.HeadWinkel -= SYNC(FX(5.0));
			if (drache_s.KieferWinkel < FX(30)) drache_s.HeadWinkel += SYNC(FX(1.0));
			if (e->xSpeed < 0) e->xAcc = FX(1.0);
			if (e->xSpeed > 0) e->xAcc = FX(-1.0);
			if (e->yPos < drache_s.StartPosY - FX(400))    /* high enough: fall */
			{
				e->xPos = FX(e->Value1 + 450);
				e->xAcc = 0;
				e->yAcc = 0;
				e->xSpeed = 0;
				e->ySpeed = FX(40.0);
				drache_s.HeadX = 0;
				drache_s.HeadY = 0;
				drache_s.HeadLocked = 1;
				drache_s.HeadWinkel = FX(75);
				drache_s.KieferWinkel = 0;
				e->Destroyable = 0;
				drache_s.Attack = GEGNER_LAUFEN2;
			}
			break;
		case GEGNER_LAUFEN2:
			e->blick = -1;
			if (e->yPos > drache_s.StartPosY + FX(295) && e->ySpeed > 0)   /* down: wait for the player */
			{
				for (i = 0; i < 30; i++) part_push(e->xPos - FX(120) + FX(GetRandom(300)), e->yPos + FX(80), SMOKEBIG);
				shake_screen(5);
				snd(SOUND_DOORSTOP);
				e->xSpeed = 0;
				e->ySpeed = 0;
				e->xAcc = 0;
				e->yAcc = 0;
				e->Handlung = GEGNER_WARTEN;
				drache_s.ArrowCount = FX(2.0);
				e->yPos = drache_s.StartPosY + FX(295);
			}
			break;
		}
		break;

	case GEGNER_NOTVISIBLE:                    /* wait for the screen */
		drache_s.StartPosY = e->yPos - FX(25);
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN2;
		}
		break;

	case GEGNER_EINFLIEGEN2:                   /* the first time: the small dragon flies past behind */
	{
		e->Energy = FX(7000);
		e->DamageTaken = 0;
		Ent *h = drache_hack();
		if (h && h->i[0] == 1) drache_vorbei = 1;
		if (!h || (h->i[0] == 0 && drache_vorbei))
		{
			if (h) h->Energy = 0;
			drache_s.hack = -1;
			e->Handlung = GEGNER_AUSWAHL;
		}
		break;
	}

	case GEGNER_AUSWAHL:                       /* the next move */
	{
		drache_s.HeadLocked = 0;
#ifndef OJ32X_MARS
		/* PC check: HB_B1_HP=<n> starts the fight with n energy (to test the end quickly) */
		if (getenv("HB_B1_HP") && e->Energy > FX(atoi(getenv("HB_B1_HP")))) e->Energy = FX(atoi(getenv("HB_B1_HP")));
#endif
		switch (GetRandom(3))
		{
		case 0:                                /* over the player, with an attack */
			e->Handlung = GEGNER_LAUFEN;
			e->blick = (int8_t)(e->xPos < FX(e->Value1) ? -1 : 1);
			e->xPos = FX(e->Value1 + 320 + 100 + 800 * e->blick);
			e->yPos = drache_s.StartPosY - FX(80);
			e->xSpeed = FX(-15.0) * e->blick;
			e->ySpeed = FX(5.0);
			e->yAcc = FX(-0.1);
			if (GetRandom(2) == 0)
			{
				drache_s.Attack = GEGNER_SCHIESSEN;     /* fire */
				drache_s.KieferWinkel = 0;
				e->AnimCount = DRACHE_PI - FX(1.0);
			}
			else
			{
				drache_s.Attack = GEGNER_BOMBARDIEREN;  /* lay eggs */
				drache_s.KieferWinkel = 0;
				e->AnimCount = FX(10.0);
			}
			break;
		case 1:                                /* race past, dropping things */
			e->Handlung = GEGNER_LAUFEN2;
			e->blick = (int8_t)(e->xPos < FX(e->Value1) ? -1 : 1);
			e->xPos = FX(e->Value1 + 320 + 100 + 800 * e->blick);
			e->yPos = drache_s.StartPosY + FX(50);
			e->xSpeed = FX(-50.0) * e->blick;
			e->ySpeed = 0;
			e->yAcc = 0;
			drache_s.Attack = GEGNER_BOMBARDIEREN;
			drache_s.KieferWinkel = 0;
			e->AnimCount = DRACHE_PI - FX(1.0);
			drache_s.ShotDelay = 0;
			break;
		case 2:                                /* in from the side */
			e->Handlung = GEGNER_LAUFEN3;
			drache_s.Attack = GEGNER_EINFLIEGEN;
			drache_s.ShotCount = 2 + GetRandom(2);
			drache_s.Position = e->xPos < FX(e->Value1) ? 1 : -1;
			e->yPos = drache_s.StartPosY + FX(50) + FX(GetRandom(50));
			drache_s.KieferWinkel = DRACHE_HALF_PI;
			e->xSpeed = 0;
			e->ySpeed = 0;
			e->xAcc = 0;
			e->yAcc = 0;
			e->AnimCount = 0;
			break;
		}
		break;
	}

	case GEGNER_LAUFEN:
		switch (drache_s.Attack)
		{
		case GEGNER_SCHIESSEN:                 /* fire balls */
			drache_s.KieferWinkel += SYNC(FX(0.75));
			while (drache_s.KieferWinkel > DRACHE_TWO_PI) drache_s.KieferWinkel -= DRACHE_TWO_PI;
			e->AnimCount += SYNC(FX(0.75));
			while (e->AnimCount > DRACHE_TWO_PI)
			{
				e->AnimCount -= DRACHE_TWO_PI;
				if (e->blick == 1)
				{
					proj_winkel = FX(90) - drache_s.HeadWinkel;
					proj_push(e->xPos + FX(240) + drache_s.HeadWinkel / 2, e->yPos + FX(50) - drache_s.HeadWinkel / 2, FIREBALL_BIG);
				}
				else
				{
					proj_winkel = FX(270) + drache_s.HeadWinkel;
					proj_push(e->xPos - FX(110) - drache_s.HeadWinkel / 2, e->yPos + FX(50) - drache_s.HeadWinkel / 2, FIREBALL_BIG);
				}
				snd(SOUND_FIREBALL);
			}
			break;
		case GEGNER_BOMBARDIEREN:              /* mini dragons */
			if (e->xPos + FX(100 - drache_s.mirrorOffset) > cam_xoff() &&
			    e->xPos + FX(100 - drache_s.mirrorOffset) < cam_xoff() + FX(RENDERWIDTH))
				e->AnimCount -= SYNC(FX(1.0));
			if (e->AnimCount < 0)
			{
				e->AnimCount = FX(25.0);
				gegner_push(e->xPos + FX(100 - drache_s.mirrorOffset), e->yPos + FX(80), MINIDRAGON, 0, 99);
			}
			break;
		}
		if ((e->xSpeed > 0 && e->xPos > FX(e->Value1 + 1000)) || (e->xSpeed < 0 && e->xPos < FX(e->Value1 - 500)))
			e->Handlung = GEGNER_AUSWAHL;
		break;

	case GEGNER_LAUFEN2:                       /* racing past */
		switch (drache_s.Attack)
		{
		case GEGNER_BOMBARDIEREN:
			if (e->xPos + FX(100 - drache_s.mirrorOffset) > cam_xoff() + FX(50) &&
			    e->xPos + FX(100 - drache_s.mirrorOffset) < cam_xoff() + FX(600))
				drache_s.ShotDelay -= SYNC(FX(1.0));
			if (drache_s.ShotDelay < 0)
			{
				drache_s.ShotDelay = FX(3.0);
				gegner_push(e->xPos + FX(100 - drache_s.mirrorOffset), e->yPos + FX(80), FETTERAKETE, 0, 99);
			}
			/* leaves in the wind, which pushes the player */
			e->AnimCount -= SYNC(FX(1.0));
			if (e->AnimCount < 0)
			{
				e->AnimCount = FX(0.3);
				if (e->blick == 1)
				{
					proj_winkel = FX(180);
					if (e->xPos > cam_xoff())
						part_push(cam_xoff() + FX(10), cam_yoff() + FX(GetRandom(RENDERHEIGHT)), BLATT2);
					pl.xpos += SYNC(FX(8.0));
				}
				else
				{
					proj_winkel = 0;
					if (e->xPos < cam_xoff() + FX(550))
						part_push(cam_xoff() + FX(RENDERWIDTH), cam_yoff() + FX(GetRandom(RENDERHEIGHT)), BLATT2);
					pl.xpos -= SYNC(FX(8.0));
				}
			}
			break;
		}
		if ((e->xSpeed > 0 && e->xPos > FX(e->Value1 + 1000)) || (e->xSpeed < 0 && e->xPos < FX(e->Value1 - 500)))
			e->Handlung = GEGNER_AUSWAHL;
		break;

	case GEGNER_LAUFEN3:                       /* in from the side, the head shoots out */
		switch (drache_s.Attack)
		{
		case GEGNER_EINFLIEGEN:
			if (drache_s.Position == 1) e->xPos = FX(e->Value1 - 300) + drache_sinr(e->AnimCount) * 150;
			else e->xPos = FX(e->Value1 + 740) - drache_sinr(e->AnimCount) * 150;
			if (e->AnimCount >= DRACHE_HALF_PI)
			{
				if (e->AnimCount == DRACHE_HALF_PI) drache_s.Attack = GEGNER_SCHIESSEN;
				else e->AnimCount = DRACHE_HALF_PI;
			}
			else e->AnimCount += SYNC(FX(0.075));
			break;

		case GEGNER_SCHIESSEN:                 /* open the jaw */
			drache_s.KieferWinkel -= SYNC(FX(0.5));
			if (drache_s.KieferWinkel < -DRACHE_HALF_PI)
			{
				drache_s.KieferWinkel = -DRACHE_HALF_PI;
				drache_s.Attack = GEGNER_VERFOLGEN;
				drache_s.HeadLocked = 1;
				{
					const int hw = FINT(drache_s.HeadWinkel);
					drache_s.HeadXSpeed = cos_d(hw) * (40 + 10 * SKILL) * drache_s.Position;
					drache_s.HeadYSpeed = -sin_d(hw) * (30 + 8 * SKILL);
				}
				if (drache_s.Position == -1 && drache_s.HeadXSpeed > FX(-0.1)) drache_s.HeadXSpeed = -6;     /* -0.01 (made big enough to move each step in fx) */
				if (drache_s.Position == 1 && drache_s.HeadXSpeed < FX(0.1)) drache_s.HeadXSpeed = 6;
			}
			break;

		case GEGNER_VERFOLGEN:                 /* the head shoots at the player */
		{
			static const Box colliderect = { 40, 30, 50, 50 };
			int hit = 0;
			drache_s.HeadX += SYNC(drache_s.HeadXSpeed);
			drache_s.HeadY += SYNC(drache_s.HeadYSpeed);
			if (pl.Handlung != PA_TOT &&
			    SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &colliderect))
			{
				drache_s.ShotCount = 0;
				hit = 1;
				DamagePlayer(FX(500), 0);
			}
			/* the head on the floor or at the side: sound, sparks, back */
			if ((drache_s.HeadYSpeed > 0 && e->yPos + FX(50) + drache_s.HeadY > drache_s.StartPosY + FX(380)) ||
			    (drache_s.HeadXSpeed > 0 && e->xPos + drache_s.HeadX > FX(e->Value1 + 400)) ||
			    (drache_s.HeadXSpeed < 0 && e->xPos + drache_s.HeadX < FX(e->Value1 + 100)) || hit)
			{
				drache_s.Attack = GEGNER_SCHLIESSEN;
				e->AnimCount = FX(4.0);
				drache_s.KieferWinkel = DRACHE_HALF_PI;
				snd(SOUND_KLONG);
				drache_s.HeadYSpeed = -drache_s.HeadYSpeed;
				drache_s.HeadXSpeed = -drache_s.HeadXSpeed;
				for (i = 0; i < 30; i++)
					part_push(e->xPos + drache_s.HeadX + FX(220) + FX(GetRandom(20)) + FX(drache_s.mirrorOffset * 275),
					          e->yPos + drache_s.HeadY + FX(70) + FX(GetRandom(20)), FUNKE);
			}
			break;
		}

		case GEGNER_SCHLIESSEN:
			if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1.0));
			else
			{
				e->AnimCount = 0;
				drache_s.HeadX += SYNC(drache_s.HeadXSpeed);
				drache_s.HeadY += SYNC(drache_s.HeadYSpeed);
				/* the head back home? */
				if (drache_s.HeadY < 0 &&
				    ((drache_s.HeadXSpeed < 0 && drache_s.HeadX < 0) || (drache_s.HeadXSpeed > 0 && drache_s.HeadX > 0)))
				{
					drache_s.HeadLocked = 0;
					drache_s.ShotCount--;
					if (drache_s.ShotCount > 0) drache_s.Attack = GEGNER_SCHIESSEN;   /* once more */
					else
					{
						e->AnimCount = DRACHE_HALF_PI;                            /* or back out */
						drache_s.Attack = GEGNER_EINFLIEGEN2;
					}
				}
			}
			break;

		case GEGNER_EINFLIEGEN2:
			if (drache_s.Position == 1) e->xPos = FX(e->Value1 - 300) + drache_sinr(e->AnimCount) * 150;
			else e->xPos = FX(e->Value1 + 740) - drache_sinr(e->AnimCount) * 150;
			e->AnimCount -= SYNC(FX(0.075));
			if (e->AnimCount <= -DRACHE_HALF_PI) e->Handlung = GEGNER_AUSWAHL;
			break;
		}
		break;

	default:
		break;
	}

	if (e->Destroyable) TestDamagePlayers(e, SYNC(FX(5.0)), 0);

	/* the drive's smoke */
	drache_s.SmokeCount -= SYNC(FX(1.0));
	if (e->Handlung != GEGNER_EINFLIEGEN || drache_s.Attack == GEGNER_LAUFEN)
		if ((e->Handlung != GEGNER_WARTEN && e->Handlung != GEGNER_EXPLODIEREN) || drache_s.Attack == GEGNER_LAUFEN)
			if (drache_s.SmokeCount < 0)
			{
				drache_s.SmokeCount = FX(0.2);
				if (e->blick == -1) { proj_winkel = FX(1.0); part_push(e->xPos + FX(180), e->yPos + FX(9) + drache_s.DrawYOffset, DRACHE_SMOKE); }
				else { proj_winkel = 0; part_push(e->xPos - FX(40), e->yPos + FX(9) + drache_s.DrawYOffset, DRACHE_SMOKE); }
			}

	/* the tail */
	if (!(e->Handlung == GEGNER_EINFLIEGEN && drache_s.Attack == GEGNER_STEHEN)) drache_s.TailSinus += SYNC(FX(1.0));
	while (drache_s.TailSinus > DRACHE_TWO_PI) drache_s.TailSinus -= DRACHE_TWO_PI;

	/* DoDraw's state part (facing, rectangle, arrow) */
	if (e->Handlung != GEGNER_NOTVISIBLE && IsOnScreen(e) && PlayerAbstand(e) <= 1000) drache_draw_state(e);
}

static void drache_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(DRACHE) = { drache_init, drache_ki, drache_explode, drache_draw, drache_load };
