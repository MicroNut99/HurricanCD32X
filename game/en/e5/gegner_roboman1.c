/*
 * ROBOMAN1 - the flying robot (enemies/Gegner_RoboMan1.cpp): keeps its distance to the player,
 * fires ROBOROCKETs; shot down it crashes and explodes against a wall.
 * Own members: f[0] = ShotCount, f[1] = SmokeCount.
 * Left out: the ROBOMANSMOKE exhaust (a dropped particle type; its timer is kept).
 * (roboman1.png is in none of the 9 stage packs: nothing is drawn there.)
 */
static void roboman1_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(150);
	e->Destroyable = 1;
	e->AnimEnde = 0;
	e->AnimSpeed = FX(1);
	e->f[0] = FX(1);
	e->f[1] = FX(0.2);
}

static void roboman1_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const Box *r = &GegnerRect[e->GegnerArt];
	e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, r, 1);
	e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, r, 1);
	e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, r, 1);
	e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, r, 1);

	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde)
			{
				if (e->AnimPhase == 14) e->blick = (int8_t)-e->blick;
				e->AnimEnde = 0;
				e->AnimPhase = 0;
			}
		}
	}

	e->f[1] -= SYNC(FX(1));
	if (e->f[1] < 0) e->f[1] += FX(0.1);     /* (ROBOMANSMOKE: dropped type) */

	/* always watch the player */
	if (e->AnimPhase == 0 && e->blick == 1 && pl.xpos + FX(45) < e->xPos + FX(45))
	{ e->AnimPhase = 3; e->AnimStart = 0; e->AnimEnde = 14; e->AnimSpeed = FX(0.5); }
	if (e->AnimPhase == 0 && e->blick == -1 && pl.xpos + FX(45) > e->xPos + FX(45))
	{ e->AnimPhase = 3; e->AnimStart = 0; e->AnimEnde = 14; e->AnimSpeed = FX(0.5); }

	const int abst = PlayerAbstand(e);
	/* away from the player */
	if (abst < 200 && e->Handlung != GEGNER_FALLEN)
	{
		if (pl.xpos + FX(45) > e->xPos + FX(45)) e->xPos -= SYNC(FX(8));
		if (pl.xpos + FX(45) < e->xPos + FX(45)) e->xPos += SYNC(FX(8));
	}
	/* towards the player */
	if (abst > 300 && e->Handlung != GEGNER_FALLEN)
	{
		if (pl.xpos + FX(45) > e->xPos + FX(45)) e->xPos += SYNC(FX(8));
		if (pl.xpos + FX(45) < e->xPos + FX(45)) e->xPos -= SYNC(FX(8));
	}
	/* shoot */
	if (abst < 500 && abst > 50 && e->Handlung != GEGNER_FALLEN && e->AnimEnde == 0)
	{
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			snd(SOUND_GRANATE);
			if (e->blick == -1) proj_push(e->xPos - FX(20), e->yPos + FX(75), ROBOROCKET);
			else proj_push(e->xPos + FX(70), e->yPos + FX(75), ROBOROCKET);
			e->f[0] = FX(15);
			e->AnimEnde = 3;
			e->AnimStart = 0;
			e->AnimPhase = 0;
			e->AnimSpeed = FX(1);
			e->AnimCount = 0;
		}
	}
	/* the player's height */
	if (e->Handlung != GEGNER_FALLEN)
	{
		if (e->yPos < pl.ypos - FX(40)) e->yAcc = FX(2.5);
		if (e->yPos > pl.ypos - FX(40)) e->yAcc = FX(-2.5);
		if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);
	}
	if (((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) && e->ySpeed < 0) e->ySpeed = -e->ySpeed;
	if (((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) && e->ySpeed > 0) e->ySpeed = -e->ySpeed;

	switch (e->Handlung)
	{
	case GEGNER_FALLEN:                /* crashing */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		e->f[0] -= SYNC(FX(3));
		if (e->f[0] <= 0)
		{
			part_push(e->xPos + FX(GetRandom(80)), e->yPos + FX(GetRandom(90)), SMOKE);
			e->f[0] = FX(1);
		}
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);

	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->Energy = FX(60);
		e->ySpeed = FX(3);
		e->yAcc = FX(2);
	}
}

static void roboman1_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	for (int i = 0; i < 3; i++)
		part_push(e->xPos - FX(40) + FX(GetRandom(70)), e->yPos + FX(GetRandom(40)), EXPLOSION_BIG);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(70)), e->yPos + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos + FX(GetRandom(70)), e->yPos + FX(20) + FX(GetRandom(60)), SPIDERSPLITTER);
}

TYPEDEF(ROBOMAN1) = { roboman1_init, roboman1_ki, roboman1_explode, 0, 0 };
