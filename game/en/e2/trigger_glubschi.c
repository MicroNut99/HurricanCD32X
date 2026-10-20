/*
 * GLUBSCHI - the goggle eye standing on the floor (triggers/Trigger_Glubschi.cpp): looks after
 * the player (the frame by his horizontal distance).
 * Own members: f[0] = rot, f[1] = rotspeed (the corona's turning).
 * Left out: the additive LavaFlare corona (so rot only turns).
 */
static void glubschi_init(Ent *e)
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

static void glubschi_draw(Ent *e)
{
	e->blick = -1;
	/* the frame by the player's horizontal distance */
	const fx a = e->xPos - pl.xpos;
	if (a < FX(200))
	{
		int anim = FINT((FX(200) - a) / 18);
		if (anim > 20) anim = 20;
		e->AnimPhase = (int16_t)anim;
	}
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void glubschi_ki(Ent *e)
{
	E2DBG(e);
	e->f[0] += SYNC(e->f[1]);          /* the corona turns */
	while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
}

static void glubschi_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(GLUBSCHI) = { glubschi_init, glubschi_ki, glubschi_explode, glubschi_draw, 0 };
