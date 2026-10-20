/*
 * SHOOTPLATTFORM - the platform with a button (triggers/Trigger_ShootPlattform.cpp): shooting
 * its button (SHOOTBUTTON, which the engine's gegner_make adds as Hurrican's PushGegner does)
 * lifts it; it sinks back to where it started.
 * Own members: i[0] = 1 once Value1 holds the start height. Hurrican's PushGegner passes it its
 * x, y as Wert1, Wert2 (Value1 = y); the constructor sees no position here, so its first step
 * takes Value1 from yPos.
 * Left out: the lava flare glow drawn while the button is being shot (additive).
 */
static void shootplattform_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->Value2 = 0;
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->TestBlock = 0;
	e->i[0] = 0;
}

static void shootplattform_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
	if (e->Value2 == 1) e->Value2 = 0;     /* (the glow while being shot is not drawn) */
}

static void shootplattform_ki(Ent *e)
{
	if (!e->i[0])
	{
		e->i[0] = 1;
		e->Value1 = (int16_t)FINT(e->yPos);
	}
	e->blick = -1;

	/* the player on it? */
	PlattformTest(e, &GegnerRect[e->GegnerArt]);

	if (e->ySpeed < FX(-30)) e->ySpeed = FX(-30);
	if (e->ySpeed > FX(30)) e->ySpeed = FX(30);

	/* back where it started? */
	if (e->ySpeed > 0 && FINT(e->yPos) > e->Value1)
	{
		e->yPos = FX(e->Value1);
		e->ySpeed = 0;
		e->yAcc = 0;
	}

	/* bounces off the ceiling (the player's head) */
	if (pl.AufPlattform == e)
	{
		fx y = pl.ypos, yo = pl.yposold;
		const int bo = BlockOben(pl.xpos, &y, pl.xposold, &yo, &pl.CollideRect, 0);
		if (e->ySpeed < 0 && (bo & B_WAND)) e->ySpeed = -e->ySpeed;
	}
}

static void shootplattform_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(SHOOTPLATTFORM) = { shootplattform_init, shootplattform_ki, shootplattform_explode, shootplattform_draw, 0 };
