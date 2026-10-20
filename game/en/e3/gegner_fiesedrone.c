/*
 * FIESEDRONE - the small mean drone (enemies/Gegner_FieseDrone.cpp): flies above the player,
 * turns towards him and shoots lasers at him.
 * Own members: f[0] = ShotDelay, f[1] = SmokeDelay.
 */
static void fiesedrone_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(70);
	e->Destroyable = 1;
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->yAcc = 0;
	e->AnimSpeed = FX(1.5);
	e->AnimEnde = 12;
	e->f[0] = FX(GetRandom(12) + 12);
	e->f[1] = 0;
}

static void fiesedrone_ki(Ent *e)
{
	int flamex;

	/* animation */
	e->AnimCount += SPEEDFACTOR;
	if (e->Handlung == GEGNER_LAUFEN)
	{
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}
	else if (e->Handlung == GEGNER_DREHEN)
	{
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase > e->AnimEnde)
			{
				e->AnimCount = 0;
				e->Handlung = GEGNER_DREHEN2;
				e->blick = (int8_t)-e->blick;
				e->xPos -= e->blick * FX(5);
				e->AnimPhase = e->AnimEnde;
			}
		}
	}
	else if (e->Handlung == GEGNER_DREHEN2)
	{
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase--;
			if (e->AnimPhase <= 13)
			{
				e->AnimPhase = 0;
				e->AnimStart = 0;
				e->AnimEnde = 12;
				e->Handlung = GEGNER_LAUFEN;
				e->AnimSpeed = FX(1.5);
			}
		}
	}

	/* towards the player */
	if (e->xPos + FX(30) < pl.xpos + FX(35)) e->xAcc = FX(1);
	if (e->xPos + FX(30) > pl.xpos + FX(35)) e->xAcc = FX(-1);
	if (e->yPos < pl.ypos - FX(100)) e->yAcc = FX(1);
	if (e->yPos > pl.ypos - FX(100)) e->yAcc = FX(-1);

	if (e->xSpeed < FX(-10)) e->xSpeed = FX(-10);
	if (e->xSpeed > FX(10)) e->xSpeed = FX(10);
	if (e->ySpeed < FX(-8)) e->ySpeed = FX(-8);
	if (e->ySpeed > FX(8)) e->ySpeed = FX(8);

	if (e->Handlung == GEGNER_LAUFEN && PlayerAbstand(e) < 600)
	{
		if (e->f[0] > 0)
			e->f[0] -= SYNC(FX(1));
		else
		{
			e->f[0] = FX(GetRandom(12) + 12);
			/* Hurrican: newwinkel = atan(xdiv / ydiv) in degrees + 180, then by quadrant */
			const int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + FX(20)));
			const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(20)));
			int t = atan2_d(xdiv, ydiv);                  /* = atan(x / y) for y >= 0 */
			if (ydiv < 0) t = xdiv >= 0 ? t - 180 : t + 180;
			int w = t + 180;
			if (xdiv >= 0 && ydiv >= 0) { }
			else if (xdiv > 0 && ydiv < 0) w = 180 + w;
			else if (xdiv < 0 && ydiv > 0) w = 360 + w;
			else if (xdiv < 0 && ydiv < 0) w = 180 + w;
			proj_winkel = FX(360 - w);
			proj_push(e->xPos + FX(20), e->yPos + FX(20), FLUGLASER);
			part_push(e->xPos + FX(20), e->yPos + FX(20), SMOKE);
			snd(SOUND_LASERSHOT);
		}
	}

	/* (Hurrican's speed *= 1.0f at walls: no effect) */

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if ((e->xPos + FX(30) < pl.xpos + FX(35) && e->blick == -1) ||
		    (e->xPos + FX(30) > pl.xpos + FX(35) && e->blick == 1))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimPhase = 13;
			e->AnimStart = 13;
			e->AnimCount = 0;
			e->AnimEnde = 17;
			e->AnimSpeed = FX(0.5);
		}
		break;
	}

	/* smoke */
	if (e->Handlung == GEGNER_LAUFEN)
		flamex = e->blick == -1 ? 31 : -12;
	else
		flamex = e->blick == -1 ? 31 - (e->AnimPhase - 13) * 5 : -12 + (e->AnimPhase - 13) * 5;

	e->f[1] -= SYNC(FX(1));
	while (e->f[1] < 0)
	{
		e->f[1] += FX(0.7);
		part_push(e->xPos + FX(flamex), e->yPos + FX(28), SMOKE);
	}

	TestDamagePlayers(e, SYNC(FX(1)), 0);
}

static void fiesedrone_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(25) + FX(GetRandom(40)), e->yPos - FX(25) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos + FX(10) + FX(GetRandom(30)), e->yPos + FX(10) + FX(GetRandom(30)), LASERFUNKE2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 80;
	gegner_default_explode(e);
}

TYPEDEF(FIESEDRONE) = { fiesedrone_init, fiesedrone_ki, fiesedrone_explode, 0, 0 };
