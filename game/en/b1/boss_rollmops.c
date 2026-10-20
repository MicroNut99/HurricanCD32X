/*
 * ROLLMOPS - the rolling snow ball boss of the ice level (bosses/Boss_Rollmops.cpp): hops in,
 * shoots its hook to the ceiling and swings on its chain (the chain links, KETTENGLIED, take the
 * hits for it), drops off, rolls left and right, jumps, opens up and shoots icicles with its gun,
 * spins and shoots all round.
 * Own members: the static struct rollmops_s (one Rollmops at a time): GunWinkel, SmokeCount,
 * ShotDelay, ShotCount, HookX, HookY, Schwung, SchwungDir, Delay, HitsToTake, the chain links'
 * slots. RoundShot's static WinkelCount and AUSSPUCKEN's static shot are file statics.
 * The chain links are KETTENGLIED objects (group E3's Gegner_Kettenglied): pushed by the
 * constructor at (0, 0) and placed along the chain each step while it swings (Hurrican does that
 * in DoDraw; here at the end of DoKI, the same step); their pParent is this Rollmops (the only
 * ROLLMOPS in the game). They are kept from despawning (NoDespawn) as Hurrican never removes them.
 * Drawing: no rotation (the gun is drawn upright), the snow ball (INIT / STEHEN) unscaled at its
 * natural size, in the middle of where Hurrican draws it scaled. Left out: the sound pitches / volumes, ShakeScreen.
 */
#define ROLLMOPS_KETTEN 20
#define ROLLMOPS_PI      FX(3.14159265)
#define ROLLMOPS_TWO_PI  FX(6.28318531)

static struct {
	fx GunWinkel, SmokeCount, ShotDelay, ShotCount, HookX, HookY, Schwung, SchwungDir, Delay, HitsToTake;
	int16_t kette[ROLLMOPS_KETTEN];
} rollmops_s;
static fx rollmops_WinkelCount;       /* RoundShot's static */
static int rollmops_shot;             /* AUSSPUCKEN's static */
static Pic rollmops_Rollen, rollmops_Aufklappen, rollmops_Gun;

/* sin / cos of an angle in radians (fx), interpolated between whole degrees */
static fx rollmops_sinr(fx rad)
{
	const fx deg = FMUL(rad, FX(57.2957795));
	int d = deg >> 8;
	const int frac = deg & 255;
	const int a = sin_d(d), b = sin_d(d + 1);
	return a + (((b - a) * frac) >> 8);
}
static fx rollmops_cosr(fx rad) { return rollmops_sinr(rad + FX(1.57079633)); }

/* atan(y / x) in degrees (fx), as C's atanf of the quotient: -90..90 */
static fx rollmops_atan_q(int y, int x)
{
	int a = atan2_d(y, x);
	if (a > 90) a -= 180;
	if (a < -90) a += 180;
	return FX(a);
}

static Ent *rollmops_link(int i)
{
	const int k = rollmops_s.kette[i];
	if (k < 0 || k >= MAX_LIVE || ents[k].GegnerArt != KETTENGLIED) return 0;
	return &ents[k];
}

static void rollmops_init(Ent *e)
{
	e->Handlung = GEGNER_EINFLIEGEN;
	e->Energy = FX(4000);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	memset(&rollmops_s, 0, sizeof rollmops_s);
	rollmops_shot = 0;
	for (int i = 0; i < ROLLMOPS_KETTEN; i++)
	{
		Ent *k = gegner_push(0, 0, KETTENGLIED, 0, 0);
		if (k->GegnerArt == KETTENGLIED) { k->NoDespawn = 1; rollmops_s.kette[i] = (int16_t)(k - ents); }
		else rollmops_s.kette[i] = -1;
	}
}

static void rollmops_load(void)
{
	rollmops_Rollen = pic("rollmops_rollen.png");
	rollmops_Aufklappen = pic("rollmops_aufklappen.png");
	rollmops_Gun = pic("rollmops_gun.png");
}

/* the angle to the player */
static void rollmops_CalcGunWinkel(Ent *e)
{
	int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + FX(60)));
	const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(50)));
	if (ydiv == 0) ydiv = 1;
	fx neww = xdiv == 0 ? (ydiv > 0 ? FX(90) : FX(-90)) : rollmops_atan_q(ydiv, xdiv);
	if (e->xPos >= FX(e->Value1 + 320)) neww = -neww;
	if (neww < FX(-30)) neww = FX(-30);
	if (neww > FX(90)) neww = FX(90);
	if (rollmops_s.GunWinkel < neww) rollmops_s.GunWinkel += SYNC(FX(5.0));
	if (rollmops_s.GunWinkel > neww) rollmops_s.GunWinkel -= SYNC(FX(5.0));
}

static void rollmops_draw(Ent *e)
{
	int mirrored = 0;
	switch (e->Handlung)
	{
	case GEGNER_AUSSPUCKEN:
	case GEGNER_LAUFEN:
	case GEGNER_LAUFEN_LINKS:
	case GEGNER_LAUFEN_RECHTS:
	case GEGNER_LAUFEN_LINKS2:
	case GEGNER_LAUFEN_RECHTS2:
	case GEGNER_SPECIAL:
	case GEGNER_SPECIAL2:
	case GEGNER_SPECIAL3:
	case GEGNER_WARTEN:
	case GEGNER_EXPLODIEREN:
		draw_sprite(&rollmops_Rollen, e->AnimPhase, e->xPos, e->yPos, 0);
		break;
	case GEGNER_INIT:
	case GEGNER_STEHEN:
		/* (Hurrican scales the 80 x 80 snow ball to 120 x 120: drawn unscaled in its middle) */
		draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos + FX(16 + 20), e->yPos + FX(20), 0);
		break;
	case GEGNER_OEFFNEN:
	case GEGNER_SCHLIESSEN:
		mirrored = e->xPos < FX(e->Value1 + 320);
		draw_sprite(&rollmops_Aufklappen, e->AnimPhase, e->xPos, e->yPos, mirrored);
		break;
	case GEGNER_VERFOLGEN:
	{
		mirrored = e->xPos < FX(e->Value1 + 320);
		const fx xoff = mirrored ? FX(65) : 0;
		draw_sprite(&rollmops_Gun, 0, e->xPos + FX(6) + xoff, e->yPos + FX(38), mirrored);
		draw_sprite(&rollmops_Aufklappen, 8, e->xPos, e->yPos, mirrored);
		break;
	}
	}
}

/* open up to shoot / close again */
static void rollmops_GunAusfahren(Ent *e, int auf)
{
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->xAcc = 0;
	e->yAcc = 0;
	snd(SOUND_STEAM);
	if (auf)
	{
		e->Handlung = GEGNER_OEFFNEN;
		e->AnimPhase = 0;
		e->AnimStart = 0;
		e->AnimEnde = 9;
		rollmops_s.HitsToTake = e->Energy;
	}
	else
	{
		e->Handlung = GEGNER_SCHLIESSEN;
		e->AnimPhase = 7;
		e->AnimStart = 7;
		e->AnimEnde = 0;
		rollmops_s.Delay = 0;
	}
	e->AnimCount = 0;
	e->AnimSpeed = FX(1.2);
}

/* bouncing off the floor */
#define ROLLMOPS_MUL_DEFAULT FX(-0.6)
static void rollmops_Abhopsen(Ent *e, fx mul)
{
	const fx floor_y = FX(e->Value2 + RENDERHEIGHT - 100 - 40);
	if (e->ySpeed > 0 && e->yPos > floor_y)
	{
		e->yPos = floor_y;
		snd(SOUND_LANDEN);
		if (mul != ROLLMOPS_MUL_DEFAULT) snd(SOUND_KLONG);
		e->ySpeed = FMUL(e->ySpeed, mul);
		for (int i = 0; i < 10; i++) part_push(e->xPos + FX(10) + FX(GetRandom(80)), e->yPos + FX(100), SNOWFLUSH);
		if (e->ySpeed > FX(-1.0))
		{
			e->ySpeed = 0;
			e->yAcc = 0;
		}
	}
}

/* shots all round: single = one after the other, else all at once */
static void rollmops_RoundShot(Ent *e, int single)
{
	if (single)
	{
		proj_winkel = rollmops_WinkelCount;
		const int w = FINT(rollmops_WinkelCount);
		proj_push(e->xPos + FX(50) + sin_d(w) * 40, e->yPos + FX(50) - cos_d(w) * 40, EISZAPFENSHOT);
		rollmops_WinkelCount += FX(30);
		if (rollmops_WinkelCount >= FX(360)) rollmops_WinkelCount = 0;
	}
	else
		for (rollmops_WinkelCount = FX(GetRandom(30)); rollmops_WinkelCount < FX(360); rollmops_WinkelCount += FX(30))
		{
			proj_winkel = rollmops_WinkelCount;
			const int w = FINT(rollmops_WinkelCount);
			proj_push(e->xPos + FX(50) + sin_d(w) * 50, e->yPos + FX(50) - cos_d(w) * 50, EISZAPFENSHOT);
		}
	snd(SOUND_STONEFALL);
}

#ifndef OJ32X_MARS
/* PC check: HB_B1LOG=1 prints the state every half second */
static void rollmops_log(const Ent *e)
{
	static int n;
	if (getenv("HB_B1LOG") && n++ % 30 == 0)
		fprintf(stderr, "rollmops: H %d E %d pos %d,%d speed %d,%d anim %d dest %d cam %d,%d state %d player %d,%d\n", e->Handlung,
		        FINT(e->Energy), FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->AnimPhase, e->Destroyable,
		        FINT(cam_xoff()), FINT(cam_yoff()), tile_zustand(), FINT(pl.xpos), FINT(pl.ypos));
}
#else
#define rollmops_log(e) ((void)0)
#endif

static void rollmops_ki(Ent *e)
{
	rollmops_log(e);
	int i;
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN) boss_bar(e->Energy, FX(4000));

	/* lock the screen on the Rollmops as soon as it is seen */
	if (tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		e->xPos = FX(e->Value1 + RENDERWIDTH);   /* out of the screen */
		music_stop();                            /* (fades out the stage music) */
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100.0));
	else e->DamageTaken = 0;

	if (rollmops_s.Delay > 0) rollmops_s.Delay -= SYNC(FX(1.0));
	else
		switch (e->Handlung)
		{
		case GEGNER_EINFLIEGEN:                 /* wait for the camera, then hop in */
			if (tile_zustand() == TS_LOCKED)
			{
				e->xSpeed = FX(-10.0);
				e->yAcc = FX(4.0);
				e->Handlung = GEGNER_INIT;
				e->AnimEnde = 19;
				e->AnimSpeed = FX(0.25);
				music_boss();
			}
			break;

		case GEGNER_INIT:                       /* the snow ball hops in */
			SimpleAnimation(e, 0);
			e->AnimSpeed += SYNC(FX(0.02));
			rollmops_Abhopsen(e, ROLLMOPS_MUL_DEFAULT);
			if (e->ySpeed == 0)
			{
				e->xAcc = FX(1.0);
				e->AnimSpeed += SYNC(FX(0.02));
				if (e->xSpeed >= 0)
				{
					e->xAcc = 0;
					e->Handlung = GEGNER_STEHEN;
					rollmops_s.HookX = e->xPos + FX(65 - 9);
					rollmops_s.HookY = e->yPos;
					rollmops_s.Schwung = 0;
					rollmops_s.SchwungDir = 0;
					rollmops_s.ShotDelay = FX(5.0);
				}
			}
			break;

		case GEGNER_AUSSPUCKEN:                 /* the hook up, swinging */
			if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1.0));
			rollmops_s.HookY -= SYNC(FX(40.0));
			if (rollmops_s.HookY < FX(e->Value2 + 20))
			{
				if (e->yPos < FX(e->Value2 + 130) && !rollmops_shot)
				{
					rollmops_RoundShot(e, 0);
					rollmops_shot = 1;
				}
				rollmops_s.HookY = FX(e->Value2 + 20);
				e->xPos = FX(e->Value1 + 320 - 65) + rollmops_sinr(rollmops_s.Schwung) * 300;
				e->yPos = FX(e->Value2 - 500) + rollmops_cosr(FMUL(rollmops_s.Schwung, FX(0.75))) * 830;
				rollmops_s.SchwungDir += SYNC(FX(0.16));
				if (rollmops_s.SchwungDir > ROLLMOPS_TWO_PI) rollmops_s.SchwungDir = 0;
				rollmops_s.Schwung = rollmops_sinr(rollmops_s.SchwungDir);
				{
					int a = -FINT(FX(e->Value1 + RENDERWIDTH) - e->xPos) / 200;
					if (a < 0) a += 10;
					e->AnimPhase = (int16_t)a;
				}
				if (((rollmops_s.SchwungDir > ROLLMOPS_PI - FX(0.2) && rollmops_s.SchwungDir < ROLLMOPS_PI + FX(0.2)) ||
				     (rollmops_s.SchwungDir > ROLLMOPS_TWO_PI - FX(0.2) && rollmops_s.SchwungDir < ROLLMOPS_TWO_PI + FX(0.2))) &&
				    GetRandom(2) == 0)
				{
					rollmops_shot = 0;
					part_push(e->xPos + FX(60) + FX(GetRandom(20)), e->yPos + FX(120), LONGFUNKE);
					part_push(e->xPos + FX(60) + FX(GetRandom(20)), e->yPos + FX(100), FUNKE);
					if (!snd_playing(SOUND_KLONG)) snd(SOUND_KLONG);
				}
			}
			/* shot off? */
			if (e->Energy < FX(2000))
			{
				e->Handlung = GEGNER_WARTEN;
				e->ySpeed = FX(-20.0);
				e->yAcc = FX(6.0);
				e->AnimSpeed = FX(10.0);
				for (i = 0; i < ROLLMOPS_KETTEN; i++)
				{
					Ent *k = rollmops_link(i);
					if (k) k->Handlung = GEGNER_SPECIAL;     /* the chain flies apart */
				}
			}
			break;

		case GEGNER_LAUFEN_LINKS:
		case GEGNER_LAUFEN_LINKS2:
			if (e->xSpeed < FX(-60.0)) e->xSpeed = FX(-60.0);
			if (e->xPos + FX(65) < FX(e->Value1 + 135)) e->xAcc = FX(19.0);
			if (e->Handlung == GEGNER_LAUFEN_LINKS2 && e->ySpeed == 0)
			{
				if (e->xPos + FX(65) < FX(e->Value1 + 380)) e->xAcc = FX(23.0);
				if (e->xSpeed > 0)
				{
					e->xSpeed = 0;
					e->ySpeed = 0;
					e->xAcc = 0;
					e->yAcc = FX(10.0);
					e->Handlung = GEGNER_SPECIAL2;
					rollmops_s.ShotCount = FX(2.0);
					e->ySpeed = FX(-77.0);
					break;
				}
			}
			e->AnimCount -= SYNC(e->xSpeed / 10);
			if (e->AnimCount > e->AnimSpeed) { e->AnimCount = 0; e->AnimPhase++; }
			if (e->AnimPhase > e->AnimEnde) e->AnimPhase = 0;
			if (e->xSpeed > 0)
			{
				e->Handlung = GEGNER_LAUFEN_RECHTS;
				rollmops_s.ShotDelay -= FX(1.0);
				if (FINT(rollmops_s.ShotDelay) % 3 == 0)
				{
					e->xSpeed = FX(12.0);
					e->ySpeed = FX(-60.0);
					e->yAcc = FX(15.0);
				}
				if (rollmops_s.ShotDelay <= 0) rollmops_GunAusfahren(e, 1);
			}
			rollmops_Abhopsen(e, FX(-0.4));
			break;

		case GEGNER_LAUFEN_RECHTS:
		case GEGNER_LAUFEN_RECHTS2:
			if (e->xSpeed > FX(60.0)) e->xSpeed = FX(60.0);
			if (e->xPos + FX(65) > FX(e->Value1 + 505)) e->xAcc = FX(-19.0);
			if (e->Handlung == GEGNER_LAUFEN_RECHTS2 && e->ySpeed == 0)
			{
				if (e->xPos + FX(65) > FX(e->Value1 + 300)) e->xAcc = FX(-23.0);
				if (e->xSpeed < 0)
				{
					e->xSpeed = 0;
					e->ySpeed = 0;
					e->xAcc = 0;
					e->yAcc = FX(8.0);
					e->Handlung = GEGNER_SPECIAL3;
					rollmops_s.ShotCount = FX(2.0);
					e->ySpeed = FX(-65.0);
					break;
				}
			}
			e->AnimCount += SYNC(e->xSpeed / 10);
			if (e->AnimCount > e->AnimSpeed) { e->AnimCount = 0; e->AnimPhase--; }
			if (e->AnimPhase < 0) e->AnimPhase = e->AnimEnde;
			if (e->xSpeed < 0)
			{
				e->Handlung = GEGNER_LAUFEN_LINKS;
				rollmops_s.ShotDelay -= FX(1.0);
				if (FINT(rollmops_s.ShotDelay) % 3 == 0)
				{
					e->xSpeed = FX(-16.0);
					e->ySpeed = FX(-60.0);
					e->yAcc = FX(12.0);
				}
				if (rollmops_s.ShotDelay <= 0) rollmops_GunAusfahren(e, 1);
			}
			rollmops_Abhopsen(e, FX(-0.4));
			break;

		case GEGNER_STEHEN:
			rollmops_s.ShotDelay -= SYNC(FX(1.0));
			if (rollmops_s.ShotDelay <= 0)
			{
				rollmops_s.ShotDelay = 0;
				e->Handlung = GEGNER_AUSSPUCKEN;
				e->AnimPhase = 0;
				e->AnimStart = 0;
				e->AnimEnde = 10;
				e->AnimSpeed = FX(0.5);
				snd(SOUND_LANDEN);
				snd(SOUND_STEAM);
				for (i = 0; i < 30; i++)
					part_push(e->xPos + FX(10) + FX(GetRandom(80)), e->yPos + FX(10) + FX(GetRandom(80)), SNOWFLUSH);
				for (i = 0; i < 8; i++)
					proj_push(e->xPos + FX(10) + FX(GetRandom(80)), e->yPos + FX(10) + FX(GetRandom(80)), SNOWBOMBSMALL);
			}
			break;

		case GEGNER_LAUFEN:
			rollmops_Abhopsen(e, FX(-0.4));
			SimpleAnimation(e, 0);
			break;

		case GEGNER_WARTEN:
			rollmops_Abhopsen(e, FX(-0.4));
			if (e->ySpeed == 0) e->AnimSpeed -= SYNC(FX(1.0));
			if (e->AnimSpeed <= 0)
			{
				rollmops_s.ShotDelay = FX(5.0);
				e->Handlung = GEGNER_LAUFEN_LINKS;
				e->xSpeed = FX(-60.0);
				e->xAcc = 0;
				e->AnimPhase = 0;
				e->AnimEnde = 9;
				e->AnimStart = 0;
				e->AnimSpeed = FX(1.0);
			}
			break;

		case GEGNER_OEFFNEN:
		case GEGNER_SCHLIESSEN:
		{
			int fertig = 0;
			e->AnimCount += SYNC(FX(1.0));
			if (e->AnimCount > e->AnimSpeed)
			{
				e->AnimCount = 0;
				if (e->Handlung == GEGNER_OEFFNEN)
				{
					e->AnimPhase++;
					if (e->AnimPhase >= e->AnimEnde - 1)
					{
						fertig = 1;
						rollmops_s.Delay = FX(10.0);
						rollmops_s.GunWinkel = 0;
					}
				}
				else
				{
					e->AnimPhase--;
					if (e->AnimPhase <= e->AnimEnde) fertig = 1;
				}
			}
			if (fertig)
			{
				e->AnimPhase = e->AnimEnde;
				if (e->Handlung == GEGNER_OEFFNEN)      /* shoot */
				{
					rollmops_s.ShotCount = FX(GetRandom(15) + 15);
					e->Handlung = GEGNER_VERFOLGEN;
					e->Destroyable = 1;
				}
				else                                    /* roll again */
				{
					e->Destroyable = 0;
					rollmops_s.ShotDelay = FX(5.0);
					e->xAcc = 0;
					e->AnimPhase = 0;
					e->AnimEnde = 9;
					e->AnimStart = 0;
					e->AnimSpeed = FX(1.0);
					if (e->xPos > FX(e->Value1 + 320))
					{
						e->Handlung = e->Energy < FX(1000) ? GEGNER_LAUFEN_LINKS2 : GEGNER_LAUFEN_LINKS;
						e->xSpeed = FX(-40.0);
					}
					else
					{
						e->Handlung = e->Energy < FX(1000) ? GEGNER_LAUFEN_RECHTS2 : GEGNER_LAUFEN_RECHTS;
						e->xSpeed = FX(40.0);
					}
				}
			}
			break;
		}

		case GEGNER_VERFOLGEN:                  /* aim with the gun */
			rollmops_CalcGunWinkel(e);
			rollmops_s.ShotDelay -= SYNC(FX(1.0));
			if (rollmops_s.ShotDelay < 0)
			{
				snd(SOUND_STONEFALL);
				fx xoff = 0;
				if (e->xPos > FX(e->Value1 + 320)) proj_winkel = FX(270) - rollmops_s.GunWinkel;
				else
				{
					proj_winkel = rollmops_s.GunWinkel + FX(90);
					xoff = FX(-70);
				}
				const int w = FINT(proj_winkel);
				part_push(e->xPos + xoff + FX(85) + sin_d(w) * 105, e->yPos + FX(46) - cos_d(w) * 90, SNOWFLUSH);
				proj_push(e->xPos + xoff + FX(90) + sin_d(w) * 105, e->yPos + FX(46) - cos_d(w) * 90, EISZAPFENSHOT);
				rollmops_s.ShotDelay = FX(3.0);
				rollmops_s.ShotCount -= FX(1.0);
				if (rollmops_s.ShotCount <= 0 || (rollmops_s.HitsToTake - e->Energy) > FX(100))
					rollmops_GunAusfahren(e, 0);
			}
			break;

		case GEGNER_SPECIAL2:
			rollmops_Abhopsen(e, FX(-0.3));
			if (e->ySpeed == 0)
			{
				rollmops_s.ShotDelay = FX(3.0);
				e->Handlung = GEGNER_LAUFEN_LINKS;
				e->xSpeed = FX(-30.0);
				e->xAcc = FX(-10.0);
			}
			if (e->yPos < FX(e->Value2 + 260))
			{
				rollmops_s.ShotCount -= SYNC(FX(1.0));
				if (rollmops_s.ShotCount <= 0)
				{
					rollmops_RoundShot(e, 0);
					rollmops_s.ShotCount = FX(4.0);
				}
			}
			break;

		case GEGNER_SPECIAL3:                   /* jump up, spin and shoot all round */
			rollmops_Abhopsen(e, FX(-0.3));
			if (e->ySpeed == 0)
			{
				rollmops_s.ShotDelay = FX(3.0);
				e->Handlung = GEGNER_LAUFEN_RECHTS;
				e->xSpeed = FX(30.0);
				e->xAcc = FX(10.0);
			}
			if (e->yPos < FX(e->Value2 + 250))
			{
				rollmops_s.ShotCount -= SYNC(FX(1.0));
				if (rollmops_s.ShotCount <= 0)
				{
					rollmops_RoundShot(e, 1);
					rollmops_s.ShotCount = FX(0.5);
				}
			}
			break;

		case GEGNER_EXPLODIEREN:
			rollmops_Abhopsen(e, FX(-0.3));
			e->AnimCount += SYNC(FX(1.0));
			while (e->AnimCount > e->AnimSpeed)
			{
				snd(SOUND_EXPLOSION3);
				part_push(e->xPos - FX(20) + FX(GetRandom(140)), e->yPos - FX(20) + FX(GetRandom(140)),
				          EXPLOSION_MEDIUM2 + GetRandom(1));
				if (GetRandom(3) == 0) part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(100)), SPLITTER);
				e->AnimCount -= e->AnimSpeed;
				e->AnimSpeed -= (e->AnimPhase & 1) ? 3 : 2;      /* 0.01 (2.56 in fx) */
				e->AnimPhase++;
				if (e->AnimPhase > e->AnimEnde) e->AnimPhase = 0;
				if (e->AnimSpeed <= FX(0.1)) e->Energy = FX(-500);
			}
			break;

		default:
			break;
		}

	/* snow while rolling */
	if (e->Handlung == GEGNER_LAUFEN_RECHTS || e->Handlung == GEGNER_LAUFEN_LINKS || e->Handlung == GEGNER_LAUFEN ||
	    e->Handlung == GEGNER_SPECIAL)
	{
		if (e->ySpeed == 0) rollmops_s.SmokeCount -= SYNC(FX(1.0));
		if (rollmops_s.SmokeCount < 0)
		{
			part_push(e->xPos + FX(10) + FX(GetRandom(80)), e->yPos + FX(80) + FX(GetRandom(10)), SNOWFLUSH);
			rollmops_s.SmokeCount = FX(0.8);
		}
	}

	/* the player touching the boss */
	GegnerRect[e->GegnerArt] = (Box){ 20, 20, 80, 80 };
	TestDamagePlayers(e, SYNC(FX(20.0)), 0);

	/* no energy left: explode */
	if (e->Energy <= 0 && e->Energy > FX(-500) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = 0;
		e->ySpeed = 0;
		e->xAcc = 0;
		e->Energy = FX(1.0);
		e->Destroyable = 0;
		e->AnimSpeed = FX(1.0);
		rollmops_s.ShotDelay = FX(1.5);
		e->AnimPhase = 0;
		e->AnimEnde = 9;
		e->AnimStart = 0;
		music_stop();                       /* (fades out the boss music) */
	}

	/* the chain from the hook to the ball (Hurrican's DoDraw) */
	if (e->Handlung == GEGNER_AUSSPUCKEN)
	{
		const fx x = ((e->xPos + FX(65 - 9)) - rollmops_s.HookX) / ROLLMOPS_KETTEN;
		const fx y = ((e->yPos + FX(65)) - rollmops_s.HookY) / ROLLMOPS_KETTEN;
		const fx w = FX(360) - FMUL(rollmops_s.Schwung, FX(57.2957795));
		for (i = 0; i < ROLLMOPS_KETTEN; i++)
		{
			Ent *k = rollmops_link(i);
			if (!k) continue;
			k->xPos = rollmops_s.HookX + x * i;
			k->yPos = rollmops_s.HookY + y * i;
			k->AnimCount = w;
		}
	}
}

static void rollmops_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION2);
	for (i = 0; i < 5; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(140)), e->yPos - FX(20) + FX(GetRandom(140)), EXPLOSION_TRACE);
	for (i = 0; i < 150; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(140)), e->yPos - FX(20) + FX(GetRandom(140)), WATERFLUSH_HIGH);
	shake_screen(5);
	pl.Score += 5000;
	/* ScrolltoPlayeAfterBoss: back to the player, the stage music again */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(ROLLMOPS) = { rollmops_init, rollmops_ki, rollmops_explode, rollmops_draw, rollmops_load };
