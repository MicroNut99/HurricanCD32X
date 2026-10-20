/*
 * MINIROCKET - the small homing rocket (enemies/Gegner_MiniRocket.cpp), fired by the
 * Stachelbeere: flies up a moment, then homes in on the player; explodes on him or on a wall.
 * Own members: f[0] = SmokeDelay, f[1] = rot (fx degrees).
 * Left out: the pulsing lava flare drawn behind it (an additive glow; FlareSin only served it).
 * Drawing: Hurrican rotates the rocket by rot; here the plain frame.
 */
static int minirocket_winkel(fx dx, fx dy)
{
	int w;
	if (dy == 0) dy = 1;
	w = dy > 0 ? atan2_d(dx, dy) : atan2_d(-dx, -dy);
	if (dx >= 0 && dy >= 0) return w;
	if (dx > 0 && dy < 0) return 180 + w;
	if (dx < 0 && dy > 0) return 360 + w;
	if (dx < 0 && dy < 0) return 180 + w;
	return w;
}

static void minirocket_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(20);
	e->Destroyable = 1;
	e->f[0] = 0;
	e->OwnDraw = 1;
	e->ySpeed = FX(-20);
	e->AnimCount = FX(3);
	e->f[1] = 0;
}

static void minirocket_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
}

static void minirocket_ki(Ent *e)
{
	int r;
	e->blick = -1;

	/* first straight up a little */
	if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1));

	if (e->AnimCount <= 0)
	{
		/* the angle to the player */
		e->f[1] = FX(360 - minirocket_winkel((e->xPos + FX(6)) - (pl.xpos + FX(35)), (e->yPos + FX(12)) - (pl.ypos + FX(20))));

		/* towards the player */
		e->xAcc = e->xPos + FX(6) < pl.xpos + FX(35) ? FX(2.5) : FX(-2.5);
		e->yAcc = e->yPos + FX(12) < pl.ypos + FX(40) ? FX(2.5) : FX(-2.5);

		if (e->xSpeed < FX(-10)) e->xSpeed = FX(-10);
		if (e->xSpeed > FX(10)) e->xSpeed = FX(10);
		if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);
	}

	/* smoke */
	r = 360 - FINT(e->f[1]);
	e->f[0] -= SYNC(FX(1));
	while (e->f[0] < 0)
	{
		e->f[0] += FX(0.5);
		part_push(e->xPos - FX(8) + sin_d(r) * 15, e->yPos - FX(8) + cos_d(r) * 15, SMOKE);
	}

	TestDamagePlayers(e, FX(8), 1);

	/* explodes at walls */
	if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) || (e->blocku & B_WAND))
		e->Energy = 0;
}

static void minirocket_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	for (i = 0; i < 5; i++)
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), SMOKE);
	for (i = 0; i < 5; i++)
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), MINIFLARE);
	pl.Score += 50;
	gegner_default_explode(e);
}

TYPEDEF(MINIROCKET) = { minirocket_init, minirocket_ki, minirocket_explode, minirocket_draw, 0 };
