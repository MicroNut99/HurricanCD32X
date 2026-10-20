/*
 * ROTZPOTT - the gun pot (enemies/Gegner_Rotzpott.cpp): turns its gun towards the player and
 * shoots - in an arc (ROTZSHOT) when he is below it, else a laser (FLUGLASER).
 * Own members: f[0] = GunWinkel, f[1] = ShotDelay, f[2] = NewWinkel (fx degrees);
 *              i[0] = dir (CalcKnarreWinkel's static: -30 / 0 / 30, shared by all in Hurrican,
 *              kept per object here).
 * Drawing: the gun (Zitronestiel.png, DroneGun) unrotated (Hurrican turns it by GunWinkel).
 */
static fx rotzpott_clamp(fx w)
{
	while (w < 0) w += FX(360);
	while (w >= FX(360)) w -= FX(360);
	return w;
}

static void rotzpott_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(70);
	e->Destroyable = 1;
	e->f[0] = 0;
	e->f[2] = 0;
	e->OwnDraw = 1;
	e->TestBlock = 0;
	e->f[1] = FX(5);
	e->i[0] = 0;
}

/* the gun points at the player */
static void rotzpott_knarre(Ent *e)
{
	fx dx, dy;
	e->f[0] = rotzpott_clamp(e->f[0]);
	dx = (e->xPos + FX(27)) - (pl.xpos + FX(35));
	dy = (e->yPos + FX(20)) - (pl.ypos + FX(40));
	if (pl.ypos > e->yPos + FX(30))
	{
		if (dx > 0) e->f[2] = FMUL(dx, FX(0.12)) + FMUL(dy, FX(0.033));
		else e->f[2] = FMUL(dx, FX(0.12)) - FMUL(dy, FX(0.033));
	}
	else
	{
		int w;
		if (dy == 0) dy = 1;
		w = dy > 0 ? atan2_d(dx, dy) : atan2_d(-dx, -dy);
		if (dx >= 0 && dy >= 0) e->f[2] = FX(w);
		else if (dx > 0 && dy < 0) e->f[2] = FX(180 + w);
		else if (dx < 0 && dy > 0) e->f[2] = FX(360 + w);
		else if (dx < 0 && dy < 0) e->f[2] = FX(180 + w);
	}
	e->f[2] = rotzpott_clamp(e->f[2]);

	/* which way to turn */
	if (e->f[2] > e->f[0]) e->i[0] = e->f[2] - e->f[0] > FX(180) ? -30 : 30;
	if (e->f[2] < e->f[0]) e->i[0] = e->f[0] - e->f[2] < FX(180) ? -30 : 30;

	/* towards the new angle */
	if (e->i[0] < 0)
	{
		e->f[0] += SYNC(FX(e->i[0]));
		if (e->f[0] < e->f[2]) e->f[0] = e->f[2];
	}
	else if (e->i[0] > 0)
	{
		e->f[0] += SYNC(FX(e->i[0]));
		if (e->f[0] > e->f[2]) e->f[0] = e->f[2];
	}
}

static void rotzpott_draw(Ent *e)
{
	draw_sprite(&DroneGun, 0, e->xPos + FX(20), e->yPos - FX(28), 0);
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
	e->AlreadyDrawn = 1;
}

static void rotzpott_ki(Ent *e)
{
	TestDamagePlayers(e, SYNC(FX(2)), 0);
	rotzpott_knarre(e);

	/* shoot when the player is near and the gun nearly there */
	if (PlayerAbstand(e) < 400 && abs(FINT(e->f[0] - e->f[2])) < 10) e->f[1] -= SYNC(FX(1));

	if (e->f[1] <= 0)
	{
		const int g = 180 - FINT(e->f[0]);
		part_push(e->xPos - sin_d(g) * 30, e->yPos - FX(18) + cos_d(g) * 38, LASERFLAMEPHARAO);
		/* in an arc or straight? */
		if (pl.ypos > e->yPos + FX(30))
		{
			e->f[1] = FX(9);
			snd(SOUND_LASERSHOT);
			proj_winkel = FX(360) - e->f[0] + FX(GetRandom(10) - 5) / 2;
			proj_push(e->xPos + FX(20) - sin_d(g) * 25, e->yPos - FX(12) + cos_d(g) * 20, ROTZSHOT);
		}
		else
		{
			e->f[1] = FX(7);
			snd(SOUND_LASERSHOT);
			proj_winkel = FX(360) - e->f[0];
			proj_push(e->xPos + FX(24) - sin_d(g) * 45, e->yPos - FX(12) + cos_d(g) * 45, FLUGLASER);
		}
	}
}

static void rotzpott_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	for (i = 0; i < 8; i++)
	{
		part_push(e->xPos - FX(10) + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(20)), EXPLOSION_MEDIUM2);
		part_push(e->xPos - FX(10) + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(20)), SPIDERSPLITTER);
	}
	for (i = 0; i < 4; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(20)), SCHROTT1);
	pl.Score += 350;
	gegner_default_explode(e);
}

TYPEDEF(ROTZPOTT) = { rotzpott_init, rotzpott_ki, rotzpott_explode, rotzpott_draw, 0 };
