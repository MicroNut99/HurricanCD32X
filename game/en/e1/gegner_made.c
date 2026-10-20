/*
 * MADE - the maggot (enemies/Gegner_Made.cpp), spat by the Bratklops (Value1 98: thrown to the
 * right) and the Schwabbel (Value1 99: thrown anywhere): falls, crawls slowly at the player,
 * dies by itself after a while.
 * Own members: f[0] = movesin (fx radians).
 * Drawing: Hurrican squeezes the crawling frame in width (RenderSpriteScaled 20 +- 5 pixels);
 * here the plain frame 15, moved left / right as Hurrican's squeezed picture's left edge. The
 * wobble is advanced in the step (Hurrican: in DoDraw, once a frame).
 */
static void made_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->HitSound = 1;
	e->Energy = FX(10);
	e->AnimStart = 0;
	e->AnimEnde = 20;
	e->AnimSpeed = FX(GetRandom(10) + 5) / 20;
	e->AnimCount = 0;
	e->Destroyable = 1;
	e->AnimPhase = 16;
	e->OwnDraw = 1;
	e->f[0] = 0;
	if (e->Value1 == 98)         /* only to the right (Bratklops) */
	{
		e->AnimPhase = (int16_t)GetRandom(20);
		e->yAcc = FX(4);
		e->xSpeed = FX(GetRandom(120)) / 3;
		e->ySpeed = -(FX(GetRandom(40)) / 3 + FX(8));
		e->Handlung = GEGNER_FALLEN;
	}
	else if (e->Value1 == 99)    /* anywhere (Schwabbel) */
	{
		e->AnimPhase = (int16_t)GetRandom(20);
		e->yAcc = FX(4);
		e->xSpeed = FX(GetRandom(200) - 100) / 12;
		e->ySpeed = -(FX(GetRandom(40)) / 2 + FX(12));
		e->Handlung = GEGNER_FALLEN;
	}
}

static void made_draw(Ent *e)
{
	if (e->Handlung == GEGNER_LAUFEN)
	{
		const int deg = FINT(FMUL(e->f[0], FX(57.29578)));
		draw_sprite(&GegnerPic[e->GegnerArt], 15, e->xPos - FMUL(sin_d(deg), FX(2.5)), e->yPos, 0);
	}
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void made_ki(Ent *e)
{
	/* (DoDraw's wobble) */
	e->f[0] += SYNC(FX(0.8));
	if (e->f[0] > FX(6.2831853)) e->f[0] = 0;

	/* dies slowly */
	e->Energy -= SYNC(FX(0.1));
	e->blick = -1;

	if (e->Handlung == GEGNER_INIT)
	{
		e->Handlung = GEGNER_LAUFEN;
		e->xSpeed = e->xPos + FX(10) < pl.xpos + FX(35) ? FX(0.8) : FX(-0.8);
	}

	if (e->Handlung == GEGNER_FALLEN)
	{
		SimpleAnimation(e, 0);
		if ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND) || (e->blocku & B_PLATTFORM))
			e->Handlung = GEGNER_LAUFEN;
	}
	else
	{
		e->AnimPhase = 15;
		e->yAcc = FX(4);
		e->ySpeed = 0;
		if (!(e->blocku & B_WAND) && !(e->blocku & B_GEGNERWAND) && !(e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_FALLEN;
			e->yAcc = FX(4);
		}
	}

	/* turn at walls */
	if ((e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
	    (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
		e->xSpeed = -e->xSpeed;

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void made_explode(Ent *e)
{
	snd(SOUND_MADE);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(24)), e->yPos - FX(12) + FX(GetRandom(22)), MADEBLUT);
}

TYPEDEF(MADE) = { made_init, made_ki, made_explode, made_draw, 0 };
