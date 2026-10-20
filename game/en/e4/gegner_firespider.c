/*
 * FIRESPIDER - the fire-spitting climbing spider (enemies/Gegner_FireSpider.cpp): crawls on the
 * back wall, turns towards the player and burns him with a flame thrower; shot down it spins
 * away and explodes on a wall.
 * Own members: f[0] = rot (fx degrees), f[1] = shotdelay.
 * Drawing: Hurrican rotates the sprite by rot; here the plain frame, turned upside down
 * (flipped both ways) while rot points downwards.
 */
/* Hurrican's angle from (dx, dy): atan(dx / dy) in degrees, put into its quadrant */
static int firespider_winkel(fx dx, fx dy)
{
	int w;
	if (dy == 0) dy = 1;                          /* dy = 0.01f */
	w = dy > 0 ? atan2_d(dx, dy) : atan2_d(-dx, -dy);
	if (dx >= 0 && dy >= 0) return w;
	if (dx > 0 && dy < 0) return 180 + w;
	if (dx < 0 && dy > 0) return 360 + w;
	if (dx < 0 && dy < 0) return 180 + w;
	return w;
}

static void firespider_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(75);
	e->AnimSpeed = FX(0.75);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->AnimPhase = 0;
	e->AnimStart = 0;
	e->AnimEnde = 10;
	e->f[0] = 0;
	e->f[1] = FX(10);
}

static void firespider_draw(Ent *e)
{
	int r = FINT(e->f[0]) % 360;
	if (r < 0) r += 360;
	if (r > 135 && r < 225) draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 1);
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void firespider_ki(Ent *e)
{
	fx winkel;
	int ab;
	e->blocku = (uint16_t)BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, &GegnerRect[e->GegnerArt]);
	SimpleAnimation(e, 0);

	/* towards the player */
	winkel = FX(360 - firespider_winkel((e->xPos + FX(25)) - (pl.xpos + FX(35)), (e->yPos + FX(18)) - (pl.ypos + FX(40))));
	ab = PlayerAbstand(e);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
	{
		fx inc = FX(5);
		if (ab > 150) inc = FX(8);
		if (ab > 250) inc = FX(12);
		if (e->f[0] < winkel) e->f[0] += SYNC(inc);
		if (e->f[0] > winkel) e->f[0] -= SYNC(inc);
		if (ab > 300) e->f[0] = winkel;

		/* follow the player */
		if (ab < 1000)
		{
			e->xSpeed = FMUL(sin_d(FINT(e->f[0])), FX(7.5));
			e->ySpeed = -FMUL(cos_d(FINT(e->f[0])), FX(7.5));
		}
		else
		{
			e->xSpeed = 0;
			e->ySpeed = 0;
		}

		/* shoot when the counter ran out and it looks at the player */
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0 && abs(FINT(winkel - e->f[0])) < 10 && ab < 200)
		{
			e->f[1] = FX(0.3);
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimPhase = 11;
			e->AnimEnde = 11;
			e->AnimStart = 11;
			e->AnimCount = 0;
			e->xSpeed = 0;
			e->ySpeed = 0;
		}
	} break;

	/* fire until the player is out of the flame or the time is up */
	case GEGNER_SCHIESSEN:
		e->f[1] -= SYNC(FX(1));
		e->AnimCount += SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			fx xs = e->xPos + FX(8);
			fx ys = e->yPos - FX(13);
			int r = 180 - FINT(e->f[0]);
			if (r < 0) r += 360;
			xs = xs + sin_d(r) * 30;
			ys = ys + cos_d(r) * 30;
			proj_push(xs, ys, SPIDERFIRE);
			e->f[1] = FX(0.5);
		}
		if (abs(FINT(winkel - e->f[0])) > 10 || ab > 350 || e->AnimCount > FX(20))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->AnimPhase = 0;
			e->AnimStart = 0;
			e->AnimEnde = 10;
			e->f[1] = FX(GetRandom(20) + 20);
		}
		break;

	/* shot down: spins away */
	case GEGNER_FALLEN:
		e->f[0] += SYNC(FX(e->Value2));
		while (e->f[0] < 0) e->f[0] += FX(360);
		while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
		if ((e->blocko & B_WAND) || (e->blocku & B_WAND) || (e->blockl & B_WAND) || (e->blockr & B_WAND))
			e->Energy = 0;
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] < 0)
		{
			e->f[1] = SYNC(FX(8));
			part_push(e->xPos + FX(35) + FX(GetRandom(5)), e->yPos + FX(20) + FX(GetRandom(5)), ROCKETSMOKE);
			part_push(e->xPos + FX(30) + FX(GetRandom(5)), e->yPos + FX(20) + FX(GetRandom(5)), SMOKE3);
			part_push(e->xPos + FX(30) + FX(GetRandom(5)), e->yPos + FX(20) + FX(GetRandom(5)), FUNKE);
		}
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);

	/* shot down? an explosion, and it sails away */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Energy = FX(100);
		e->Handlung = GEGNER_FALLEN;
		e->xSpeed = FX(GetRandom(15)) - FX(7);
		e->ySpeed = -FX(GetRandom(8)) - FX(8);
		e->yAcc = FX(3);
		/* spin speed, maybe the other way */
		e->Value2 = (int16_t)(GetRandom(20) + 20);
		if (GetRandom(2) == 0) e->Value2 = (int16_t)-e->Value2;
		snd(SOUND_EXPLOSION1);
		part_push(e->xPos + FX(5), e->yPos, EXPLOSION_MEDIUM2);
		e->f[1] = FX(1);
	}
}

static void firespider_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos + FX(5), e->yPos, EXPLOSION_MEDIUM2);
	for (i = 0; i < 10; i++)
	{
		part_push(e->xPos + FX(GetRandom(40)), e->yPos + FX(GetRandom(30)), SPIDERSPLITTER);
		part_push(e->xPos + FX(GetRandom(40)), e->yPos + FX(GetRandom(30)), FUNKE);
	}
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(FIRESPIDER) = { firespider_init, firespider_ki, firespider_explode, firespider_draw, 0 };
