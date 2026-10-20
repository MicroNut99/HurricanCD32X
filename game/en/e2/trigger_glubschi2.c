/*
 * GLUBSCHI2 - the goggle eye hanging from the ceiling (triggers/Trigger_Glubschi2.cpp): looks
 * after the player (the frame by his horizontal distance).
 * Own members: f[0] = rot, f[1] = rotspeed (the corona's turning).
 * Left out: the additive LavaFlare corona (so rot only turns).
 */
static void glubschi2_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->f[0] = 0;
	e->f[1] = FX(GetRandom(10) + 1) / 5;
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

static void glubschi2_draw(Ent *e)
{
	e->blick = -1;
	const fx a = e->xPos - pl.xpos;
	if (a < FX(200))
	{
		int anim = FINT((FX(200) - a) / 18);
		if (anim > 20) anim = 20;
		e->AnimPhase = (int16_t)anim;
	}
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void glubschi2_ki(Ent *e)
{
	E2DBG(e);
	e->f[0] += SYNC(e->f[1]);
	while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
}

static void glubschi2_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(GLUBSCHI2) = { glubschi2_init, glubschi2_ki, glubschi2_explode, glubschi2_draw, 0 };
