/*
 * KRABBLERLINKS - the wall crawler on the left wall (enemies/Gegner_KrabblerLinks.cpp): crawls up
 * and down (speed Value2) between walls, shoots sideways every Value1.
 * Own members: f[0] = ShotDelay.
 */
static void krabblerlinks_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(75);
	e->ySpeed = FX(e->Value2);
	e->blick = -1;
	e->f[0] = FX(e->Value1);
	e->AnimSpeed = FX(0.5);
	e->AnimStart = 0;
	e->AnimEnde = 9;
	e->Destroyable = 1;
}

static void krabblerlinks_ki(Ent *e)
{
	E2DBG(e);
	e->blick = -1;
	SimpleAnimation(e, 0);

	/* at the ceiling or the floor */
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
			proj_push(e->xPos + FX(24), e->yPos + FX(23), KRABBLERLASER2);
			part_push(e->xPos + FX(48), e->yPos + FX(3), LASERFLAME);
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
	default:
		break;
	}
}

static void krabblerlinks_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(15) + FX(GetRandom(20)), e->yPos - FX(15) + FX(GetRandom(40)), EXPLOSION_MEDIUM3);
	snd(SOUND_EXPLOSION1);
	pl.Score += 300;
	gegner_default_explode(e);
}

TYPEDEF(KRABBLERLINKS) = { krabblerlinks_init, krabblerlinks_ki, krabblerlinks_explode, 0, 0 };
