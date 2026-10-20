/*
 * FALLBOMBE - the falling bomb in the elevator (enemies/Gegner_FallBombe.cpp): when it comes
 * onto the screen it is set a screen above and falls onto the player; it explodes when it
 * reaches the elevator (g_Fahrstuhl_yPos).
 * Own members: f[0] = SmokeDelay.
 */
static void fallbombe_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->Energy = FX(10);
	e->Destroyable = 0;
	e->AnimEnde = 6;
	e->AnimStart = 0;
	e->AnimSpeed = FX(1);
	e->ySpeed = FX(10);
	e->yAcc = FX(3);
	e->f[0] = 0;
	e->TestBlock = 0;
#ifndef OJ32X_MARS
	/* PC test: HB_E4_FAHRSTUHL=<y> puts the elevator's floor there (no elevator in this build) */
	if (getenv("HB_E4_FAHRSTUHL")) g_Fahrstuhl_yPos = FX(atoi(getenv("HB_E4_FAHRSTUHL")));
#endif
}

static void fallbombe_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	/* set above the player so that it can fall from there */
	case GEGNER_NOTVISIBLE:
		e->Handlung = GEGNER_FALLEN;
		e->yPos -= FX(RENDERHEIGHT + 59);
		if (e->yPos < 0) e->yPos = 0;
		break;

	case GEGNER_FALLEN:
		if (e->ySpeed > g_Fahrstuhl_Speed + FX(50)) e->ySpeed = g_Fahrstuhl_Speed + FX(50);
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.15);
			part_push(e->xPos - FX(GetRandom(8)), e->yPos - FX(16), SMOKE);
		}
		break;
	}

	TestDamagePlayers(e, FX(25), 1);

	if (e->yPos >= g_Fahrstuhl_yPos - FX(59)) e->Energy = 0;
}

static void fallbombe_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos - FX(40), e->yPos, EXPLOSION_BIG);
	for (i = 0; i < 40; i++)
		part_push(e->xPos + FX(GetRandom(20)), e->yPos + FX(40) + FX(GetRandom(30)), FUNKE);
	for (i = 0; i < 16; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos + FX(40) + FX(GetRandom(30)), SMOKE3);
	gegner_default_explode(e);
}


TYPEDEF(FALLBOMBE) = { fallbombe_init, fallbombe_ki, fallbombe_explode, 0, 0 };
