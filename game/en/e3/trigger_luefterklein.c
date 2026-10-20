/*
 * LUEFTER_KLEIN - the small fan (triggers/Trigger_LuefterKlein.cpp): a turning background fan,
 * Value1 its animation speed (tenths; 0: 0.5).
 * Own members: none. Its lighting colour (LightValue, ForceLight) is left out.
 */
static void luefterklein_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->Energy = FX(100);
	e->AnimEnde = 11;
	e->BackGround = 1;
	e->AnimSpeed = e->Value1 == 0 ? FX(0.5) : FX(e->Value1) / 10;
}

static void luefterklein_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void luefterklein_ki(Ent *e)
{
	SimpleAnimation(e, 0);
}

static void luefterklein_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(LUEFTER_KLEIN) = { luefterklein_init, luefterklein_ki, luefterklein_explode, luefterklein_draw, 0 };
