/*
 * SKIWALKER - the walker on skis (enemies/Gegner_SkiWalker.cpp): races along, turns at walls,
 * shoots its laser when the player is ahead; the player can jump on it (it then flies off and
 * races away until it hits a wall).
 * Own members: f[0] = ShotDelay.
 */
static void skiwalker_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(25);
	e->Destroyable = 1;
	e->xSpeed = FX(20);
	e->ySpeed = 0;
	e->yAcc = 0;
	e->AnimSpeed = FX(1);
	e->AnimEnde = 11;
	e->f[0] = 0;
}

static void skiwalker_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	/* turn at walls */
	if (((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) && e->xSpeed > 0)
	{
		if (e->Handlung == GEGNER_WATSCHELN && (e->blockr & B_WAND)) e->Energy = 0;
		e->blick = -1;
		e->xSpeed = -e->xSpeed;
	}
	if (((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) && e->xSpeed < 0)
	{
		if (e->Handlung == GEGNER_WATSCHELN && (e->blockl & B_WAND)) e->Energy = 0;
		e->blick = 1;
		e->xSpeed = -e->xSpeed;
	}

	/* a platform: the player can jump on it (and even ride it) */
	PlattformTest(e, rect);

	/* jumped on? */
	if (pl.AufPlattform == e && (e->Handlung == GEGNER_LAUFEN || e->Handlung == GEGNER_FALLEN) &&
	    pl.Handlung != PA_RADELN && pl.Handlung != PA_RADELN_FALL && pl.Handlung != PA_SACKREITEN &&
	    pl.Handlung != PA_DREHEN && pl.yspeed > 0)
	{
		pl.JumpPossible = 0;
		pl.AnimPhase = 0;
		pl.Handlung = PA_SPRINGEN;
		pl.JumpStart = pl.ypos;
		pl.yspeed = FX(-43);              /* -PLAYER_MAXJUMPSPEED */
		pl.JumpAdd = 0;
		e->AnimSpeed = FX(0.4);
		e->Handlung = GEGNER_SPRINGEN;
		e->xSpeed = 0;
		e->ySpeed = FX(-30);
		e->yAcc = FX(5);
		snd(SOUND_WALKERGIGGLE);
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		/* falls off */
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			e->yAcc = FX(5);
			e->yPos -= FX(1);
			e->Handlung = GEGNER_FALLEN;
		}
		/* shoots when near */
		if (PlayerAbstand(e) <= 220 &&
		    ((e->blick == -1 && pl.xpos + FX(45) <= e->xPos) || (e->blick == 1 && pl.xpos - FX(45) >= e->xPos)))
		{
			e->f[0] -= SYNC(FX(1));
			if (e->f[0] <= 0)
			{
				e->f[0] = FX(10 + GetRandom(5));
				snd(SOUND_LASERSHOT);
				if (e->blick == -1) proj_push(e->xPos - FX(18), e->yPos + FX(9), WALKER_LASER);
				else proj_push(e->xPos + FX(40), e->yPos + FX(9), WALKER_LASER2);
			}
		}
		break;

	case GEGNER_FALLEN:
		/* landed */
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			if (e->ySpeed > 0) e->ySpeed = -e->ySpeed / 2;
			if (e->ySpeed > FX(-2))
			{
				BlockUnten(e->xPos, &e->yPos, e->xSpeed, &e->ySpeed, rect, 0);
				e->ySpeed = 0;
				e->yAcc = 0;
				e->Handlung = GEGNER_LAUFEN;
			}
		}
		break;

	/* jumped on: flies up and falls */
	case GEGNER_SPRINGEN:
		if (e->ySpeed > FX(38)) e->ySpeed = FX(38);
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)))
		{
			e->Handlung = GEGNER_WATSCHELN;
			e->yAcc = 0;
			e->ySpeed = 0;
			e->xSpeed = FX(43) * e->blick;
			BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
		}
		break;

	/* races away */
	case GEGNER_WATSCHELN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			e->yAcc = FX(1.6);
			e->Handlung = GEGNER_SPRINGEN;
		}
		break;
	}

	if (e->Handlung != GEGNER_WATSCHELN && e->Handlung != GEGNER_SPRINGEN) TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void skiwalker_explode(Ent *e)
{
	int i;
	for (i = 0; i < 5; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(45)), e->yPos - FX(20) + FX(GetRandom(45)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 100;
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	gegner_default_explode(e);
}

TYPEDEF(SKIWALKER) = { skiwalker_init, skiwalker_ki, skiwalker_explode, 0, 0 };
