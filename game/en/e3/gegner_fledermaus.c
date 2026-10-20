/*
 * FLEDERMAUS - the bat (enemies/Gegner_Fledermaus.cpp): hangs from the ceiling and flies at the
 * player when he comes near or when it is hit; flies back to the ceiling now and then.
 * Value2 == 1: flies at once (the cave "boss").
 * Own members: none (Value1 / Value2 are its flight target, as in Hurrican). Hurrican's
 * PushGegner passes the map's y as Value1 - only the target uses Value1, set before it is read.
 * fledermaus_DeckenCount: Hurrican's function-static DeckenCount (shared by all bats).
 */
static fx fledermaus_DeckenCount;

static void fledermaus_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(35);
	e->AnimSpeed = FX(0.75);
	e->Destroyable = 1;
	e->AnimPhase = 15;
	e->AnimStart = 15;
	e->AnimEnde = 21;
	if (e->Value2 == 1) e->Handlung = GEGNER_LAUFEN;
}

static void fledermaus_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	if (((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)) && e->ySpeed > 0) e->ySpeed = -e->ySpeed;
	if ((e->blocko & B_WAND) && e->ySpeed < 0) e->ySpeed = -e->ySpeed;
	if ((e->blockr & B_WAND) && e->xSpeed > 0) { e->xSpeed = -e->xSpeed; e->blick = -1; }
	if ((e->blockl & B_WAND) && e->xSpeed < 0) { e->xSpeed = -e->xSpeed; e->blick = 1; }

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		if ((PlayerAbstandHoriz(e) < 240 && pl.ypos > e->yPos) || e->DamageTaken > 0) e->Handlung = GEGNER_LAUFEN;
		break;
	case GEGNER_LAUFEN:          /* a new target near the player */
		e->AnimSpeed = FX(0.5);
		e->AnimEnde = 14;
		e->AnimStart = 0;
		e->Value1 = (int16_t)(FINT(pl.xpos) + GetRandom(80) - 30);
		e->Value2 = (int16_t)(FINT(pl.ypos) - 50 + GetRandom(100));
		e->Handlung = GEGNER_VERFOLGEN;
		break;
	case GEGNER_VERFOLGEN:
	{
		if (FX(e->Value1) < e->xPos && !(e->blockl & B_WAND)) { e->xAcc = FX(-3); e->blick = -1; }
		if (FX(e->Value1) > e->xPos && !(e->blockr & B_WAND)) { e->xAcc = FX(3); e->blick = 1; }
		if (FX(e->Value2) < e->yPos)
		{
			if (!(e->blocko & B_WAND)) e->yAcc = FX(-3);
			else e->ySpeed = FX(20);
		}
		if (FX(e->Value2) > e->yPos)
		{
			if (!(e->blocku & B_WAND)) e->yAcc = FX(3);
			else { e->ySpeed = FX(-10); e->yAcc = 0; }
		}
		if (e->xSpeed < FX(-10)) e->xSpeed = FX(-10);
		if (e->xSpeed > FX(10)) e->xSpeed = FX(10);
		if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);

		const int dx = abs(FINT(e->xPos - FX(e->Value1)));
		const int dy = abs(FINT(e->yPos - FX(e->Value2)));
		if (PlayerAbstand(e) > 300 || dx * dx + dy * dy < 20 * 20) e->Handlung = GEGNER_LAUFEN;

		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimPhase = 0;
		}

		/* back at the ceiling? */
		if (fledermaus_DeckenCount > 0) fledermaus_DeckenCount -= SYNC(FX(1));
		if (fledermaus_DeckenCount <= 0 && (e->blocko & B_WAND))
		{
			fledermaus_DeckenCount = FX(10);
			e->Handlung = GEGNER_STEHEN;
			e->AnimPhase = 15;
			e->AnimStart = 15;
			e->AnimEnde = 21;
			e->AnimSpeed = FX(0.75);
			e->xSpeed = 0;
			e->ySpeed = 0;
			e->xAcc = 0;
			e->yAcc = 0;
		}
	} break;
	}

	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void fledermaus_explode(Ent *e)
{
	for (int i = 0; i < 100; i++)
		part_push(e->xPos + FX(10) + FX(GetRandom(35)), e->yPos + FX(10) + FX(GetRandom(25)), FUNKE);
	part_push(e->xPos - FX(5), e->yPos - FX(5), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 100;
	gegner_default_explode(e);
}

TYPEDEF(FLEDERMAUS) = { fledermaus_init, fledermaus_ki, fledermaus_explode, 0, 0 };
