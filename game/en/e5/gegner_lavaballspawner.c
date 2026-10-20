/*
 * LAVABALLSPAWNER - makes the lava balls that hop out of the lava
 * (enemies/Gegner_LavaBallSpawner.cpp): a LAVABALL (Value1, Value2) every Value2 + 0..19.
 * Own members: f[0] = Delay.  Draws nothing (OwnDraw without a DoDraw).
 */
static void lavaballspawner_init(Ent *e)
{
	e->Active = 1;
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->Destroyable = 0;
	e->f[0] = FX(e->Value2);
	e->DontMove = 1;
	e->OwnDraw = 1;
}

static void lavaballspawner_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	if (PlayerAbstand(e) < 2000) e->f[0] -= SYNC(FX(1));
	if (e->f[0] <= 0)
	{
		e->f[0] = FX(e->Value2 + GetRandom(20));
		gegner_push(e->xPos, e->yPos, LAVABALL, e->Value1, e->Value2);
	}
}

static void lavaballspawner_explode(Ent *e) { (void)e; }

TYPEDEF(LAVABALLSPAWNER) = { lavaballspawner_init, lavaballspawner_ki, lavaballspawner_explode, 0, 0 };
