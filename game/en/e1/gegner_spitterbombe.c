/*
 * SPITTERBOMBE - the spitter bomb (enemies/Gegner_Spitterbombe.cpp): falls; on the ground it
 * lets a Spitter (SPITTER) out; shot in the air it bursts into six shots.
 * Own members: none.
 */
static void spitterbombe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(20);
	e->AnimEnde = 6;
	e->AnimSpeed = FX(0.5);
	e->Destroyable = 1;
	e->ySpeed = FX(15);
}

static void spitterbombe_ki(Ent *e)
{
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		part_push(e->xPos - FX(5) + FX(GetRandom(10)), e->yPos, SMOKE);
		if (e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
	}

	TestDamagePlayers(e, SYNC(FX(1)), 0);

	/* on the ground: the Spitter comes out */
	if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
	{
		e->Energy = 0;
		gegner_push(e->xPos + FX(4), e->yPos + FX(20), SPITTER, 0, 0);
	}
}

static void spitterbombe_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(25) + FX(GetRandom(20)), e->yPos - FX(30) + FX(GetRandom(50)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos + FX(GetRandom(20)), e->yPos + FX(GetRandom(40)), SPIDERSPLITTER);
	/* not on the ground: shot down */
	if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
	{
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTLO);
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTLM);
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTLU);
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTRO);
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTRM);
		proj_push(e->xPos + FX(10), e->yPos + FX(30), SPITTERBOMBESHOTRU);
		pl.Score += 200;
	}
}

TYPEDEF(SPITTERBOMBE) = { spitterbombe_init, spitterbombe_ki, spitterbombe_explode, 0, 0 };
