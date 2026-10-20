/*
 * WALKER - the walker (enemies/Gegner_Walker.cpp): walks to a wall and turns, shoots its laser
 * when the player is ahead, can be jumped on.
 * Own members: f[0] = ShotDelay.
 */
static void walker_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimStart = 0;
	e->AnimEnde = 11;
	e->AnimSpeed = FX(0.5);
	e->xSpeed = FX(10);
	e->ySpeed = 0;
	e->xAcc = 0;
	e->yAcc = 0;
	e->Energy = FX(10);
	e->Destroyable = 1;
	e->f[0] = 0;
}

static void walker_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	/* turn at walls */
	if (e->blick == -1 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)))
	{
		e->blick = 1;
		e->xSpeed = FX(10);
		if (e->Handlung == GEGNER_WATSCHELN) e->Energy = 0;
	}
	if (e->blick == 1 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)))
	{
		e->blick = -1;
		e->xSpeed = FX(-10);
		if (e->Handlung == GEGNER_WATSCHELN) e->Energy = 0;
	}

	e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);

	/* towards the player when hit */
	if (e->DamageTaken > 0 && e->Handlung != GEGNER_WATSCHELN && e->Handlung != GEGNER_SPRINGEN)
	{
		if (pl.xpos < e->xPos) { e->blick = -1; e->xSpeed = FX(-10); }
		else { e->blick = 1; e->xSpeed = FX(10); }
	}

	/* the player can jump on its head */
	if (e->Handlung != GEGNER_WATSCHELN && e->Handlung != GEGNER_SPRINGEN) PlattformTest(e, rect);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM)) { e->Handlung = GEGNER_FALLEN; e->yAcc = FX(4); }
		if (PlayerAbstand(e) <= 220 &&
		    ((e->blick == -1 && pl.xpos + FX(45) <= e->xPos) || (e->blick == 1 && pl.xpos - FX(45) >= e->xPos)))
		{
			e->f[0] -= SYNC(FX(1));
			if (e->f[0] <= 0)
			{
				e->f[0] = FX(10 + GetRandom(5));
				e->AnimStart = 12;
				e->AnimPhase = 12;
				e->AnimEnde = 20;
				e->xSpeed = 0;
				e->Handlung = GEGNER_SCHIESSEN;
			}
		}
		break;
	case GEGNER_SCHIESSEN:
		e->xSpeed = 0;
		if (e->AnimPhase == e->AnimStart && e->AnimCount == 0)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimStart = 0;
			e->AnimPhase = 0;
			e->AnimEnde = 11;
			e->xSpeed = FX(10) * e->blick;
		}
		if (e->AnimPhase == 17 && e->AnimCount == 0)
		{
			snd(SOUND_LASERSHOT);
			if (e->blick == -1) proj_push(e->xPos - FX(18), e->yPos + FX(23), WALKER_LASER);
			else proj_push(e->xPos + FX(30), e->yPos + FX(23), WALKER_LASER2);
		}
		break;
	case GEGNER_WATSCHELN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM)) { e->Handlung = GEGNER_SPRINGEN; e->yAcc = FX(3); }
		break;
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(25)) e->ySpeed = FX(25);
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)) { e->Handlung = GEGNER_LAUFEN; e->yAcc = 0; e->ySpeed = 0; }
		break;
	case GEGNER_SPRINGEN:
		if (e->ySpeed > FX(30)) e->ySpeed = FX(30);
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)))
		{
			e->Handlung = GEGNER_WATSCHELN;
			e->yAcc = 0;
			e->ySpeed = 0;
			e->xSpeed = FX(25) * e->blick;
		}
		break;
	}

	/* jumped on: the player bounces off, the walker is knocked away */
	if (pl.Handlung != PA_RADELN && pl.Handlung != PA_RADELN_FALL && pl.yspeed >= 0 && pl.AufPlattform == e)
	{
		pl.AufPlattform = 0;
		pl.JumpPossible = 0;
		pl.AnimPhase = 2;
		pl.Handlung = PA_SPRINGEN;
		pl.JumpStart = pl.ypos;
		pl.yspeed = FX(-43);
		pl.JumpAdd = 0;
		e->AnimSpeed = FX(0.3);
		e->AnimPhase = 20;
		e->AnimStart = 20;
		e->AnimEnde = 31;
		e->Handlung = GEGNER_SPRINGEN;
		e->xSpeed = 0;
		e->ySpeed = FX(-30);
		e->yAcc = FX(5);
		snd(SOUND_WALKERGIGGLE);
		e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
		e->yPos -= FX(5);
	}

	if (e->Handlung != GEGNER_WATSCHELN && e->Handlung != GEGNER_SPRINGEN) TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void walker_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(45)), e->yPos - FX(20) + FX(GetRandom(45)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 100;
	gegner_default_explode(e);
}

TYPEDEF(WALKER) = { walker_init, walker_ki, walker_explode, 0, 0 };
