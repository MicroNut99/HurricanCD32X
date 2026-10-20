/*
 * KRABBLERRECHTS - the wall crawler on the right wall (enemies/Gegner_KrabblerRechts.cpp):
 * crawls up and down (speed Value2), shoots sideways every Value1.
 * Own members: f[0] = ShotDelay.
 * Left out: the LASERFLAME glow (a dropped particle type).
 */
static void krabblerrechts_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(75);
	e->ySpeed = FX(e->Value2);
	e->blick = 1;
	e->f[0] = FX(e->Value1);
	e->AnimSpeed = FX(0.5);
	e->AnimStart = 0;
	e->AnimEnde = 9;
	e->Destroyable = 1;
}

static void krabblerrechts_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->blick = -1;
	SimpleAnimation(e, 0);
	if ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND)) e->ySpeed = FX(e->Value2);
	if ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)) e->ySpeed = FX(-e->Value2);
	e->f[0] -= SYNC(FX(1));
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(e->Value1);
			e->Handlung = GEGNER_SCHIESSEN;
			proj_push(e->xPos - FX(24), e->yPos + FX(23), KRABBLERLASER3);
			snd(SOUND_LASERSHOT);
			e->AnimPhase = 10;
			e->AnimEnde = 14;
			e->AnimStart = 0;
			e->AnimSpeed = FX(0.75);
		}
		break;
	case GEGNER_SCHIESSEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->AnimEnde = 9;
			e->AnimPhase = 0;
			e->AnimSpeed = FX(0.5);
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	}
}

static void krabblerrechts_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(15) + FX(GetRandom(20)), e->yPos - FX(15) + FX(GetRandom(40)), EXPLOSION_MEDIUM3);
	snd(SOUND_EXPLOSION1);
	pl.Score += 300;
}

TYPEDEF(KRABBLERRECHTS) = { krabblerrechts_init, krabblerrechts_ki, krabblerrechts_explode, 0, 0 };
