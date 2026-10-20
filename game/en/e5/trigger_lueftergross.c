/*
 * LUEFTER_GROSS - the big fan (triggers/Trigger_LuefterGross.cpp): background decoration.
 * Own members: none.  Left out: its light colour (LightValue).
 */
static void lueftergross_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->Energy = FX(100);
	e->BackGround = 1;
}

static void lueftergross_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void lueftergross_ki(Ent *e)
{
	E5_TEST_HOOK(e);
}
static void lueftergross_explode(Ent *e) { (void)e; }

TYPEDEF(LUEFTER_GROSS) = { lueftergross_init, lueftergross_ki, lueftergross_explode, lueftergross_draw, 0 };
