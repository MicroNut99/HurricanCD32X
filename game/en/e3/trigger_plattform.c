/*
 * PLATTFORM - the moving platform (triggers/Trigger_Plattform.cpp): moves with Value1 / Value2
 * as its x / y speed and turns at the map's turning points (B_WENDEPUNKT).
 * Own members: none. Frame 1 when the level's power block setting is 2 (GetUsedPowerBlock).
 */
static void plattform_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->xSpeed = FX(e->Value1);
	e->ySpeed = FX(e->Value2);
	e->Destroyable = 0;
	e->Active = 1;
}

static void plattform_ki(Ent *e)
{
	e->AnimPhase = UsedPowerBlock == 2 ? 1 : 0;

	/* turn at the turning points */
	if (e->blockl & B_WENDEPUNKT) e->xSpeed = FX(e->Value1);
	if (e->blockr & B_WENDEPUNKT) e->xSpeed = -FX(e->Value1);
	if (e->blocko & B_WENDEPUNKT) e->ySpeed = FX(e->Value2);
	if (e->blocku & B_WENDEPUNKT) e->ySpeed = -FX(e->Value2);

	/* the player stands on it */
	PlattformTest(e, &GegnerRect[e->GegnerArt]);
}

static void plattform_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(PLATTFORM) = { plattform_init, plattform_ki, plattform_explode, 0, 0 };
