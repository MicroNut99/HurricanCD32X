/*
 * KUGELRIESIG - the giant spiked ball (enemies/Gegner_KugelRiesig.cpp): bounces along the corridor, splits into
 * two KUGELGROSS when destroyed. Value1: its x speed (0: 8 in a random direction).
 * Own members: none.
 */
static void kugelriesig_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->blick = -1;
	e->yAcc = FX(5);
	if (e->Value1 != 0) e->xSpeed = FX(e->Value1);
	else e->xSpeed = GetRandom(2) == 0 ? FX(-8) : FX(8);
	e->AnimSpeed = FX(0.3);
	e->AnimStart = 0;
	e->AnimEnde = 19;
	e->Destroyable = 1;
}

static void kugelriesig_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->blick = -1;
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		/* forwards or backwards, as the ball rolls */
		if (e->xSpeed > 0) { if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart; }
		else { if (--e->AnimPhase < e->AnimStart) e->AnimPhase = e->AnimEnde; }
	}
	if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if ((e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND) || (e->blocku & B_PLATTFORM))) ||
		    (e->ySpeed < 0 && ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND))))
		{
			e->ySpeed = -e->ySpeed;
			if (e->ySpeed < 0 && e->ySpeed > FX(-22)) e->ySpeed = FX(-22);
		}
		if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
			e->xSpeed = -e->xSpeed;
		break;
	}
	TestDamagePlayers(e, SYNC(FX(8)), 0);
}

static void kugelriesig_explode(Ent *e)
{
	gegner_push(e->xPos + FX(16), e->yPos + FX(16), KUGELGROSS, -8, 0);
	gegner_push(e->xPos + FX(16), e->yPos + FX(16), KUGELGROSS, 8, 0);
	part_push(e->xPos - FX(5), e->yPos - FX(5), EXPLOSION_BIG);
	snd(SOUND_EXPLOSION4);
	pl.Score += 300;
}

TYPEDEF(KUGELRIESIG) = { kugelriesig_init, kugelriesig_ki, kugelriesig_explode, 0, 0 };
