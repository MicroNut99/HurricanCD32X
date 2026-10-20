/*
 * LUEFTER_KLEIN2 - the small broken fan (triggers/Trigger_LuefterKlein2.cpp): a background
 * animation; Value1 its speed (0: 0.5).
 * Own members: none. Left out: the forced lighting colour.
 */
static void luefterklein2_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->Energy = FX(100);
	e->AnimEnde = 11;
	e->BackGround = 1;
	e->AnimSpeed = e->Value1 == 0 ? FX(0.5) : FX(e->Value1) / 10;
}

static void luefterklein2_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void luefterklein2_ki(Ent *e)
{
	E2DBG(e);
	SimpleAnimation(e, 0);
}

static void luefterklein2_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(LUEFTER_KLEIN2) = { luefterklein2_init, luefterklein2_ki, luefterklein2_explode, luefterklein2_draw, 0 };
