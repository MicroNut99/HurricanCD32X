/*
 * UFO - the Ufo, boss of the tower stage (bosses/Boss_Ufo.cpp): comes down when the screen has
 * locked on it, swings over the player, shoots homing shots, lasers, drops rockets, dives; on
 * no energy it falls, and when it hits the floor it bursts (one-up, score, the screen goes back
 * to the player).
 * Own members: f[0] = ShotDelay, f[1] = ActionDelay, f[2] = SmokeDelay.
 * Left out: the LavaFlare glow drawn on it (additive), TileEngine.MaxOneUps (no counter here),
 * the music fades (the stage music stops when the screen locks, the boss music starts when it
 * is locked, the stage music comes back after the boss).
 */
static void ufo_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(4000);
	e->AnimEnde = 21;
	e->AnimSpeed = FX(1);
	e->Destroyable = 1;
	e->f[1] = FX(40);
	e->f[2] = 0;
	e->OwnDraw = 1;
	e->LastAction = -1;
	e->f[0] = 0;
}

static void ufo_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void ufo_ki(Ent *e)
{
	e->f[2] -= SYNC(FX(1));

	/* the energy bar */
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN) boss_bar(e->Energy, FX(4000));

	SimpleAnimation(e, 0);

	/* lock the screen on the boss as soon as it is seen */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		e->yPos -= FX(300);                 /* and the boss out of the screen */
		e->Handlung = GEGNER_INIT;
		music_stop();                       /* FadeSong(STAGEMUSIC, -2, 0, pause) */
	}

	/* a boss does not flash as long as the others */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* badly hit: it smokes */
	if (e->Energy < FX(700) && GetRandom(5) == 0)
		part_push(e->xPos + FX(GetRandom(190)), e->yPos + FX(GetRandom(60) + 25), SMOKE);

	/* no energy left: it falls */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = 0;
		e->ySpeed = 0;
		e->xAcc = 0;
		e->yAcc = FX(0.5);
		e->AnimEnde = 0;
		e->AnimCount = FX(1);
		music_stop();                       /* FadeSong(BOSS, -2, 0) */
	}

	/* swing over the player */
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EINFLIEGEN && e->Handlung != GEGNER_EXPLODIEREN)
	{
		if (e->xPos + FX(100) < FX(e->Value1 + 320)) e->xAcc = FX(3);
		else e->xAcc = FX(-3);
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:                       /* wait until the screen is locked */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_EINFLIEGEN:                 /* comes down into the screen */
		e->Energy = FX(4000);
		e->DamageTaken = 0;
		e->yPos += SYNC(FX(8));
		if (e->yPos >= FX(e->Value2 + 100))   /* TileEngine.ScrolltoY + 100 */
		{
			e->yPos = FX(e->Value2 + 100);
			e->Handlung = GEGNER_LAUFEN;
			e->xSpeed = FX(-35);
		}
		break;

	case GEGNER_LAUFEN:                     /* hovers over the player */
		if (e->yPos < FX(e->Value2 + 100)) e->yPos += SYNC(FX(5));
		if (e->yPos > FX(e->Value2 + 100)) e->yPos -= SYNC(FX(5));
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] < 0)
		{
			gegner_push(e->xPos + FX(45), e->yPos + FX(40), FETTERAKETE, 360, 99);
			gegner_push(e->xPos + FX(135), e->yPos + FX(40), FETTERAKETE, 0, 99);
			e->f[0] = FX(18);
			snd(SOUND_GRANATE);
		}
		/* a new action? */
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			int j = GetRandom(3);
			while (j == e->LastAction) j = GetRandom(3);
			e->LastAction = (int16_t)j;
			switch (j)
			{
			case 0: e->Handlung = GEGNER_SCHIESSEN; break;
			case 1: e->Handlung = GEGNER_BOMBARDIEREN; break;
			case 2:
				e->Handlung = GEGNER_FALLEN;
				e->ySpeed = FX(28);
				e->yAcc = FX(-2);
				e->f[0] = FX(1);
				break;
			}
			e->f[0] = FX(10);
			e->f[1] = FX(60);
		}
		break;

	case GEGNER_SCHIESSEN:                  /* homing shots at the player */
		if (e->yPos > FX(e->Value2 + 50)) e->yPos -= SYNC(FX(5));
		else e->yPos = FX(e->Value2 + 50);
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] = FX(80);
			e->Handlung = GEGNER_LAUFEN;
		}
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			snd(SOUND_GOLEMSHOT);
			proj_push(e->xPos + FX(20), e->yPos + FX(40), SUCHSCHUSS2);
			proj_push(e->xPos + FX(165), e->yPos + FX(40), SUCHSCHUSS2);
			e->f[0] = FX(5);
		}
		break;

	case GEGNER_BOMBARDIEREN:               /* the laser down at the player */
		if (e->yPos > FX(e->Value2 + 50)) e->yPos -= SYNC(FX(5));
		else e->yPos = FX(e->Value2 + 50);
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] = FX(60);
			e->Handlung = GEGNER_LAUFEN;
		}
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(4);
			snd(SOUND_LASERSHOT);
			proj_push(e->xPos + FX(100 - 21), e->yPos + FX(60), UFOLASER);
			part_push(e->xPos + FX(100 - 80), e->yPos + FX(30), UFOLASERFLARE);
		}
		break;

	case GEGNER_FALLEN:                     /* dives and comes back */
		if (e->ySpeed < FX(-28))
		{
			e->yAcc = 0;
			e->ySpeed = 0;
			e->f[1] = FX(30);
			e->Handlung = GEGNER_LAUFEN;
		}
		break;

	case GEGNER_EXPLODIEREN:                /* falls, burning */
		e->Energy = FX(100);
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			part_push(e->xPos + FX(GetRandom(180)), e->yPos + FX(GetRandom(70) + 20), EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
			if (GetRandom(4) == 0)
				part_push(e->xPos + FX(GetRandom(180) - 40), e->yPos + FX(GetRandom(40)), EXPLOSION_BIG);
			e->AnimCount = FX(2);
		}
		if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
		/* crashed on the floor: it bursts */
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->yAcc = 0;
			e->ySpeed = 0;
			e->Energy = 0;
		}
		break;

	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(20)), 0);
}

static void ufo_explode(Ent *e)
{
	snd(SOUND_EXPLOSION2);
	shake_screen(5);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos + FX(GetRandom(190)), e->yPos + FX(GetRandom(60) + 30), SPLITTER);
	pl.Score += 4000;
	gegner_push(e->xPos + FX(80), e->yPos + FX(20), ONEUP, 0, 0);
	/* ScrolltoPlayeAfterBoss: the screen back to the player, the stage music again */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(UFO) = { ufo_init, ufo_ki, ufo_explode, ufo_draw, 0 };
