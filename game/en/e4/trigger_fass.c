/*
 * FASS - the barrel (triggers/Trigger_Fass.cpp): stands about, can be stood on and pushed; shot
 * (or fallen onto the ground) it explodes into burning splinters (FLAMME) that hurt enemies.
 * Own members: none.
 * (fass.png is in none of the stage packs: no stage of this port has the barrel.)
 */
static void fass_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(10);
	e->AnimPhase = 0;
	e->Destroyable = 1;
}

static void fass_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];

	/* the player standing on it? */
	PlattformTest(e, rect);

	/* pushed */
	if (SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, e->xPos, e->yPos, rect))
	{
		e->xPos += SYNC(pl.xspeed / 2);
		pl.xpos -= SYNC(pl.xspeed / 2);
	}

	switch (e->Handlung)
	{
	/* waits for the player */
	case GEGNER_STEHEN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			e->ySpeed = FX(1);
			e->yAcc = FX(8);
			e->Handlung = GEGNER_FALLEN;
		}
		break;

	/* falls: explodes on the ground */
	case GEGNER_FALLEN:
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM)) e->Energy = 0;
		break;
	}
}

static void fass_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	for (i = 0; i < 5; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(48)), e->yPos - FX(50) + FX(GetRandom(56)), EXPLOSION_BIG);
	for (i = 0; i < 8; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(48)), e->yPos - FX(30) + FX(GetRandom(56)), EXPLOSION_MEDIUM);
	for (i = 0; i < 20; i++)
		part_push(e->xPos - FX(2) + FX(GetRandom(48)), e->yPos - FX(2) + FX(GetRandom(12)), FUNKE);
	/* the burning splinters */
	for (i = 0; i < 16; i++)
		proj_push(e->xPos - FX(24) + FX(GetRandom(48)), e->yPos + FX(GetRandom(20)), FLAMME);
	gegner_default_explode(e);
}

TYPEDEF(FASS) = { fass_init, fass_ki, fass_explode, 0, 0 };
