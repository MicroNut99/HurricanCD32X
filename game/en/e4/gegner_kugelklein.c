/*
 * KUGELKLEIN - the small spiked ball (enemies/Gegner_KugelKlein.cpp): bounces along the
 * corridor; Value1 gives its speed, else it starts left or right at random.
 * Own members: none.
 */
static void kugelklein_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(20);
	e->blick = -1;
	e->yAcc = FX(5);
	if (e->Value1 != 0) e->xSpeed = FX(e->Value1);
	else e->xSpeed = GetRandom(2) == 0 ? FX(-8) : FX(8);
	e->AnimSpeed = FX(0.3);
	e->AnimStart = 0;
	e->AnimEnde = 19;
	e->Destroyable = 1;
}

static void kugelklein_ki(Ent *e)
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
			if (e->ySpeed < 0 && e->ySpeed > FX(-10)) e->ySpeed = FX(-10);
		}
		/* and from the sides */
		if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
			e->xSpeed = -e->xSpeed;
		break;
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void kugelklein_explode(Ent *e)
{
	part_push(e->xPos - FX(15), e->yPos - FX(15), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION4);
	pl.Score += 300;
	gegner_default_explode(e);
}

TYPEDEF(KUGELKLEIN) = { kugelklein_init, kugelklein_ki, kugelklein_explode, 0, 0 };
