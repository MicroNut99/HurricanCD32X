/*
 * DIAMANT - the diamond (enemies/Gegner_Diamant.cpp). An example of a port: the constructor,
 * DoKI and GegnerExplode, line by line, in Hurrican's units.
 * Own members: none.
 */
#define DIAMONDS_FOR_EXTRALIFE 50

static void diamant_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(1);
	e->AnimStart = 0;
	e->AnimEnde = 9;
	e->AnimSpeed = FX(0.75);
	e->Destroyable = 0;
}

static void diamant_ki(Ent *e)
{
	e->blick = -1;
	/* a short pause on frame 0, so it does not blink like a fire engine */
	e->AnimSpeed = e->AnimPhase == 0 ? FX(4) : FX(1);
	e->AnimCount += SYNC(FX(1));
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = 0;
	}
	switch (e->Handlung)
	{
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(35)) e->ySpeed = FX(35);
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			if (e->ySpeed > 0)
			{
				e->ySpeed = -e->ySpeed / 2;
				if (e->ySpeed > FX(-0.5))
				{
					e->ySpeed = 0;
					e->yAcc = 0;
					e->Handlung = GEGNER_STEHEN;
					e->yPos += FX(1);
				}
			}
		}
		break;
	case GEGNER_STEHEN:
		if (e->Value1 == 0)
		{
			const int bu = BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, &GegnerRect[e->GegnerArt], 0);
			if (!(bu & B_WAND) && !(bu & B_PLATTFORM))
			{
				e->yAcc = FX(2);
				e->Handlung = GEGNER_FALLEN;
			}
		}
		break;
	}
	if (SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
	{
		part_push(e->xPos, e->yPos, DIAMANTCOLLECTED);
		pl.Score += 100;
		pl.CollectedDiamonds++;
		pl.DiamondsThisLevel++;
		if (pl.CollectedDiamonds >= DIAMONDS_FOR_EXTRALIFE)
		{
			/* Hurrican adds time (no time limit here) and removes the Punisher */
			pl.CollectedDiamonds = 0;
		}
		e->Energy = 0;
	}
}

static void diamant_explode(Ent *e)
{
	(void)e;
	snd(SOUND_COLLECT);
}

TYPEDEF(DIAMANT) = { diamant_init, diamant_ki, diamant_explode, 0, 0 };
