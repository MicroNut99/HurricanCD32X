/*
 * RIESENQUALLE - the giant jellyfish (bosses/Boss_RiesenQualle.cpp): sinks, pushes itself up
 * towards the player with a puff of bubbles. No map of Hurrican places it (it is in the object
 * list only); ported for completeness.
 * Own members: none.
 * PC test: HB_B1_SPAWN=101 (the test hook in boss_skull.c).
 */
static void riesenqualle_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimStart = 0;
	e->AnimEnde = 6;
	e->AnimSpeed = FX(2.0);
	e->blick = -1;
	e->xSpeed = 0;
	e->ySpeed = FX(-8.0);
	e->yAcc = FX(0.4);
	e->Energy = FX(3000);
	e->Destroyable = 1;
}

#ifndef OJ32X_MARS
/* PC check: HB_B1LOG=1 prints the state every half second */
static void riesenqualle_log(const Ent *e)
{
	static int n;
	if (getenv("HB_B1LOG") && n++ % 30 == 0)
		fprintf(stderr, "riesenqualle: H %d E %d pos %d,%d speed %d,%d anim %d dest %d cam %d,%d state %d player %d,%d\n", e->Handlung,
		        FINT(e->Energy), FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->AnimPhase, e->Destroyable,
		        FINT(cam_xoff()), FINT(cam_yoff()), tile_zustand(), FINT(pl.xpos), FINT(pl.ypos));
}
#else
#define riesenqualle_log(e) ((void)0)
#endif

static void riesenqualle_ki(Ent *e)
{
	riesenqualle_log(e);
	/* animieren */
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase > e->AnimEnde) e->AnimPhase = e->AnimStart;
	}

	switch (e->Handlung)
	{
	case GEGNER_SPRINGEN:                  /* pushed up to swim */
		if (e->ySpeed > 0) e->yAcc = FX(0.25);
		if (e->ySpeed > FX(8.0)) e->yAcc = 0;
		if (e->AnimPhase >= e->AnimEnde)
		{
			e->AnimPhase = 0;
			e->AnimEnde = 0;
			e->AnimStart = 0;
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	case GEGNER_FALLEN:
		if (PlayerAbstand(e) < 200)
		{
			e->AnimPhase = 0;
			e->Handlung = GEGNER_LAUFEN;
			e->yAcc = 0;
		}
		break;
	case GEGNER_LAUFEN:                    /* sinks */
		if (e->yPos - FX(20) > pl.ypos || (e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND))
		{
			part_push(e->xPos + FX(20), e->yPos + FX(30), BUBBLE);
			e->xSpeed = e->xPos < pl.xpos ? FX(1.0) : FX(-1.0);
			e->AnimPhase = 1;
			e->AnimEnde = 6;
			e->AnimCount = 0;
			e->ySpeed = FX(-8.0);
			e->yAcc = FX(0.4);
			e->Handlung = GEGNER_SPRINGEN;
		}
		break;
	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(5.0)), 0);
}

static void riesenqualle_explode(Ent *e)
{
	int i;
	for (i = 0; i < 8; i++)
		part_push(e->xPos + FX(64) + FX(GetRandom(64)), e->yPos + FX(64) + FX(GetRandom(64)), PIRANHATEILE2);
	for (i = 0; i < 100; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(256)), e->yPos + FX(10) + FX(GetRandom(210)), BUBBLE);
	for (i = 0; i < 70; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(256)), e->yPos + FX(10) + FX(GetRandom(210)), PIRANHABLUT);
	pl.Score += 4000;
	gegner_default_explode(e);
}

TYPEDEF(RIESENQUALLE) = { riesenqualle_init, riesenqualle_ki, riesenqualle_explode, 0, 0 };
