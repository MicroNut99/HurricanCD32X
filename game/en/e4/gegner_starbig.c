/*
 * STARBIG - the big star of the flying-sack level (enemies/Gegner_StarBig.cpp): bounces off the
 * walls and the screen's top and bottom, fires a homing shot every few animation rounds.
 * Own members: i[0] = ShotCount.
 * (starbig.png is in none of the stage packs: no stage of this port has the big star.)
 */
static void starbig_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(100);
	e->AnimStart = 0;
	e->AnimEnde = 4;
	e->AnimSpeed = FX(0.75);
	e->AnimCount = 0;
	e->Destroyable = 1;
	e->i[0] = 0;
	e->ySpeed = FX(12);
	e->yAcc = 0;
	e->xAcc = 0;
	/* towards the player (the constructor sees xPos 0, as in Hurrican) */
	if (pl.xpos + FX(80) < e->xPos) e->xSpeed = FX(-12);
	else e->xSpeed = FX(12);
}

static void starbig_ki(Ent *e)
{
	e->blick = e->xSpeed > 0 ? 1 : -1;

	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase >= e->AnimEnde)
		{
			e->AnimPhase = e->AnimStart;
			e->i[0]++;
			/* shoot */
			if (e->i[0] > SKILL + 2)
			{
				e->i[0] = 0;
				snd(SOUND_CANON);
				proj_push(e->xPos + FX(36), e->yPos + FX(36), SUCHSCHUSS);
			}
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if ((e->xSpeed < 0 && (e->blockl & B_WAND)) || (e->xSpeed > 0 && (e->blockr & B_WAND))) e->xSpeed = -e->xSpeed;
		if ((e->ySpeed < 0 && (e->blocko & B_WAND)) || (e->ySpeed > 0 && (e->blocku & B_WAND)) ||
		    (e->ySpeed < 0 && e->yPos - cam_yoff() < 0) || (e->ySpeed > 0 && e->yPos - cam_yoff() > FX(400)))
			e->ySpeed = -e->ySpeed;
		break;
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void starbig_explode(Ent *e)
{
	int i;
	part_push(e->xPos - FX(20), e->yPos - FX(20), EXPLOSION_GIANT);
	for (i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(80)), e->yPos - FX(30) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
	for (i = 0; i < 15; i++)
		part_push(e->xPos + FX(GetRandom(80)), e->yPos + FX(GetRandom(80)), LONGFUNKE);
	snd(SOUND_EXPLOSION3);
	pl.Score += 150;
	gegner_default_explode(e);
}

TYPEDEF(STARBIG) = { starbig_init, starbig_ki, starbig_explode, 0, 0 };
