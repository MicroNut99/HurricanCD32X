/*
 * ONEUP - the extra life (enemies/Gegner_OneUp.cpp): falls, bounces, lies; collected it gives a
 * life and 1000 points.
 * Own members: none.
 * Left out: moving on conveyor belts (the map keeps no BLOCKWERT_FLIESSBAND flags).
 */
static void oneup_init(Ent *e)
{
	e->Handlung = GEGNER_FALLEN;
	e->Energy = FX(1);
	if (e->Value2 != 99) e->yAcc = FX(2);
	e->AnimSpeed = FX(0.75);
	e->Destroyable = 0;
	e->AnimEnde = 20;
	e->AnimSpeed = FX(0.5);
}

#ifndef OJ32X_MARS
/* PC test hook: HB_E4_SPAWN="art,dx,dy[,v1,v2]" makes object art at the player's position
 * + (dx, dy) once, when the first extra life runs (warp next to one) */
static void oneup_test_spawn(void)
{
	static int done;
	const char *s = getenv("HB_E4_SPAWN");
	int art = 0, dx = 0, dy = 0, v1 = 0, v2 = 0;
	if (done || !s) return;
	done = 1;
	if (sscanf(s, "%d,%d,%d,%d,%d", &art, &dx, &dy, &v1, &v2) >= 3)
		gegner_push(pl.xpos + FX(dx), pl.ypos + FX(dy), art, v1, v2);
}
#endif

static void oneup_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
#ifndef OJ32X_MARS
	oneup_test_spawn();
#endif
	SimpleAnimation(e, 0);
	e->blick = -1;

	switch (e->Handlung)
	{
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(50)) e->ySpeed = FX(50);
		/* bounce on the ground? */
		if ((BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect) & B_WAND) ||
		    (BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect) & B_PLATTFORM))
		{
			e->yPos -= FX(1);
			e->ySpeed = -FMUL(e->ySpeed, FX(0.2));
			if (e->ySpeed > FX(-0.5))
			{
				e->xSpeed = 0;
				e->ySpeed = 0;
				e->yAcc = 0;
				e->Handlung = GEGNER_SPECIAL;
			}
		}
		break;

	case GEGNER_STEHEN:
		if (!(BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect) & B_WAND) &&
		    !(BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect) & B_PLATTFORM))
		{
			e->Handlung = GEGNER_FALLEN;
			e->yAcc = FX(2);
		}
		break;
	}

	/* collected? */
	if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
	{
		int j;
		pl.Score += 1000;
		pl.Lives++;
		pl.LivesThisLevel++;
		e->Energy = 0;
		part_push(e->xPos - FX(40), e->yPos - FX(40), EXPLOSIONFLARE);
		part_push(e->xPos - FX(40), e->yPos - FX(40), EXPLOSIONFLARE);
		for (j = 0; j < 200; j++) part_push(e->xPos + FX(20), e->yPos + FX(20), KRINGELSECRET);
	}
}

static void oneup_explode(Ent *e)
{
	snd(SOUND_COLLECT);
	snd(SOUND_VOICE_EXTRALIFE);
	gegner_default_explode(e);
}

TYPEDEF(ONEUP) = { oneup_init, oneup_ki, oneup_explode, 0, 0 };
