/*
 * SKELETOR - the last boss, Robo-Skeletor (bosses/Boss_Skeletor.cpp).
 * Drops in, laughs, then: gatling fire at the player, grenades, a big jump with a fan of five
 * fireballs, a run over the screen firing rockets. When beaten it bursts and leaves its skull
 * (SKULL, which goes on with the ending).
 * Own members: i[0] = ShotCount, i[1] = AnimOffset, i[2] = LastHandlung, i[3] = bit 0 HasLaughed,
 * bit 1 DrawNow; f[0] = SmokeDelay (unused, as in Hurrican), f[1] = ShotDelay, f[2] = GunWinkel
 * (fx degrees).
 * Drawing: the body; the gun's flame (the plain frame, not rotated, not additive). The four
 * after-images of the run (SPECIAL2) are left out (no extra copies on the 32X).
 * Left out: screen shaking, the music fades (stop / start instead).
 */
static Pic P_sk_flamme;

static void sk_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void sk_init(Ent *e)
{
	e->i[0] = 0;
	e->i[2] = -1;
	e->f[0] = e->f[1] = e->f[2] = 0;
	e->i[3] = 0;
	e->AnimPhase = 10;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = 1;
	e->Energy = FX(7000);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

static void sk_laugh(Ent *e)
{
	e->AnimPhase = 2;
	snd(SOUND_LAUGH);
	e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
	e->Handlung = GEGNER_SPECIAL;
	e->AnimCount = FX(20);
}

/* the gun's angle to the player: 90 + atan(dy / dx), 50 .. 120 */
static void sk_calcgunwinkel(Ent *e)
{
	int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + FX(100)));
	const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(60)));
	if (ydiv == 0) ydiv = 1;
	int a = atan2_d(ydiv, xdiv);
	if (a > 90) a -= 180;                /* atan, not atan2: -90 .. 90 */
	if (a < -90) a += 180;
	fx w = FX(90 + a);
	if (w < FX(50)) w = FX(50);
	if (w > FX(120)) w = FX(120);
	e->f[2] = w;
}

static void sk_draw(Ent *e)
{
	if (!(e->i[3] & 2)) return;
	if (e->Handlung == GEGNER_EXPLODIEREN) return;
	const int mirror = e->blick == 1;
	const fx yoff = e->AnimPhase >= 10 ? FX(10) : 0;
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos + yoff, mirror);
	/* the flame */
	if (e->Handlung == GEGNER_SCHIESSEN && e->f[1] > FX(0.5))
	{
		const int foff = mirror ? 87 : -120;
		draw_sprite(&P_sk_flamme, e->i[0] % 2, e->xPos + FX(foff), e->yPos + FX(60), !mirror);
	}
}

/* AnimPhase while it moves over the screen */
static void sk_runphase(Ent *e)
{
	if (e->xPos + FX(60) < FX(e->Value1 + 320))
	{
		const int a = FINT(e->xPos - cam_xoff()) / 56;
		e->blick = 1;
		e->AnimPhase = (int16_t)(10 + a);
	}
	else
	{
		const int a = FINT(e->xPos - cam_xoff() - FX(320) + FX(60)) / 56;
		e->blick = -1;
		e->AnimPhase = (int16_t)(15 - a);
	}
}

static void sk_ki(Ent *e)
{
	sk_calcgunwinkel(e);

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(7000));

	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		/* outside the screen */
		e->xPos = cam_xoff() + FX(100);
		e->yPos = cam_yoff() - FX(250);
		e->i[3] |= 2;
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_SPECIAL:                 /* waits after laughing */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0) e->Handlung = GEGNER_STEHEN;
		break;

	case GEGNER_CRUSHENERHOLEN:
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount > FX(10) && e->AnimPhase == 0) e->AnimPhase = 1;
		if (e->AnimCount > FX(20) && e->AnimPhase == 1)
		{
			e->AnimPhase = 2;
			if (!(e->i[3] & 1)) snd(SOUND_LAUGH);
		}
		if (e->AnimCount > FX(25))
		{
			e->Handlung = GEGNER_STEHEN;
			if (!(e->i[3] & 1))
			{
				e->i[3] |= 1;
				music_boss();
			}
		}
		break;

	case GEGNER_STEHEN:
	{
		int j = GetRandom(4);
		while (j == e->i[2]) j = GetRandom(4);
		e->i[2] = j;
		switch (j)
		{
		case 0:
			e->f[1] = 0;
			e->i[0] = 40;
			e->Handlung = GEGNER_SCHIESSEN;
			e->i[1] = 0;
			break;
		case 1:
			e->f[1] = 0;
			e->i[0] = 20;
			e->Handlung = GEGNER_BOMBARDIEREN;
			e->i[1] = 0;
			break;
		case 2:
			e->f[1] = FX(3);
			e->i[0] = 1;
			e->Handlung = GEGNER_SPRINGEN;
			e->xSpeed = FX(25);
			e->ySpeed = FX(-60);
			e->yAcc = FX(6);
			break;
		case 3:
			e->Handlung = GEGNER_SPECIAL2;
			e->xSpeed = 0;
			e->xAcc = FX(2);
			e->f[1] = FX(9);
			e->i[0] = 5;
			break;
		}
		if (e->xPos > FX(e->Value1 + 320))
		{
			e->xSpeed = -e->xSpeed;
			e->xAcc = -e->xAcc;
		}
	} break;

	case GEGNER_SPECIAL2:                /* runs over the screen, rockets */
		if (e->i[0] > 0) e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			proj_winkel = FX(2 * e->i[0] + 8);
			snd(SOUND_ROCKET);
			proj_push(e->xPos + FX(50), e->yPos + FX(80), TURRIEXTRAWURST);
			e->f[1] = FX(3);
			e->i[0]--;
		}
		if ((e->xSpeed > FX(31) && e->xAcc > 0) || (e->xSpeed < FX(-31) && e->xAcc < 0)) e->xAcc = -e->xAcc;
		if ((e->xAcc > 0 && e->xSpeed < 0 && e->xSpeed > FX(-2)) || (e->xAcc < 0 && e->xSpeed > 0 && e->xSpeed < FX(2)))
			e->xAcc = 0;
		sk_runphase(e);
		/* arrived? */
		if (e->xPos < FX(e->Value1) + FX(20) || e->xPos > FX(e->Value1 + 500))
		{
			if (e->xPos < FX(e->Value1 + 50)) e->xPos = FX(e->Value1) + FX(20);
			else e->xPos = FX(e->Value1) + FX(500);
			e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
			sk_laugh(e);
		}
		break;

	case GEGNER_SPRINGEN:
		sk_runphase(e);
		/* the fan of five */
		if (e->i[0] == 1 && e->xPos + FX(62) > FX(e->Value1 + 300) && e->xPos + FX(62) < FX(e->Value1 + 340))
		{
			snd(SOUND_FIREBALL);
			e->i[0] = 0;
			for (int i = 0; i < 5; i++)
			{
				proj_winkel = FX(180 - 50 + i * 25);
				proj_push(e->xPos + FX(50), e->yPos + FX(70), FIREBALL_BIG);
			}
		}
		/* landed? */
		if (e->yPos > FX(e->Value2 + 250))
		{
			e->yPos = FX(e->Value2) + FX(250);
			e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
			e->AnimPhase = 0;
			e->Handlung = GEGNER_CRUSHENERHOLEN;
			e->AnimCount = 0;
			for (int i = 0; i < 10; i++)
				part_push(e->xPos + FX(GetRandom(100) - 20), e->yPos + FX(GetRandom(20) + 130), SMOKEBIG);
			for (int i = 0; i < 10; i++)
				part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(10) + 180), SPIDERSPLITTER);
			shake_screen(5);
			snd(SOUND_DOORSTOP);
		}
		break;

	case GEGNER_BOMBARDIEREN:            /* grenades */
		e->f[1] -= SYNC(FX(0.1));
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->i[1]++;
			if (e->i[1] % 2 == 0) e->AnimCount = FX(2);
			else
			{
				const int off = e->blick == -1 ? -95 : 0;
				e->AnimCount = FX(1.5);
				snd(SOUND_GRANATE);
				part_push(e->xPos + FX(50 + off), e->yPos + FX(35), EXPLOSIONFLARE);
				part_push(e->xPos + FX(50 + off), e->yPos + FX(35), EXPLOSIONFLARE);
				int w = 40 - e->AnimPhase * 4 - GetRandom(8);
				if (e->blick == -1) w += 1;
				if (w < 0) w = 0;
				proj_winkel = FX(w * e->blick);
				proj_push(e->xPos + FX(100 + off), e->yPos + FX(80), SKELETORGRANATE);
				e->i[0]--;
			}
			const int a = e->i[1] % 14;
			e->AnimPhase = (int16_t)(a < 8 ? a + 2 : 16 - a);
			if (e->i[1] > 42) e->i[1] = 0;
			if (e->i[0] < 0) sk_laugh(e);
		}
		break;

	case GEGNER_SCHIESSEN:               /* gatling */
		e->f[1] -= SYNC(FX(0.5));
		e->AnimPhase = e->f[1] > FX(0.5) ? 3 : 2;
		if (e->f[1] <= 0)
		{
			e->i[0]--;
			e->f[1] = FX(1);
			/* the shell */
			part_push(e->xPos + FX(e->blick == -1 ? 30 : 70), e->yPos + FX(75), BULLET_SKELETOR);
			snd(SOUND_GATLING);
			fx w = e->f[2] + FX(GetRandom(4) - 2);
			if (e->blick == -1) w += FX(180);
			proj_winkel = w;
			proj_push(e->xPos + FX(e->blick == -1 ? 20 : 100), e->yPos + FX(75), SKELETOR_SHOT);
			if (e->i[0] < 0) sk_laugh(e);
		}
		break;

	case GEGNER_NOTVISIBLE:
		if (tile_zustand() == TS_LOCKED)
		{
			snd(SOUND_STONEFALL);
			e->Handlung = GEGNER_EINFLIEGEN;
			e->xSpeed = 0;
			e->ySpeed = FX(80);
			e->Destroyable = 1;
		}
		break;

	case GEGNER_EINFLIEGEN:              /* drops onto the scrap heap */
		e->blick = 1;
		if (e->yPos > FX(e->Value2 + 250))
		{
			e->yPos = FX(e->Value2) + FX(250);
			e->AnimPhase = 0;
			e->ySpeed = 0;
			e->Handlung = GEGNER_CRUSHENERHOLEN;
			e->AnimCount = 0;
			for (int i = 0; i < 50; i++)
				part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(150) + 30), SMOKEBIG);
			for (int i = 0; i < 20; i++)
				part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(10) + 180), SPIDERSPLITTER);
			shake_screen(5);
			snd(SOUND_DOORSTOP);
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0) e->Energy = 0;
		break;

	default:
		break;
	}

	/* beaten (Hurrican: Energy < 0; here <= 0, as an exact 0 would remove it) */
	if (e->Energy <= 0 && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Energy = FX(1);
		e->Destroyable = 0;
		e->Handlung = GEGNER_EXPLODIEREN;
		e->f[1] = FX(20);
		gegner_push(e->xPos + FX(50), e->yPos + FX(50), SKULL, 0, 99);   /* the skull */
		for (int i = 0; i < 15; i++)
			part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(GetRandom(200)), SPLITTER);
		for (int i = 0; i < 40; i++)
			part_push(e->xPos + FX(GetRandom(200) - 50), e->yPos + FX(GetRandom(250) - 20), SPIDERSPLITTER);
		for (int i = 0; i < 40; i++)
			part_push(e->xPos + FX(GetRandom(200) - 50), e->yPos + FX(GetRandom(250) - 20), SCHROTT1);
		for (int i = 0; i < 60; i++)
			part_push(e->xPos + FX(GetRandom(200) - 50), e->yPos + FX(GetRandom(200) - 50), EXPLOSION_MEDIUM2);
		part_push(e->xPos + FX(62), e->yPos + FX(100), SHOCKEXPLOSION);
		snd(SOUND_EXPLOSION2);
		pl.Score += 12500;
		shake_screen(5);
		music_stop();
	}

	TestDamagePlayers(e, SYNC(FX(10)), 0);
}

static void sk_explode(Ent *e)
{
	sk_after_boss();
	gegner_default_explode(e);
}

static void sk_load(void)
{
	P_sk_flamme = pic("skeletor_flame.png");
}

TYPEDEF(SKELETOR) = { sk_init, sk_ki, sk_explode, sk_draw, sk_load };
