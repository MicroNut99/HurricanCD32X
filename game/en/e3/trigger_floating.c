/*
 * FLOATING - the small floating platform (triggers/Trigger_Floating.cpp): dips when the player
 * lands on it (the faster he falls, the deeper) and comes back up.
 * Own members: i[0] = playeron. Value2 keeps its rest height while it dips (as in Hurrican).
 */
static void floating_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->blick = 1;
	e->Energy = FX(100);
	if (e->Value1 == 0) e->Value1 = 16;
	e->Destroyable = 0;
	e->AnimCount = 0;
	e->i[0] = 0;
}

static void floating_ki(Ent *e)
{
	e->blick = 1;
	e->i[0] = pl.AufPlattform == e;

	PlattformTest(e, &GegnerRect[e->GegnerArt]);

	/* the player landed on it in this step? */
	if (pl.AufPlattform == e && !e->i[0])
	{
		e->i[0] = 1;
		if (e->ySpeed == 0)    /* dips down */
		{
			e->Value2 = (int16_t)FINT(e->yPos);
			e->ySpeed = pl.yspeed / 3;
			e->yAcc = FX(-3);
		}
	}

	/* back at its height? */
	if (e->ySpeed < 0 && FINT(e->yPos) <= e->Value2)
	{
		e->ySpeed = 0;
		e->yAcc = 0;
		e->yPos = FX(e->Value2);
	}

	e->AnimCount -= SYNC(FX(1));
	if (e->AnimCount <= 0)
	{
		e->AnimCount = FX(0.15);
		part_push(e->xPos + FX(14), e->yPos + FX(5), FLOATSMOKE);
	}
}

static void floating_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(FLOATING) = { floating_init, floating_ki, floating_explode, 0, 0 };
