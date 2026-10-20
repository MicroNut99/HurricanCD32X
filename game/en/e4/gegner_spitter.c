/*
 * SPITTER - the spitter (enemies/Gegner_Spitter.cpp): follows the player along the ground and
 * spits a shot straight up every animation round.
 * Own members: none.
 */
static void spitter_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimEnde = 4;
	e->AnimSpeed = FX(1.25);
	e->Energy = FX(20);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void spitter_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
	e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
	e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);

	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase == 2)
			{
				proj_push(e->xPos + FX(8), e->yPos, STRAIGHTSCHUSS);
				part_push(e->xPos, e->yPos - FX(8), SMOKE);
			}
			if (e->AnimPhase >= e->AnimEnde) e->AnimPhase = 0;
		}
	}

	/* follow the player */
	if (PlayerAbstand(e) > 100)
	{
		if (pl.xpos < e->xPos) { e->blick = -1; e->xAcc = FX(-2); }
		else { e->blick = 1; e->xAcc = FX(2); }
	}
	if (e->xSpeed < FX(-7)) { e->xSpeed = FX(-7); e->xAcc = 0; }
	if (e->xSpeed > FX(7)) { e->xSpeed = FX(7); e->xAcc = 0; }

	if (e->ySpeed >= 0) e->blocku = (uint16_t)BlockSlopes(e->xPos, &e->yPos, rect, e->ySpeed);

	/* falling? */
	if (!(e->blocku & B_SCHRAEGE_R) && !(e->blocku & B_SCHRAEGE_L))
	{
		e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			if (e->ySpeed == 0) e->ySpeed = FX(5);
			e->yAcc = FX(3);
		}
		else
		{
			e->ySpeed = 0;
			e->yAcc = 0;
		}
	}

	/* turn at walls */
	if (e->xSpeed < 0)
		if (!(e->blocku & B_SCHRAEGE_L) && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) e->xSpeed = -e->xSpeed;
	if (e->xSpeed > 0)
		if (!(e->blocku & B_SCHRAEGE_R) && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))) e->xSpeed = -e->xSpeed;

	TestDamagePlayers(e, SYNC(FX(1)), 0);
}

static void spitter_explode(Ent *e)
{
	pl.Score += 50;
	part_push(e->xPos - FX(10), e->yPos - FX(10), EXPLOSION_MEDIUM);
	snd(SOUND_EXPLOSION1);
	gegner_default_explode(e);
}

TYPEDEF(SPITTER) = { spitter_init, spitter_ki, spitter_explode, 0, 0 };
