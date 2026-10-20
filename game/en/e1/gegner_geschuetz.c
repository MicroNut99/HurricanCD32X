/*
 * GESCHUETZ - the ceiling gun (enemies/Gegner_Geschuetz.cpp): hangs closed on the ceiling,
 * opens when the player is near, fires a laser and closes again (in a loop).
 * Own members: none.
 */
static void geschuetz_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(50);
	e->AnimSpeed = FX(0.75);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void geschuetz_ki(Ent *e)
{
	e->blick = -1;
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:   /* closed, waits for the player */
		if (pl.ypos + FX(70) >= e->yPos && PlayerAbstandHoriz(e) < 400 && PlayerAbstandVert(e) < 400)
		{
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimPhase = 1;
			e->AnimStart = 11;
			e->AnimEnde = 11;
			e->AnimCount = 0;
		}
		break;
	case GEGNER_SCHIESSEN:   /* open: fire */
		if (e->AnimPhase == e->AnimStart)
		{
			proj_push(e->xPos + FX(27), e->yPos + FX(36), KRABBLERLASER1);
			snd(SOUND_LASERSHOT);
			e->Handlung = GEGNER_SCHLIESSEN;
			e->AnimEnde = 25;
			e->AnimStart = 0;
		}
		break;
	case GEGNER_SCHLIESSEN:   /* closed again? */
		if (e->AnimPhase == e->AnimStart)
		{
			e->Handlung = GEGNER_STEHEN;
			e->AnimEnde = 0;
			e->AnimStart = 0;
			e->AnimPhase = 0;
		}
		break;
	default:
		break;
	}
	TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void geschuetz_explode(Ent *e)
{
	part_push(e->xPos - FX(10), e->yPos - FX(10), EXPLOSION_MEDIUM2);
	part_push(e->xPos - FX(10), e->yPos - FX(10), SMOKEBIG);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos + FX(10) + FX(GetRandom(40)), e->yPos + FX(GetRandom(40)), LONGFUNKE);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
}

TYPEDEF(GESCHUETZ) = { geschuetz_init, geschuetz_ki, geschuetz_explode, 0, 0 };
