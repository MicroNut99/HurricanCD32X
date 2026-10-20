/*
 * POKENUKE - the giant nuke (enemies/Gegner_PokeNuke.cpp): falls into the elevator and blows
 * up on it, hurting the player unless he stands at the edge.
 * Own members: f[0] = SmokeDelay.
 * Left out: ShakeScreen, the SPLITTER / FUNKE / EXPLOSIONFLARE of the explosion (dropped
 * particle types).
 */
static void pokenuke_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(10);
	e->Destroyable = 0;
	e->ySpeed = FX(27);
	e->f[0] = 0;
}

static void pokenuke_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->blick = -1;
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		e->Handlung = GEGNER_FALLEN;
		e->yPos -= FX(RENDERHEIGHT + 217);
		break;
	case GEGNER_FALLEN:
		if (e->yPos > g_Fahrstuhl_yPos - FX(217)) e->Energy = 0;
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.2);
			part_push(e->xPos + FX(24) - FX(GetRandom(26)), e->yPos - FX(16), SMOKEBIG);
		}
		break;
	}
	TestDamagePlayers(e, FX(100), 1);
}

static void pokenuke_explode(Ent *e)
{
	snd(SOUND_EXPLOSION2);
	shake_screen(7);
	if (PlayerAbstand(e) < 160) DamagePlayer(FX(50), 0);
	for (int i = 0; i < 16; i++)
		part_push(e->xPos - FX(60) + FX(GetRandom(90)), e->yPos - FX(40) + FX(GetRandom(200)), SMOKEBIG);
	for (int i = 0; i < 8; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(90)), e->yPos - FX(30) + FX(GetRandom(200)), EXPLOSION_BIG);
	for (int i = 0; i < 12; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(90)), e->yPos - FX(20) + FX(GetRandom(200)), EXPLOSION_MEDIUM2);
}

TYPEDEF(POKENUKE) = { pokenuke_init, pokenuke_ki, pokenuke_explode, 0, 0 };
