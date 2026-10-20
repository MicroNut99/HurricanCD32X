/*
 * FETTERAKETE - the fat rocket (enemies/Gegner_FetteRakete.cpp), spat by the spider machine /
 * the dragon: flies in the direction Value1 (degrees, 0 = up); with Value2 == 99 (from the
 * dragon) it first rises, turns towards the player, then flies.
 * Own members: f[0] = SmokeDelay, f[1] = rot (fx degrees).
 * Drawing: no rotated sprites - pointing downwards (90..270 degrees) it is drawn upside down,
 * otherwise upright.
 */
static void fetterakete_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(50);
	e->Destroyable = 1;
	e->f[0] = 0;
	e->AnimEnde = 10;
	e->AnimSpeed = FX(0.3);
	e->OwnDraw = 1;
	e->f[1] = FX(e->Value1);
	e->blick = -1;
	if (e->Value2 == 99)   /* from the dragon */
	{
		e->xSpeed = 0;
		e->ySpeed = FX(-10);
		e->yAcc = FX(2);
		e->Handlung = GEGNER_DREHEN;
		e->AnimCount = FX(12);
	}
}

static void fetterakete_draw(Ent *e)
{
	int r = FINT(e->f[1]) % 360;
	if (r < 0) r += 360;
	if (r > 90 && r < 270) draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 1);
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void fetterakete_ki(Ent *e)
{
	e->blick = -1;
	switch (e->Handlung)
	{
	case GEGNER_INIT:     /* Value1 = the direction */
		e->xSpeed = sin_d(e->Value1) * 35;
		e->ySpeed = -cos_d(e->Value1) * 35;
		e->Handlung = GEGNER_LAUFEN;
		break;
	case GEGNER_DREHEN:   /* turns towards the player */
	{
		e->AnimCount -= SYNC(FX(1));
		const fx dx = (e->xPos + FX(12)) - (pl.xpos + FX(35));
		fx dy = (e->yPos + FX(17)) - (pl.ypos + FX(40));
		if (dy == 0) dy = 1;
		/* w = atan(dx / dy) in degrees */
		const int w = dy > 0 ? atan2_d(dx, dy) : atan2_d(-dx, -dy);
		int winkel = w;
		if (dx >= 0 && dy >= 0) winkel = w;
		else if (dx > 0 && dy < 0) winkel = 180 + w;
		else if (dx < 0 && dy > 0) winkel = 360 + w;
		else if (dx < 0 && dy < 0) winkel = 180 + w;
		winkel = 360 - winkel;
		if (e->f[1] < FX(winkel)) e->f[1] += SYNC(FX(25));
		if (e->f[1] > FX(winkel)) e->f[1] -= SYNC(FX(25));
		if (e->AnimCount < 0)   /* turned: off it goes */
		{
			e->Handlung = GEGNER_LAUFEN;
			snd(SOUND_ROCKET);
			e->xAcc = 0;
			e->yAcc = 0;
			e->xSpeed = sin_d(FINT(e->f[1])) * 35;
			e->ySpeed = -cos_d(FINT(e->f[1])) * 35;
		}
		break;
	}
	case GEGNER_LAUFEN:   /* flies */
		SimpleAnimation(e, 0);
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] < 0)
		{
			e->f[0] = FX(0.3);
			part_push(e->xPos + FX(2), e->yPos + FX(5), SMOKE);
		}
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) || (e->blocku & B_WAND))
			e->Energy = 0;
		break;
	default:
		break;
	}
	TestDamagePlayers(e, FX(25), 1);
}

static void fetterakete_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos - FX(10), e->yPos - FX(20), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 5; i++)
	{
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), MINIFLARE);
		part_push(e->xPos + FX(GetRandom(20)), e->yPos - FX(10) + FX(GetRandom(20)), SPIDERSPLITTER);
	}
	pl.Score += 50;
}

TYPEDEF(FETTERAKETE) = { fetterakete_init, fetterakete_ki, fetterakete_explode, fetterakete_draw, 0 };
