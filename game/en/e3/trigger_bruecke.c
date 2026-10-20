/*
 * BRUECKE - a piece of the hanging bridge (triggers/Trigger_Bruecke.cpp): hangs lower the nearer
 * the player (or an enemy standing on it) is. Only a picture: the player walks on the map's
 * platform tiles under it.
 * Hurrican's PushGegner passes the map's y as Value1 (the rest position); the constructor here
 * does not see the position yet, so Value1 is taken from yPos in the first DoKI (i[0] = done).
 * Own members: i[0] = Value1 set.
 * Hurrican's DETAIL_MAXIMUM behaviour (the default detail) is ported. For speed the enemies that
 * may stand on a piece are collected once a step (bruecke_cand: those active near the screen;
 * Hurrican tests all of them for every piece); the test itself is Hurrican's.
 */
#define BRUECKE_NCAND 24
static int16_t bruecke_cand[BRUECKE_NCAND];
static int bruecke_ncand, bruecke_last = 1 << 30;

static void bruecke_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->AnimCount = 0;
	e->TestBlock = 0;
	e->i[0] = 0;
}

/* the enemies near the screen that are not bridge pieces (once a step: the pieces run in slot
 * order, so a piece at a lower slot than the last one starts a new step) */
static void bruecke_collect(Ent *e)
{
	const int k = (int)(e - ents);
	if (k > bruecke_last) { bruecke_last = k; return; }
	bruecke_last = k;
	bruecke_ncand = 0;
	const fx x0 = cam_xoff() - FX(200), x1 = cam_xoff() + FX(RENDERWIDTH + 200);
	const fx y0 = cam_yoff() - FX(200), y1 = cam_yoff() + FX(RENDERHEIGHT + 200);
	for (int j = 0; j < MAX_LIVE && bruecke_ncand < BRUECKE_NCAND; j++)
	{
		const Ent *o = &ents[j];
		if (o->GegnerArt < 0 || !o->Active || o->GegnerArt == BRUECKE || o->GegnerArt == STAHLMUECKE) continue;
		if (o->xPos < x0 || o->xPos > x1 || o->yPos < y0 || o->yPos > y1) continue;
		bruecke_cand[bruecke_ncand++] = (int16_t)j;
	}
}

static void bruecke_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	if (!e->i[0]) { e->i[0] = 1; e->Value1 = (int16_t)FINT(e->yPos); }

	/* not on the screen? */
	if (e->xPos + FX(10) < cam_xoff() || e->xPos > cam_xoff() + FX(RENDERWIDTH) ||
	    e->yPos + FX(10) < cam_yoff() || e->yPos > cam_yoff() + FX(RENDERHEIGHT))
		return;

	e->yPos = FX(e->Value1);

	/* an enemy stands on the bridge */
	bruecke_collect(e);
	for (int c = 0; c < bruecke_ncand; c++)
	{
		const Ent *o = &ents[bruecke_cand[c]];
		if (o->GegnerArt < 0 || o->GegnerArt == BRUECKE || o->ySpeed != 0) continue;
		const Box *r = &GegnerRect[o->GegnerArt];
		if (!SpriteCollision(e->xPos, e->yPos, rect, o->xPos, o->yPos, r)) continue;
		const fx w = FX(r->r - r->l) / 2;
		const fx h = o->GegnerArt == DIAMANT ? FX(2) : FX(r->b - r->t) / 2;
		const int dx = FINT(o->xPos + w - (e->xPos + FX(5)));
		const int dy = FINT(o->yPos + h - (e->yPos + FX(5)));
		fx a = FX(isqrt(dx * dx + dy * dy));
		if (a > FX(40)) a = FX(40);
		if (o->GegnerArt == SPITTER) a += FX(13);
		e->yPos = o->yPos + FX(r->b) - a * 2 / 5 + FX(13);
	}

	/* the player on the bridge: the piece goes down by his distance */
	if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect) && pl.yspeed >= 0)
	{
		const int dx = FINT(pl.xpos + FX(35) - (e->xPos + FX(5)));
		const int dy = FINT(pl.ypos + FX(40) - (e->yPos + FX(5)));
		const fx a = FX(isqrt(dx * dx + dy * dy));
		e->yPos = FX(e->Value1) + (FX(32) - a * 2 / 3);
	}
}

static void bruecke_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(BRUECKE) = { bruecke_init, bruecke_ki, bruecke_explode, 0, 0 };
