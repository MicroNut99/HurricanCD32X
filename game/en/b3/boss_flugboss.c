/*
 * FLUGBOSS - the flying boss (bosses/Boss_FlugBoss.cpp, "still to be invented"): in Hurrican's
 * source only a sketch is live - the hit zone below the screen rises and sinks at a random
 * place; it has 6000 energy and gives 6000 points when shot down; everything else of its DoKI (flying in, rails, cannons, shooting,
 * the energy bar, the screen lock) is commented out there, so it is not here either. No map of
 * the game has it, and its pictures (flugboss_*.png) are not in the packs, so here it is
 * invisible. (Also in Hurrican: its Handlung stays NOTVISIBLE, so Render never calls its DoDraw;
 * it is only hit in its GegnerRect, the eye's place, and the hit zone never becomes the
 * rectangle - kept so here.)
 * Parts (DoDraw, most at screen positions as Hurrican's): the two rails, the big / medium /
 * two mini cannons, the main body, the hit zone (two pictures), the two turrets (rotated in
 * Hurrican: plain frame), the eye (alpha-pulsed in Hurrican: plain).
 * Own members: the static struct below.
 * Left out: the shadow rectangle (alpha), the eye's alpha pulse.
 * Engine note: flugboss_hitzone.png / flugboss_hitzone2.png and flugboss_kanone_fett / _klein /
 * _mini.png share their first 16 characters (pic() compares 16) - if they are ever added to
 * the packs they need the order lookup the Spinnenmaschine uses.
 */
static struct {
	fx SchienePos, tempSpeed, yKanone[4], xKanone, Winkel[2], xHit, yHit, dHit;
} flb;
static Pic flugboss_schiene_l, flugboss_schiene_r, flugboss_kanone_big, flugboss_kanone_medium,
           flugboss_kanone_small, flugboss_geschuetz, flugboss_auge, flugboss_hitzone[2];

static void flugboss_load(void)
{
	flugboss_schiene_l = pic("flugboss_schiene_l.png");
	flugboss_schiene_r = pic("flugboss_schiene_r.png");
	flugboss_kanone_big = pic("flugboss_kanone_fett.png");
	flugboss_kanone_medium = pic("flugboss_kanone_klein.png");
	flugboss_kanone_small = pic("flugboss_kanone_mini.png");
	flugboss_geschuetz = pic("flugboss_geschuetz.png");
	flugboss_auge = pic("flugboss_auge.png");
	flugboss_hitzone[0] = pic("flugboss_hitzone.png");
	flugboss_hitzone[1] = pic("flugboss_hitzone2.png");
}

static void flugboss_init(Ent *e)
{
	memset(&flb, 0, sizeof flb);
	e->Handlung = GEGNER_NOTVISIBLE;
	e->Energy = FX(6000);
	e->AnimPhase = 0;
	e->AnimEnde = 0;
	e->AnimSpeed = 0;
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	flb.SchienePos = FX(320);
	flb.xHit = FX(GetRandom(440) + 100);
	flb.yHit = FX(100);
	flb.dHit = FX(-1);
	/* GegnerClass.cpp's rectangle (175, 182, 175 + 29, 182 + 30): game/hur_tables.h has the
	 * sums cut off */
	GegnerRect[FLUGBOSS] = (Box){ 175, 182, 175 + 29, 182 + 30 };
}

static void flugboss_draw(Ent *e)
{
	const fx xo = cam_xoff(), yo = cam_yoff();
	/* the rails (screen x) */
	draw_sprite(&flugboss_schiene_l, 0, xo - flb.SchienePos, e->yPos + FX(161), 0);
	draw_sprite(&flugboss_schiene_r, 0, xo + FX(320) + flb.SchienePos, e->yPos + FX(161), 0);
	/* the cannons */
	draw_sprite(&flugboss_kanone_big, 0, e->xPos + FX(121), e->yPos + FX(249) - flb.yKanone[0], 0);
	draw_sprite(&flugboss_kanone_medium, 0, e->xPos + FX(148), e->yPos + FX(254) - flb.yKanone[1], 0);
	draw_sprite(&flugboss_kanone_small, 0, e->xPos + FX(68), e->yPos + FX(261) - flb.yKanone[2], 0);
	draw_sprite(&flugboss_kanone_small, 0, e->xPos + FX(78), e->yPos + FX(251) - flb.yKanone[3], 0);
	/* (the shadow: an alpha rectangle, left out) */
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
	/* the hit zone (screen position) */
	draw_sprite(&flugboss_hitzone[0], 0, xo + flb.xHit, yo + FX(380) + flb.yHit, 0);
	draw_sprite(&flugboss_hitzone[1], 0, xo + flb.xHit, yo + FX(401) + flb.yHit, 0);
	GegnerRect[e->GegnerArt].l = (int16_t)FINT(flb.xHit + xo - e->xPos);
	GegnerRect[e->GegnerArt].r = (int16_t)(GegnerRect[e->GegnerArt].l + 52);
	GegnerRect[e->GegnerArt].t = (int16_t)FINT(FX(380) + yo - e->yPos + flb.yHit);
	GegnerRect[e->GegnerArt].b = (int16_t)(GegnerRect[e->GegnerArt].t + 100);
	/* the turrets (rotated by Winkel in Hurrican) */
	draw_sprite(&flugboss_geschuetz, 0, xo + flb.xKanone - FX(62), e->yPos + FX(160), 0);
	draw_sprite(&flugboss_geschuetz, 0, xo + FX(RENDERWIDTH) - flb.xKanone, e->yPos + FX(160), 0);
	/* the eye */
	draw_sprite(&flugboss_auge, 0, e->xPos + FX(175), e->yPos + FX(182), 0);
}

static void flugboss_ki(Ent *e)
{
	/* (Hurrican's whole fight here is commented out) */
	/* the hit zone rises and sinks */
	flb.yHit += SYNC(flb.dHit);
	if (flb.yHit < 0)
	{
		flb.yHit = 0;
		flb.dHit = FX(0.75);
	}
	if (flb.yHit > FX(100))
	{
		flb.yHit = FX(100);
		flb.dHit = FX(-3);
		flb.xHit = FX(GetRandom(440) + 100);
	}
}

static void flugboss_explode(Ent *e)
{
	for (int i = 0; i < 20; i++)
		part_push(e->xPos + FX(30) + FX(GetRandom(500)), e->yPos + FX(30) + FX(GetRandom(2000)), SPLITTER);
	pl.Score += 6000;
	music_stage();                          /* FadeSong(STAGEMUSIC, 2, 100) */
	gegner_default_explode(e);
}

TYPEDEF(FLUGBOSS) = { flugboss_init, flugboss_ki, flugboss_explode, flugboss_draw, flugboss_load };
