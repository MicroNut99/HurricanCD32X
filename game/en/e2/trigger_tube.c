/*
 * TUBE - the tube a mutant may jump out of (triggers/Trigger_Tube.cpp). Value1: 0 whole,
 * 1 broken, 2 broken below only, 3 broken above only, 4 whole with a mutant inside, 5 whole with
 * a mutant that jumps out when the player comes near. Value2 1: liquid inside.
 * Own members: f[0] = SinOff (fx degrees; Hurrican: radians).
 * The liquid (RenderRect4 gradients, additive) is drawn as green lines: the surface and every
 * eighth row below it. Hurrican's PlattformTest in DoDraw (only on the screen) is in DoKI here,
 * on the same condition.
 */
static void tube_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->Energy = FX(100);
	e->BackGround = 1;
	e->OwnDraw = 1;
	e->f[0] = 0;
}

static void tube_liquid(Ent *e, int top, int h)
{
	const fx x0 = e->xPos + FX(18), x1 = e->xPos + FX(18 + 62 - 1);
	draw_line(x0, e->yPos + FX(top), x1, e->yPos + FX(top), col_green);
	for (int y = top + 8; y < top + 3 + h; y += 8)
		draw_line(x0, e->yPos + FX(y), x1, e->yPos + FX(y), col_green);
}

static void tube_draw(Ent *e)
{
	/* a mutant inside? */
	if (e->Value1 >= 4)
		draw_sprite(&GegnerPic[MUTANT], 0, e->xPos - FX(1), e->yPos + FX(60) + FMUL(sin_d(FINT(e->f[0])), FX(1.8)), 0);

	/* liquid inside? */
	if (e->Value2 == 1)
	{
		if (e->Value1 % 4 == 0 || e->Value1 == 5) tube_liquid(e, 75, 89);
		else tube_liquid(e, 145, 19);
	}

	/* and the tube over it */
	int a = e->Value1 % 4;
	if (e->Value1 == 5) a = 0;
	draw_sprite(&GegnerPic[e->GegnerArt], a, e->xPos, e->yPos, 0);
}

static void tube_ki(Ent *e)
{
	E2DBG(e);
	if ((e->Value1 == 0 || e->Value1 == 1 || e->Value1 == 4 || e->Value1 == 5) && IsOnScreen(e))
		PlattformTest(e, &GegnerRect[e->GegnerArt]);

	e->f[0] += SYNC(FX(0.3 * 57.29578));
	if (e->f[0] > FX(360)) e->f[0] = 0;

	/* the mutant can jump out? */
	if (e->Value1 == 5 && PlayerAbstand(e) < 100 && pl.ypos > e->yPos)
	{
		e->Value1 = 1;
		for (int i = 0; i < 100; i++)
			part_push(e->xPos + FX(5 + GetRandom(55)), e->yPos + FX(70 + GetRandom(60)), GLASSPLITTER);
		gegner_push(e->xPos - FX(1), e->yPos + FX(60), MUTANT, 1, 0);
		gegner_push(e->xPos + FX(50 - 10), e->yPos + FX(150), PARTIKELSPAWN, 11, 80);
		snd(SOUND_GLASSBREAK);
	}
}

static void tube_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(TUBE) = { tube_init, tube_ki, tube_explode, tube_draw, 0 };
