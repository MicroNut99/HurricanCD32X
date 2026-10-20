/*
 * WASSERMINE - the water mine (enemies/Gegner_WasserMine.cpp): bobs in the water and waits for
 * a touch. Hurrican's PushGegner passes it its y position as Value1 (unused).
 * Own members: none.
 * Left out: the screen shaking of its explosion.
 */
static void wassermine_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(20);
	e->AnimSpeed = FX(3);
	e->Destroyable = 0;
	e->AnimSpeed = FX(4);
	e->AnimEnde = 10;
}

static void wassermine_ki(Ent *e)
{
	SimpleAnimation(e, 0);
	TestDamagePlayers(e, FX(50), 1);
}

static void wassermine_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION3);
	snd(SOUND_EXPLOSION4);
	shake_screen(5);
	for (i = 0; i < 50; i++)
		part_push(e->xPos - FX(5) + FX(GetRandom(40)), e->yPos - FX(5) + FX(GetRandom(60)), WATERFLUSH_HIGH);
	for (i = 0; i < 25; i++)
		part_push(e->xPos - FX(5) + FX(GetRandom(40)), e->yPos - FX(50) + FX(GetRandom(110)), SPIDERSPLITTER);
	part_push(e->xPos - FX(40), e->yPos - FX(55), EXPLOSION_GIANT);
	gegner_default_explode(e);
}

TYPEDEF(WASSERMINE) = { wassermine_init, wassermine_ki, wassermine_explode, 0, 0 };
