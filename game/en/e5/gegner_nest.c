/*
 * NEST - the wasp nest (enemies/Gegner_Nest.cpp): lets out a STAHLMUECKE now and then.
 * Value2 == 99: the tutorial's (no wasps).
 * Own members: none (AnimCount is the spawn counter, as Hurrican).
 */
static void nest_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(120);
	e->AnimSpeed = FX(1);
	e->Destroyable = 1;
	e->AnimCount = FX(50);
}

static void nest_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	if (PlayerAbstand(e) < 800 && e->Value2 != 99)
	{
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			gegner_push(e->xPos + FX(20), e->yPos - FX(10), STAHLMUECKE, 0, 0);
			e->AnimCount = FX(GetRandom(30) + 30);
		}
	}
	TestDamagePlayers(e, SYNC(FX(0.5)), 0);
}

static void nest_explode(Ent *e)
{
	snd(SOUND_EXPLOSION4);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(25) + FX(GetRandom(20)), e->yPos - FX(30) + FX(GetRandom(50)), EXPLOSION_MEDIUM2);
	part_push(e->xPos - FX(5), e->yPos - FX(5), NESTLUFT);
	pl.Score += 500;
}

TYPEDEF(NEST) = { nest_init, nest_ki, nest_explode, 0, 0 };
