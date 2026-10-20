/*
 * FIESERWALKER - the mean walker with the laser (enemies/Gegner_FieserWalker.cpp): walks, turns
 * at walls and when shot, stops and fires a homing shot (SUCHSCHUSS) at the player.
 * Own members: f[0] = ShotDelay.
 * (Its picture fieserwalker.png is in no stage pack: the type is in no map either.)
 */
static void fieserwalker_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(100);
	e->Destroyable = 1;
	e->AnimSpeed = FX(0.5);
	e->AnimEnde = 14;
	e->xSpeed = FX(5);
	e->f[0] = 0;
}

static void fieserwalker_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = FX(2);
			e->yAcc = FX(3);
		}
		/* near enough to shoot? */
		if (e->f[0] <= 0 && e->AnimPhase == 3 && PlayerAbstand(e) < 400 && PlayerAbstandVert(e) < 150 &&
		    ((e->blick == 1 && e->xPos < pl.xpos) || (e->blick == -1 && e->xPos > pl.xpos)))
		{
			e->f[0] = FX(15);
			e->Handlung = GEGNER_STEHEN;
			e->xSpeed = 0;
			e->AnimEnde = 0;
		}
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		TurnonShot(e);
		break;
	case GEGNER_STEHEN:
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		if (e->f[0] < FX(12))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimEnde = 14;
			e->xSpeed = FX(5) * e->blick;
			snd(SOUND_LASERSHOT);
			part_push(e->xPos + FX(10) + FX(e->blick * 40), e->yPos + FX(6), LASERFLAME);
			proj_push(e->xPos + FX(26) + FX(e->blick * 40), e->yPos + FX(23), SUCHSCHUSS);
		}
		break;
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(35))
		{
			e->ySpeed = FX(35);
			e->yAcc = 0;
		}
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->ySpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	}

	TurnonWall(e);
	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void fieserwalker_explode(Ent *e)
{
	part_push(e->xPos - FX(30), e->yPos - FX(30), EXPLOSION_BIG);
	for (int i = 0; i < 8; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(60)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 12; i++)
		part_push(e->xPos + FX(GetRandom(50)), e->yPos + FX(GetRandom(50)), SPIDERSPLITTER);
	snd(SOUND_EXPLOSION4);
	pl.Score += 80;
	gegner_default_explode(e);
}

TYPEDEF(FIESERWALKER) = { fieserwalker_init, fieserwalker_ki, fieserwalker_explode, 0, 0 };
