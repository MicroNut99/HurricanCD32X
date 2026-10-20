/*
 * RIESENPIRANHA - the giant piranha (bosses/Boss_RiesenPiranha.cpp): swims from one side of
 * its pool to the other, follows the player in the water and spits small piranhas.
 * Own members: f[0] = ShotDelay (unused, as in Hurrican).
 * Notes: its picture (riesenpiranha.png) is in none of the stage packs (it is on no map), so
 * it draws nothing unless a pack gets it. Not on any of the 9 maps; tested with HB_B2_SPAWN.
 */
static void riesenpiranha_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimStart = 0;
	e->AnimEnde = 4;
	e->AnimSpeed = FX(2);
	e->xSpeed = FX(-7);
	e->ySpeed = 0;
	e->xAcc = 0;
	e->yAcc = 0;
	e->Energy = FX(3000);
	e->Destroyable = 1;
	e->f[0] = 0;
}

static void riesenpiranha_turn(Ent *e)
{
	e->xSpeed = 0;
	e->Handlung = GEGNER_DREHEN;
	e->AnimPhase = 4;
	e->AnimStart = 0;
	e->AnimEnde = 8;
}

static void riesenpiranha_ki(Ent *e)
{
	b2test_hurt(e);
	/* animate */
	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}

	/* bubbles now and then */
	if (GetRandom(10) == 0)
		part_push(e->xPos + FX(10) + FX(GetRandom(230)), e->yPos + FX(10) + FX(GetRandom(190)), BUBBLE);

	/* turn at walls */
	if (e->blick == -1 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)))
		riesenpiranha_turn(e);
	if (e->blick == 1 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)))
	{
		riesenpiranha_turn(e);
		e->xPos -= FX(1);
	}

	/* the player in the water and near? */
	if (pl.InLiquid && PlayerAbstand(e) <= 300)
	{
		/* spit a small piranha now and then */
		if (e->AnimPhase == 0 && e->AnimCount == 0 && GetRandom(2) == 0)
			gegner_push(e->xPos + FX(110), e->yPos + FX(100), PIRANHA, 0, 0);

		if (pl.ypos - FX(70) < e->yPos && !(e->blocko & B_WAND) && !(e->blocko & B_GEGNERWAND))
			e->yPos -= SYNC(FX(6));
		if (pl.ypos - FX(70) > e->yPos && !(e->blocku & B_WAND) && !(e->blocku & B_GEGNERWAND))
			e->yPos += SYNC(FX(6));
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (pl.InLiquid && PlayerAbstand(e) <= 300)
		{
			/* swum past the player? then turn and follow him */
			if ((e->blick == -1 && pl.xpos > e->xPos + FX(GegnerRect[e->GegnerArt].r - 20)) ||
			    (e->blick == 1 && pl.xpos + FX(pl.CollideRect.r) < e->xPos))
				riesenpiranha_turn(e);
		}
		else e->xSpeed = e->blick == -1 ? FX(-7) : FX(7);
		break;

	case GEGNER_DREHEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimEnde = 4;
			e->AnimStart = 0;
			e->AnimPhase = 0;
			if (e->blick == -1) { e->blick = 1; e->xSpeed = FX(7); }
			else { e->blick = -1; e->xSpeed = FX(-7); }
		}
		break;
	}

	TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void riesenpiranha_explode(Ent *e)
{
	int i;
	for (i = 0; i < 8; i++)
		part_push(e->xPos + FX(64) + FX(GetRandom(64)), e->yPos + FX(64) + FX(GetRandom(64)), PIRANHATEILE2);
	for (i = 0; i < 100; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(256)), e->yPos + FX(10) + FX(GetRandom(210)), BUBBLE);
	for (i = 0; i < 70; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(256)), e->yPos + FX(10) + FX(GetRandom(210)), PIRANHABLUT);
	gegner_push(e->xPos + FX(110), e->yPos + FX(100), ONEUP, 0, 0);
	pl.Score += 4000;
	gegner_default_explode(e);
}

static void riesenpiranha_load(void)
{
	b2test_load();
}

TYPEDEF(RIESENPIRANHA) = { riesenpiranha_init, riesenpiranha_ki, riesenpiranha_explode, 0, riesenpiranha_load };
