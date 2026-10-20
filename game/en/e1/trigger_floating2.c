/*
 * FLOATING2 - the middle floating platform (triggers/Trigger_Floating2.cpp): dips down when
 * the player lands on it (the faster he falls the deeper), then rises back.
 * Own members: i[0] = playeron (AnimCount = the smoke timer, Value2 = the rest height).
 */
static void floating2_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->blick = 1;
	e->Energy = FX(100);
	if (e->Value1 == 0) e->Value1 = 16;
	e->Destroyable = 0;
	e->AnimCount = 0;
	e->i[0] = 0;
}

static void floating2_ki(Ent *e)
{
	e->blick = 1;
	e->i[0] = pl.AufPlattform == e;
	PlattformTest(e, &GegnerRect[e->GegnerArt]);

	/* the player landed this step: dip */
	if (pl.AufPlattform == e && !e->i[0])
	{
		e->i[0] = 1;
		if (e->ySpeed == 0)
		{
			e->Value2 = (int16_t)FINT(e->yPos);
			e->ySpeed = pl.yspeed / 3;
			e->yAcc = FX(-3);
		}
	}

	/* back at the rest height? */
	if (e->ySpeed < 0 && FINT(e->yPos) <= e->Value2)
	{
		e->ySpeed = 0;
		e->yAcc = 0;
		e->yPos = FX(e->Value2);
	}

	/* the jets' smoke */
	e->AnimCount -= SYNC(FX(1));
	if (e->AnimCount <= 0)
	{
		e->AnimCount = FX(0.15);
		part_push(e->xPos + FX(5), e->yPos + FX(9), FLOATSMOKE);
		part_push(e->xPos + FX(92), e->yPos + FX(9), FLOATSMOKE);
		part_push(e->xPos + FX(45), e->yPos + FX(12), FLOATSMOKE);
		part_push(e->xPos + FX(49), e->yPos + FX(12), FLOATSMOKE);
	}
}

static void floating2_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(FLOATING2) = { floating2_init, floating2_ki, floating2_explode, 0, 0 };
