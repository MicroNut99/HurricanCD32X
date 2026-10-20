/*
 * FLOATING3 - the big floating platform that turns over (triggers/Trigger_Floating3.cpp): dips
 * when the player lands on it, and a moment later turns over, dropping him.
 * Own members: f[0] = winkel, f[1] = turncount, f[2] = drehdir; i[0] = playeron.
 * Drawn: the standing surface (frame 0) and the middle part (frame 1); Hurrican turns the
 * surface by winkel - here it is drawn unturned (the player falls off all the same).
 */
static void floating3_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = 1;
	e->Energy = FX(100);
	if (e->Value1 == 0) e->Value1 = 16;
	e->Destroyable = 0;
	e->AnimCount = 0;
	e->i[0] = 0;
	e->f[0] = 0;
	e->f[1] = 0;
	e->f[2] = 0;
	e->OwnDraw = 1;
}

static void floating3_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	draw_sprite(p, 0, e->xPos, e->yPos, 0);
	draw_sprite(p, 1, e->xPos, e->yPos, 0);
}

static void floating3_ki(Ent *e)
{
	e->i[0] = pl.AufPlattform == e;

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		PlattformTest(e, &GegnerRect[e->GegnerArt]);
		/* the player landed on it in this step? */
		if (pl.AufPlattform == e && !e->i[0])
		{
			e->i[0] = 1;
			e->f[1] = FX(0.01);
			e->f[2] = pl.xpos + FX(35) < e->xPos + FX(50) ? FX(1) : FX(-1);
			if (e->ySpeed == 0)
			{
				e->Value2 = (int16_t)FINT(e->yPos);
				e->ySpeed = pl.yspeed / 5;
				e->yAcc = FX(-3);
			}
		}
		/* he was on it: count to the turn */
		if (e->f[1] > 0) e->f[1] += SYNC(FX(1));
		/* turn: he falls off */
		if (e->f[1] > FX(8))
		{
			if (pl.AufPlattform == e)
				e->f[2] = pl.xpos + FX(35) < e->xPos + FX(50) ? FX(1) : FX(-1);
			pl.AufPlattform = 0;
			e->f[1] = 0;
			e->Handlung = GEGNER_DREHEN;
		}
		break;
	case GEGNER_DREHEN:          /* turning */
		e->f[0] += SYNC(FMUL(FX(25), e->f[2]));
		if (e->f[0] > FX(180) || e->f[0] < FX(-180))
		{
			e->f[0] = 0;
			e->Handlung = GEGNER_STEHEN;
		}
		break;
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
		part_push(e->xPos + FX(44), e->yPos + FX(21), FLOATSMOKE);
		part_push(e->xPos + FX(27), e->yPos + FX(10), FLUGSACKSMOKE);
		part_push(e->xPos + FX(57), e->yPos + FX(10), FLUGSACKSMOKE2);
	}
}

static void floating3_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(FLOATING3) = { floating3_init, floating3_ki, floating3_explode, floating3_draw, 0 };
