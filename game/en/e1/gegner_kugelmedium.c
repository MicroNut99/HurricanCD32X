/*
 * KUGELMEDIUM - the middle spiked ball (enemies/Gegner_KugelMedium.cpp): bounces along the
 * corridor; destroyed, it splits into two small balls (KUGELKLEIN).
 * Own members: none.
 */
static void kugelmedium_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(30);
	e->blick = -1;
	e->yAcc = FX(5);
	if (e->Value1 != 0) e->xSpeed = FX(e->Value1);
	else e->xSpeed = GetRandom(2) == 0 ? FX(-8) : FX(8);
	e->AnimSpeed = FX(0.3);
	e->AnimStart = 0;
	e->AnimEnde = 19;
	e->Destroyable = 1;
}

static void kugelmedium_ki(Ent *e)
{
	e->blick = -1;
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		/* forwards or backwards, as it rolls */
		if (e->xSpeed > 0)
		{
			if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
		else
		{
			if (--e->AnimPhase < e->AnimStart) e->AnimPhase = e->AnimEnde;
		}
	}

	if (e->ySpeed > FX(20)) e->ySpeed = FX(20);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		/* bounce up and down */
		if ((e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND) || (e->blocku & B_PLATTFORM))) ||
		    (e->ySpeed < 0 && ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND))))
		{
			e->ySpeed = -e->ySpeed;
			if (e->ySpeed < 0 && e->ySpeed > FX(-14)) e->ySpeed = FX(-14);
		}
		/* and off the walls */
		if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
			e->xSpeed = -e->xSpeed;
		break;
	default:
		break;
	}
	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void kugelmedium_explode(Ent *e)
{
	gegner_push(e->xPos + FX(8), e->yPos + FX(8), KUGELKLEIN, -8, 0);
	gegner_push(e->xPos + FX(8), e->yPos + FX(8), KUGELKLEIN, 8, 0);
	part_push(e->xPos - FX(8), e->yPos - FX(8), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION4);
	pl.Score += 300;
}

TYPEDEF(KUGELMEDIUM) = { kugelmedium_init, kugelmedium_ki, kugelmedium_explode, 0, 0 };
