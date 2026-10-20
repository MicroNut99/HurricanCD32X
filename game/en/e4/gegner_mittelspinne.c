/*
 * MITTELSPINNE - the middle-sized spider (enemies/Gegner_MittelSpinne.cpp): climbs down from the
 * ceiling during the Wuxe spiders' attack, turning slowly towards the player.
 * Own members: f[0] = rot (fx degrees), f[1] = WalkDelay (unused, as in Hurrican),
 *              f[2] = shotdelay, f[3] = yStart.
 * Drawing: Hurrican draws a shadow and the spider rotated by rot, darker near the screen's top
 * (coming out of the dark) and reddened when hit; here the plain frame (no shadow, no colour).
 * (mittelspinne.png is in none of the stage packs: no stage of this port has it.)
 */
static int mittelspinne_winkel(fx dx, fx dy)
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

static void mittelspinne_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->AnimSpeed = FX(GetRandom(8) + 5) / 8;
	e->Destroyable = 1;
	e->AnimEnde = 8;
	e->f[0] = FX(GetRandom(80) + 140);
	e->OwnDraw = 1;
	e->TestBlock = 0;
	e->f[2] = 0;
	e->Active = 1;
	e->f[3] = cam_yoff();
}

static void mittelspinne_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void mittelspinne_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
	{
		const fx winkel = FX(360 - mittelspinne_winkel((e->xPos + FX(25)) - (pl.xpos + FX(35)),
		                                               (e->yPos + FX(18)) - (pl.ypos + FX(40))));
		fx inc = FX(0.3);
		const fx k = FMUL(FX(15), FX(5) - e->AnimSpeed);
		if (e->yPos > pl.ypos) inc = 0;
		if (e->f[0] < winkel) e->f[0] += SYNC(inc);
		if (e->f[0] > winkel) e->f[0] -= SYNC(inc);
		/* move */
		e->xSpeed = SYNC(FMUL(sin_d(FINT(e->f[0])), k));
		e->ySpeed = SYNC(-FMUL(cos_d(FINT(e->f[0])), k));
	} break;

	case GEGNER_FALLEN:
		e->f[0] += SYNC(FX(e->Value2));
		while (e->f[0] < 0) e->f[0] += FX(360);
		while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
		if ((e->blocko & B_WAND) || (e->blocku & B_WAND) || (e->blockl & B_WAND) || (e->blockr & B_WAND) ||
		    (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		e->f[2] -= SYNC(FX(1));
		if (e->f[2] < 0)
		{
			e->f[2] = SYNC(FX(0.2));
			part_push(e->xPos + FX(20) + FX(GetRandom(5)), e->yPos + FX(15) + FX(GetRandom(5)), ROCKETSMOKE);
		}
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void mittelspinne_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos + FX(5), e->yPos, EXPLOSION_MEDIUM3);
	for (i = 0; i < 10; i++) part_push(e->xPos + FX(GetRandom(80)), e->yPos + FX(GetRandom(50)), SPIDERSPLITTER);
	for (i = 0; i < 30; i++) part_push(e->xPos + FX(GetRandom(80)), e->yPos + FX(GetRandom(50)), FUNKE);
	for (i = 0; i < 7; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(60)), e->yPos - FX(10) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(MITTELSPINNE) = { mittelspinne_init, mittelspinne_ki, mittelspinne_explode, mittelspinne_draw, 0 };
