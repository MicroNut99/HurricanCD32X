/*
 * METALHEAD - the robot skull of the cave level (bosses/Boss_MetalHead.cpp): rises out of the
 * scrap heap on its long neck, opens its jaw and turbine and shoots fire balls (aimed, a spread,
 * bombs), crushes down on the player and on the floor (dropping a SCHLEIMBOLLER). The turbine
 * takes the first half of the hits; once it is shot off the eye is the target and the boss
 * spits flames. When killed, the neck's vertebrae blow off one by one.
 * Own members: the static struct metalhead_s (one MetalHead at a time): ShotCount, Akt, ShotArt,
 * KieferSpeed, KieferPos, TurbineOff, Eye_Alpha, SmokeCount, ShotDelay, GunWinkel, NewX, NewY,
 * SinCount, Moving, Turbine_dran, Hals[25] (the vertebrae: x, y in the world, w the angle).
 * The vertebrae's places are worked out at the end of DoKI (Hurrican does it in DoDraw).
 * Drawing: no rotation (the turbine, the neck parts and the spinning head are drawn upright);
 * left out: the eye's corona (giantspiderflare, additive), the turbine's glow (Turbine2,
 * additive; its Handlung SPECIAL2 is never set anyway), the red tint (the engine's hit flash
 * stands in), ShadowAlpha, PartikelSystem.SetTarget (the turbine smoke is not ported),
 * ShakeScreen, the sound pitches.
 * The picture "metalhead_kiefer2.png" has the same first 16 letters as "metalhead_kiefer.png"
 * (the packs keep 16): metalhead_pic_sized finds it by its size.
 * GegnerRect[METALHEAD] is set as Hurrican's (20, 20 + 113, 130, 130 + 82) in the constructor
 * (game/hur_tables.h has 20, 20, 130, 130).
 */
#define METALHEAD_MAXWIRBEL 25
#define METALHEAD_SK_AUSGANG 1
#define METALHEAD_SK_BANGEN  3
#define METALHEAD_HALF_PI FX(1.57079633)
#define METALHEAD_TWO_PI  FX(6.28318531)

static struct {
	int ShotCount, Akt, ShotArt, Moving, Turbine_dran;
	fx KieferSpeed, KieferPos, TurbineOff, Eye_Alpha, SmokeCount, ShotDelay, GunWinkel, NewX, NewY, SinCount;
	struct { fx x, y, w; } Hals[METALHEAD_MAXWIRBEL];
} metalhead_s;
static Pic metalhead_Kiefer, metalhead_Kiefer2, metalhead_Turbine, metalhead_HalsGfx;

/* sin of fx degrees, interpolated between whole degrees */
static fx metalhead_sind(fx deg)
{
	const int d = deg >> 8, frac = deg & 255;
	const int a = sin_d(d), b = sin_d(d + 1);
	return a + (((b - a) * frac) >> 8);
}
static fx metalhead_sinr(fx rad) { return metalhead_sind(FMUL(rad, FX(57.2957795))); }

/* a picture by its name and full size (two names alike in their first 16 letters) */
static Pic metalhead_pic_sized(const char *name, int w, int h)
{
	(void)w; (void)h;             /* the packs keep whole names now */
	return pic(name);
}

static void metalhead_load(void)
{
	metalhead_Kiefer = metalhead_pic_sized("metalhead_kiefer.png", 102, 153);
	metalhead_Kiefer2 = metalhead_pic_sized("metalhead_kiefer2.png", 131, 88);
	metalhead_Turbine = pic("metalhead_turbine.png");
	metalhead_HalsGfx = pic("metalhead_halsteil.png");
}

static void metalhead_init(Ent *e)
{
	memset(&metalhead_s, 0, sizeof metalhead_s);
	metalhead_s.Akt = METALHEAD_SK_AUSGANG;
	metalhead_s.KieferSpeed = FX(10.0);
	metalhead_s.Turbine_dran = 1;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(4000);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	g_Fahrstuhl_Speed = 0;
	GegnerRect[METALHEAD] = (Box){ 20, 20 + 113, 130, 130 + 82 };
}

/* the neck from the left edge to the head (Hurrican's DoDraw) */
static void metalhead_calc_hals(Ent *e)
{
	const fx dx = e->xPos - FX(e->Value1 + 40);
	const fx dy = e->yPos - FX(e->Value2 + 250);
	int i;
	for (i = 0; i < METALHEAD_MAXWIRBEL; i++)
	{
		metalhead_s.Hals[i].x = FX(e->Value1 - 40) + dx * i / (METALHEAD_MAXWIRBEL - 1);
		/* the way from the left edge to the head is 0 .. PI of a cosine */
		const fx c = metalhead_sind(FX(90) + FX(180) * i / METALHEAD_MAXWIRBEL);
		metalhead_s.Hals[i].y = FX(e->Value2 + 400) + dy / 2 - FMUL(c, dy) / 2;
		metalhead_s.Hals[i].w = 0;
	}
	for (i = 0; i < METALHEAD_MAXWIRBEL - 1; i++)
	{
		const fx ax = metalhead_s.Hals[i + 1].x - metalhead_s.Hals[i].x;
		const fx ay = metalhead_s.Hals[i + 1].y - metalhead_s.Hals[i].y;
		/* tanh(ay / ax) (as Hurrican: tanh, not atan) in degrees + 90 */
		fx t;
		if (ax == 0) t = ay > 0 ? FX(1) : ay < 0 ? FX(-1) : 0;
		else
		{
			fx r = FDIV(ay, ax);
			if (r > FX(3)) r = FX(3);
			if (r < FX(-3)) r = FX(-3);
			const fx r2 = FMUL(r, r);
			t = FDIV(FMUL(r, FX(27) + r2), FX(27) + 9 * r2);
		}
		fx w = FMUL(t, FX(57.2957795)) + FX(90);
		while (w < 0) w += FX(360);
		while (w >= FX(360)) w -= FX(360);
		metalhead_s.Hals[i].w = w;
	}
}

static void metalhead_draw(Ent *e)
{
	int i;
	if (e->Handlung == GEGNER_EXPLODIEREN)
	{
		/* the head spins away */
		if (e->AnimEnde > METALHEAD_MAXWIRBEL - 5) draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
		for (i = 0; i < e->AnimEnde && i < METALHEAD_MAXWIRBEL; i++)
			draw_sprite(&metalhead_HalsGfx, 0, metalhead_s.Hals[i].x, metalhead_s.Hals[i].y, 0);
		return;
	}
	draw_sprite(&metalhead_Kiefer, 0, e->xPos + FX(87), e->yPos + FX(107) + metalhead_s.KieferPos, 0);
	for (i = 0; i < METALHEAD_MAXWIRBEL - 1; i++)
		draw_sprite(&metalhead_HalsGfx, 0, metalhead_s.Hals[i].x, metalhead_s.Hals[i].y, 0);
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
	if (metalhead_s.Turbine_dran)
		draw_sprite(&metalhead_Turbine, 0, e->xPos + FX(85) + metalhead_s.TurbineOff, e->yPos + FX(202), 0);
	draw_sprite(&metalhead_Kiefer2, 0, e->xPos + FX(78), e->yPos + FX(180) + metalhead_s.KieferPos, 0);
}

/* a new point to move to */
static void metalhead_MoveToNewPoint(Ent *e, fx x, fx y, int aktion)
{
	metalhead_s.Moving = 1;
	metalhead_s.NewX = x;
	metalhead_s.NewY = y;
	e->xSpeed = e->ySpeed = 0;
	if (e->xPos < x) e->xSpeed = FX(10.0);
	if (e->xPos > x) e->xSpeed = FX(-10.0);
	if (e->yPos < y) e->ySpeed = FX(10.0);
	if (e->yPos > y) e->ySpeed = FX(-10.0);
	e->Handlung = (int16_t)aktion;
}

/* the gun's angle to the player */
static void metalhead_WinkelToPlayer(Ent *e)
{
	int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + FX(200)));
	const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(100)));
	if (ydiv == 0) ydiv = 1;
	fx neww;
	if (metalhead_s.Akt != GEGNER_SCHIESSEN) neww = 0;
	else
	{
		int a = xdiv == 0 ? (ydiv > 0 ? 90 : -90) : atan2_d(ydiv, xdiv);
		if (a > 90) a -= 180;
		if (a < -90) a += 180;
		neww = FX(a);
		if (neww < FX(-20)) neww = FX(-20);
		if (neww > FX(50)) neww = FX(50);
	}
	if (metalhead_s.GunWinkel < neww) metalhead_s.GunWinkel += SYNC(FX(8.0));
	if (metalhead_s.GunWinkel > neww) metalhead_s.GunWinkel -= SYNC(FX(8.0));
}

static void metalhead_DoMove(Ent *e)
{
	if (e->xSpeed < 0 && metalhead_s.NewX > e->xPos) { e->xSpeed += SYNC(FX(1.0)); if (e->xSpeed > 0) e->xSpeed = 0; }
	if (e->xSpeed > 0 && metalhead_s.NewX < e->xPos) { e->xSpeed -= SYNC(FX(1.0)); if (e->xSpeed < 0) e->xSpeed = 0; }
	if (e->ySpeed < 0 && metalhead_s.NewY > e->yPos) { e->ySpeed += SYNC(FX(1.0)); if (e->ySpeed > 0) e->ySpeed = 0; }
	if (e->ySpeed > 0 && metalhead_s.NewY < e->yPos) { e->ySpeed -= SYNC(FX(1.0)); if (e->ySpeed < 0) e->ySpeed = 0; }
	if (e->xSpeed == 0 && e->ySpeed == 0) metalhead_s.Moving = 0;
}

#ifndef OJ32X_MARS
/* PC check: HB_B1LOG=1 prints the state every half second */
static void metalhead_log(const Ent *e)
{
	static int n;
	if (getenv("HB_B1LOG") && n++ % 30 == 0)
		fprintf(stderr, "metalhead: H %d E %d pos %d,%d speed %d,%d anim %d dest %d cam %d,%d state %d player %d,%d\n", e->Handlung,
		        FINT(e->Energy), FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->AnimPhase, e->Destroyable,
		        FINT(cam_xoff()), FINT(cam_yoff()), tile_zustand(), FINT(pl.xpos), FINT(pl.ypos));
}
#else
#define metalhead_log(e) ((void)0)
#endif

static void metalhead_ki(Ent *e)
{
	metalhead_log(e);
	int i;
	const int V1 = e->Value1, V2 = e->Value2;

	if (!metalhead_s.Turbine_dran) GegnerRect[METALHEAD] = (Box){ 140, 90, 180, 130 };

	/* the player under the head? Then crash down */
	if (e->Handlung != GEGNER_CRUSHENERHOLEN && e->Handlung != GEGNER_EINFLIEGEN && e->Handlung != GEGNER_EXPLODIEREN &&
	    e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_CRUSHEN && e->Handlung != GEGNER_CRUSHEN2 &&
	    e->yPos < FX(V2 + 230))
		if (pl.xpos + FX(35) < e->xPos + FX(200))
		{
			e->ySpeed = FX(50.0);
			e->Handlung = GEGNER_CRUSHEN;
			snd(SOUND_STONEFALL);
			e->yAcc = FX(10.0);
			metalhead_s.Moving = 0;
		}

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN) boss_bar(e->Energy, FX(4000));

	/* lock the screen and stop the stage music */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(V1), FX(V2), TS_SCROLLTOLOCK);
		music_stop();
	}

	if (e->DamageTaken > 0)
	{
		/* the turbine takes the hits while it is on */
		if (metalhead_s.Turbine_dran && e->Energy < FX(2000))
		{
			metalhead_s.Turbine_dran = 0;
			for (i = 0; i < 20; i++)
				part_push(e->xPos + FX(20) + FX(GetRandom(80)), e->yPos + FX(130) + FX(GetRandom(20)), EXPLOSION_MEDIUM2);
			part_push(e->xPos + FX(85) + metalhead_s.TurbineOff, e->yPos + FX(202), KAPUTTETURBINE);
			snd(SOUND_EXPLOSION2);
			shake_screen(3);
		}
		e->DamageTaken -= SYNC(FX(10.0));
	}
	else e->DamageTaken = 0;

	/* little energy: the neck smokes and sparks */
	if (e->Energy < FX(2500) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		metalhead_s.SmokeCount -= SYNC(FX(1.0));
		while (metalhead_s.SmokeCount < 0)
		{
			metalhead_s.SmokeCount += FX(0.8);
			part_push(e->xPos + FX(50) + FX(GetRandom(15)), e->yPos + FX(160) + FX(GetRandom(4)), SMOKE3);
			if (GetRandom(3) == 0)
				part_push(e->xPos + FX(40) + FX(GetRandom(10)), e->yPos + FX(140) + FX(GetRandom(4)), SMOKEBIG);
			if (GetRandom(10) == 0)
			{
				for (i = 0; i < 16; i++)
				{
					part_push(e->xPos + FX(55) + FX(GetRandom(15)), e->yPos + FX(160) + FX(GetRandom(15)), FUNKE);
					part_push(e->xPos + FX(55) + FX(GetRandom(15)), e->yPos + FX(160) + FX(GetRandom(15)), LASERFUNKE2);
				}
				part_push(e->xPos + FX(30) + FX(GetRandom(20)), e->yPos + FX(140), LASERFLAME);
				part_push(e->xPos + FX(5), e->yPos + FX(110), EXPLOSIONFLARE);
				part_push(e->xPos + FX(5), e->yPos + FX(110), EXPLOSIONFLARE);
				snd(SOUND_FUNKE);
			}
		}
	}

	/* no energy left: explode */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Destroyable = 0;
		e->Energy = FX(100);
		e->Handlung = GEGNER_EXPLODIEREN;
		e->AnimEnde = METALHEAD_MAXWIRBEL - 1;
		e->xSpeed = FX(20.0);
		e->ySpeed = FX(-30.0);
		e->xAcc = 0;
		e->yAcc = FX(5.0);
		e->AnimCount = FX(8.0);
		metalhead_s.Eye_Alpha = METALHEAD_TWO_PI;
		if (pl.AufPlattform == e) pl.AufPlattform = 0;
		music_stop();                       /* (fades out the boss music) */
		for (i = 0; i < 60; i++)
			part_push(e->xPos + FX(GetRandom(10)), e->yPos + FX(90) + FX(GetRandom(10)), FUNKE);
		part_push(e->xPos - FX(40), e->yPos + FX(40), EXPLOSION_GIANT);
		snd(SOUND_EXPLOSION2);
	}

	/* the jaw moves in some actions */
	if (e->Handlung == GEGNER_SPECIAL || e->Handlung == GEGNER_SPECIAL2 || e->Handlung == GEGNER_SPECIAL3)
	{
		metalhead_s.KieferSpeed -= SYNC(FX(2.0));
		metalhead_s.KieferPos += SYNC(metalhead_s.KieferSpeed);
		if (metalhead_s.KieferPos < 0 && metalhead_s.KieferSpeed < 0)
		{
			metalhead_s.KieferSpeed = FX(10.0);
			snd(SOUND_KLONG);
			for (i = 0; i < 10; i++) part_push(e->xPos + FX(130) + FX(i * 4), e->yPos + FX(160), LONGFUNKE);
		}
	}

	if (metalhead_s.Moving) metalhead_DoMove(e);
	else
		switch (e->Handlung)
		{
		case GEGNER_CRUSHEN:
		case GEGNER_CRUSHEN2:
			metalhead_s.TurbineOff = 0;
			if (metalhead_s.GunWinkel > 0) metalhead_s.GunWinkel -= SYNC(FX(50.0));
			if (metalhead_s.GunWinkel < 0) metalhead_s.GunWinkel += SYNC(FX(50.0));
			if (metalhead_s.KieferPos > 0) metalhead_s.KieferPos -= SYNC(FX(50.0));
			else metalhead_s.KieferPos = 0;
			if (e->yPos > FX(V2 + 200))
			{
				e->xSpeed = 0;
				e->ySpeed = 0;
				e->xAcc = 0;
				e->yAcc = FX(-1.0);
				shake_screen(5);
				snd(SOUND_STONEEXPLODE);
				if (e->Handlung == GEGNER_CRUSHEN2)
					gegner_push(FX(V1 + 50 + GetRandom(550)), FX(V2 - 40), SCHLEIMBOLLER, 40, 0);   /* size 40: medium */
				e->Handlung = GEGNER_CRUSHENERHOLEN;
				for (i = 0; i < 20; i++) part_push(e->xPos + FX(GetRandom(180)), e->yPos + FX(200), SMOKEBIG);
				/* the player still under it? */
				if (pl.xpos + FX(35) < e->xPos + FX(200)) DamagePlayer(FX(500), 0);
			}
			break;

		case GEGNER_CRUSHENERHOLEN:
			if (e->ySpeed < 0 && e->yPos < FX(V2 + 50))
			{
				e->ySpeed = 0;
				e->yAcc = 0;
				metalhead_MoveToNewPoint(e, FX(V1 + 130), FX(V2 + 0), GEGNER_SCHIESSEN);
				metalhead_s.ShotArt = 1;
			}
			break;

		case GEGNER_NOTVISIBLE:
			e->Handlung = GEGNER_INIT;
			break;

		case GEGNER_INIT:                       /* wait for the screen */
			if (tile_zustand() == TS_LOCKED)
			{
				music_boss();
				e->Handlung = GEGNER_EINFLIEGEN;
			}
			break;

		case GEGNER_EINFLIEGEN:                 /* the head rises from the scrap */
			e->Energy = FX(4000);
			e->DamageTaken = 0;
			metalhead_s.Eye_Alpha += SYNC(FX(10.0));
			if (metalhead_s.Eye_Alpha > FX(255))
			{
				metalhead_s.Eye_Alpha = FX(255);
				metalhead_MoveToNewPoint(e, FX(V1 + 70), FX(V2 + 130), GEGNER_SCHIESSEN);
				metalhead_s.ShotArt = 0;
				metalhead_s.Akt = GEGNER_OEFFNEN;
				metalhead_s.SinCount = 0;
			}
			break;

		case GEGNER_SCHIESSEN:
			if (metalhead_s.ShotArt == 2)
			{
				if (metalhead_s.GunWinkel > 0) metalhead_s.GunWinkel -= SYNC(FX(50.0));
				if (metalhead_s.GunWinkel < 0) metalhead_s.GunWinkel += SYNC(FX(50.0));
			}
			else metalhead_WinkelToPlayer(e);
			if (metalhead_s.ShotArt == 1)
			{
				e->yPos += SYNC(FX(1.5));
				e->xPos -= SYNC(FX(1.0));
			}
			metalhead_s.KieferPos = metalhead_sinr(metalhead_s.SinCount) * 50;

			switch (metalhead_s.Akt)
			{
			case GEGNER_OEFFNEN:
				metalhead_s.TurbineOff = metalhead_sinr(metalhead_s.SinCount) * 30;
				metalhead_s.SinCount += SYNC(FX(0.2));
				if (metalhead_s.SinCount >= METALHEAD_HALF_PI)
				{
					e->Destroyable = 1;
					{
						const int l = FINT(FX(85) + metalhead_s.TurbineOff);
						GegnerRect[e->GegnerArt] = (Box){ (int16_t)l, 210, (int16_t)(l + 98), 210 + 49 + 20 };
					}
					metalhead_s.Akt = GEGNER_SCHIESSEN;
					if (metalhead_s.Turbine_dran)
					{
						metalhead_s.ShotCount = metalhead_s.ShotArt == 0 ? GetRandom(4) + 4 : 15;
						metalhead_s.ShotDelay = FX(5.0);
					}
					else
					{
						metalhead_s.ShotCount = 100;
						metalhead_s.ShotDelay = FX(1.0);
					}
				}
				break;

			case GEGNER_SCHIESSEN:
				if (metalhead_s.TurbineOff < FX(30.0)) metalhead_s.TurbineOff += SYNC(FX(5.0));
				else metalhead_s.TurbineOff = FX(30.0);
				metalhead_s.ShotDelay -= SYNC(FX(1.0));
				if (metalhead_s.ShotDelay <= 0)
				{
					if (metalhead_s.ShotCount <= 0)
					{
						metalhead_s.Akt = GEGNER_SCHLIESSEN;
						e->Destroyable = !metalhead_s.Turbine_dran;
					}
					else
					{
						if (metalhead_s.Turbine_dran)
						{
							snd(SOUND_FIREBALL);
							metalhead_s.ShotDelay = FX(9.0);
							if (metalhead_s.ShotArt == 0)
							{
								proj_winkel = metalhead_s.GunWinkel + FX(90);
								const fx w = FX(450) - metalhead_s.GunWinkel;
								proj_push(e->xPos + FX(110) + metalhead_sind(w) * 70, e->yPos + FX(195) + metalhead_sind(w + FX(90)) * 70,
								          FIREBALL_BIG);
							}
							if (metalhead_s.ShotArt == 1)
							{
								snd(SOUND_FIREBALL);
								for (int a = -40; a <= 40; a += 40)
								{
									proj_winkel = metalhead_s.GunWinkel + FX(90 + a);
									const fx w = FX(450) - metalhead_s.GunWinkel;
									proj_push(e->xPos + FX(130) + metalhead_sind(w) * 70,
									          e->yPos + FX(200) + metalhead_sind(w + FX(90)) * 70, FIREBALL);
								}
								if (e->yPos > FX(V2 + 240)) metalhead_s.ShotCount = 0;
							}
							if (metalhead_s.ShotArt == 2)
							{
								snd(SOUND_FIREBALL);
								proj_push(e->xPos + FX(180), e->yPos + FX(195), FIREBALL_BOMB);
							}
							metalhead_s.TurbineOff = FX(15.0);
						}
						else
						{
							metalhead_s.ShotDelay = FX(0.5);
							if (metalhead_s.ShotArt == 1)
								proj_push(e->xPos + FX(95), e->yPos + FX(185) + FX(GetRandom(5)), WALKERFIRE);
							else if (metalhead_s.ShotCount % 12 == 0)
							{
								proj_push(e->xPos + FX(100), e->yPos + FX(213) + FX(GetRandom(5)), PFLANZESHOT);
								part_push(e->xPos + FX(80), e->yPos + FX(150), EXPLOSIONFLARE);
								part_push(e->xPos + FX(80), e->yPos + FX(150), EXPLOSIONFLARE);
							}
						}
						metalhead_s.ShotCount--;
					}
				}
				break;

			case GEGNER_SCHLIESSEN:
				metalhead_s.TurbineOff = metalhead_sinr(metalhead_s.SinCount) * 30;
				metalhead_s.SinCount -= SYNC(FX(0.2));
				if (metalhead_s.SinCount <= 0)
				{
					const int n = e->Energy < FX(2000) ? 4 : 3;
					int j = GetRandom(n);
					while (j == metalhead_s.ShotArt) j = GetRandom(n);
					metalhead_s.ShotArt = j;
					if (j == 2)
					{
						metalhead_s.Akt = GEGNER_OEFFNEN;
						metalhead_MoveToNewPoint(e, FX(V1 + 10), FX(V2 + 0), GEGNER_SCHIESSEN);
					}
					else if (j == 1)
					{
						metalhead_s.Akt = GEGNER_OEFFNEN;
						metalhead_MoveToNewPoint(e, FX(V1 + 130), FX(V2 + 0), GEGNER_SCHIESSEN);
					}
					else if (j == 0)
					{
						metalhead_s.Akt = GEGNER_OEFFNEN;
						metalhead_MoveToNewPoint(e, FX(V1 + 70), FX(V2 + 80), GEGNER_SCHIESSEN);
					}
					else
					{
						e->ySpeed = FX(50.0);
						e->Handlung = GEGNER_CRUSHEN2;
						snd(SOUND_STONEFALL);
						e->yAcc = FX(10.0);
						metalhead_s.Moving = 0;
						metalhead_s.Akt = GEGNER_OEFFNEN;
						metalhead_s.SinCount = 0;
					}
				}
				break;
			}
			/* fall through */

		case GEGNER_STEHEN:
			/* (Akt never is SK_BANGEN / SK_AUSGANG here: Hurrican's FIXME) */
			if (metalhead_s.Akt == METALHEAD_SK_BANGEN)
			{
				if (metalhead_s.KieferPos < FX(60.0)) metalhead_s.KieferPos += SYNC(FX(5.0));
				else metalhead_s.KieferPos = FX(60.0);
			}
			if (metalhead_s.Akt == METALHEAD_SK_AUSGANG)
			{
				if (metalhead_s.KieferPos > 0) metalhead_s.KieferPos -= SYNC(FX(5.0));
				else metalhead_s.KieferPos = 0;
			}
			break;

		case GEGNER_EXPLODIEREN:
			e->Energy = FX(100);
			metalhead_s.Eye_Alpha -= SYNC(FX(0.3));
			while (metalhead_s.Eye_Alpha < 0) metalhead_s.Eye_Alpha += METALHEAD_TWO_PI;
			e->AnimCount -= SYNC(FX(1.0));
			/* one more vertebra blows off */
			if (e->AnimCount < 0 && e->AnimEnde >= 0 && e->AnimEnde < METALHEAD_MAXWIRBEL)
			{
				const fx hx = metalhead_s.Hals[e->AnimEnde].x, hy = metalhead_s.Hals[e->AnimEnde].y;
				part_push(hx, hy, HALSWIRBEL);
				part_push(hx - FX(20), hy - metalhead_s.Hals[e->AnimEnde].w / 3, EXPLOSION_MEDIUM2);
				for (i = 0; i < 30; i++) part_push(hx + FX(GetRandom(20) - 20), hy + FX(GetRandom(20) - 20), FUNKE);
				snd(SOUND_EXPLOSION1);
				e->AnimCount = FX(2.5);
				e->AnimEnde--;
				/* the flying head blows up when 5 vertebrae are gone */
				if (e->AnimEnde == METALHEAD_MAXWIRBEL - 5)
				{
					for (i = 0; i < 20; i++)
						part_push(e->xPos + FX(10) + FX(GetRandom(160)), e->yPos + FX(20) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
					for (i = 0; i < 5; i++)
						part_push(e->xPos + FX(GetRandom(160)), e->yPos + FX(40) + FX(GetRandom(80)), EXPLOSION_BIG);
					for (i = 0; i < 40; i++)
						part_push(e->xPos + FX(GetRandom(180)), e->yPos + FX(GetRandom(140)), SCHROTT1);
					for (i = 0; i < 10; i++)
						part_push(e->xPos + FX(30) + FX(GetRandom(160)), e->yPos + FX(50) + FX(GetRandom(80)), SPLITTER);
					for (i = 0; i < 100; i++)
						part_push(e->xPos + FX(30) + FX(GetRandom(160)), e->yPos + FX(50) + FX(GetRandom(80)), SMOKE3);
					snd(SOUND_EXPLOSION2);
				}
				if (e->AnimEnde <= 0) e->Energy = 0;
			}
			/* (Hurrican's DoDraw: the head gone, it is parked at the screen's corner) */
			if (e->AnimEnde <= METALHEAD_MAXWIRBEL - 5)
			{
				e->xSpeed = 0;
				e->ySpeed = 0;
				e->xPos = cam_xoff();
				e->yPos = cam_yoff();
			}
			break;

		default:
			break;
		}

	/* the player touching the skull */
	{
		static const Box r = { 0, 0, 110, 140 };
		if (e->Handlung != GEGNER_EXPLODIEREN &&
		    SpriteCollision(e->xPos + FX(50), e->yPos + FX(130), &r, pl.xpos, pl.ypos, &pl.CollideRect))
		{
			/* sucked in: much damage at once and the sucking ends */
			if (e->Handlung == GEGNER_SPECIAL2 && pl.Handlung != PA_RADELN && pl.Handlung != PA_RADELN_FALL)
			{
				DamagePlayer(FX(500), 0);
				e->AnimPhase = 1600;
			}
			else DamagePlayer(SYNC(FX(4.0)), 0);
		}
	}

	if (e->Handlung != GEGNER_EXPLODIEREN) metalhead_calc_hals(e);
}

static void metalhead_explode(Ent *e)
{
	snd(SOUND_EXPLOSION2);
	pl.Score += 12500;
	/* ScrolltoPlayeAfterBoss: back to the player, the stage music again */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(METALHEAD) = { metalhead_init, metalhead_ki, metalhead_explode, metalhead_draw, metalhead_load };
