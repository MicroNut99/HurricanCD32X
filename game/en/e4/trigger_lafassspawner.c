/*
 * LAFASSSPAWNER - makes a LaFass every Value2 (default 40) time units while the player is near
 * (triggers/Trigger_LaFassSpawner.cpp); Value1 0: it swings to the left, else to the right.
 * Own members: none (AnimCount is the timer, as in Hurrican).
 */
static void lafassspawner_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->AnimCount = FX(e->Value2);         /* AnimCount = Wert2: the map's value, before the default */
	if (e->Value2 == 0) e->Value2 = 40;
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->DontMove = 1;
	e->Active = 1;
}

static void lafassspawner_ki(Ent *e)
{
	if (PlayerAbstand(e) < 800) e->AnimCount -= SYNC(FX(1));
	if (e->AnimCount <= 0)
	{
		e->AnimCount = FX(e->Value2);
		/* pushed the right way */
		gegner_push(e->xPos, e->yPos, LAFASS, e->Value1 == 0 ? -5 : 5, 0);
	}
}

static void lafassspawner_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(LAFASSSPAWNER) = { lafassspawner_init, lafassspawner_ki, lafassspawner_explode, 0, 0 };
