/*
 * CLIMBSPIDER - the climbing spider (enemies/Gegner_Climber.cpp): crawls on the back wall,
 * turns towards the player and follows him, shoots SUCHSCHUSS; shot, it sails down spinning.
 * Value1 == 99: one from the spider machine (random start angle, no shooting).
 * Own members: f[0] = rot (degrees), f[1] = rotspeed, f[2] = shotdelay.
 * Simplified: Hurrican draws the frame rotated by rot; here the plain frame (no rotation).
 * Left out: the ROCKETSMOKE trail while falling and the FUNKE of the explosion (dropped types).
 */
/* the angle (0 < w <= 360) Hurrican's atan / quadrant code gives for dx, dy (spider - player) */
static fx climber_winkel(int dx, int dy)
{
	int w = atan2_d(-dx, dy);
	if (w <= 0) w += 360;
	return FX(w);
}

static void climber_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(20);
	e->AnimSpeed = FX(0.5);
	e->Destroyable = 1;
	e->AnimPhase = 3;
	e->AnimStart = 3;
	e->AnimEnde = 13;
	e->f[0] = 0;
	e->f[2] = FX(10);
	e->OwnDraw = 1;
	if (e->Value1 == 99) e->f[0] = FX((360 + GetRandom(50) - 50) % 360);
	e->f[1] = FX(GetRandom(8) + 3) / 2;
}

static void climber_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void climber_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const Box *rect = &GegnerRect[e->GegnerArt];
	e->blocku = (uint16_t)BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect);

	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		if (e->Handlung == GEGNER_LAUFEN)
		{
			e->AnimPhase--;
			if (e->AnimPhase <= e->AnimStart) e->AnimPhase = e->AnimEnde;
		}
		else if (e->Handlung == GEGNER_SCHIESSEN)
		{
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde)
			{
				e->Handlung = GEGNER_LAUFEN;
				e->AnimPhase = 3;
				e->AnimStart = 3;
				e->AnimEnde = 13;
			}
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:
		if (e->Value1 != 99)
			e->f[0] = climber_winkel(FINT(e->xPos) + 25 - (FINT(pl.xpos) + 35), FINT(e->yPos) + 18 - (FINT(pl.ypos) + 20));
		else
			e->f[0] = 0;
		e->Handlung = GEGNER_LAUFEN;
		break;

	case GEGNER_LAUFEN:
	{
		fx winkel = climber_winkel(FINT(e->xPos) + 25 - (FINT(pl.xpos) + 35), FINT(e->yPos) + 18 - (FINT(pl.ypos) + 40));
		if (e->Value1 == 99 && e->xPos - cam_xoff() < FX(250) && winkel > FX(75)) winkel = FX(75);

		const int abst = PlayerAbstand(e);
		fx inc = e->f[1];
		if (abst > 150) inc = e->f[1] * 2;
		if (abst > 250) inc = e->f[1] * 3;
		if ((e->blocko & B_WAND) || (e->blocku & B_WAND) || (e->blockl & B_WAND) || (e->blockr & B_WAND)) inc *= 5;
		if (e->f[0] < winkel) e->f[0] += SYNC(inc);
		if (e->f[0] > winkel) e->f[0] -= SYNC(inc);
		if (abst > 300 && e->Value1 != 99) e->f[0] = winkel;

		if (abst < 1000)
		{
			const int r = FINT(e->f[0]);
			e->xSpeed = FMUL(sin_d(r), FX(7.5));
			e->ySpeed = -FMUL(cos_d(r), FX(7.5));
		}
		else e->xSpeed = e->ySpeed = 0;

		if (e->xSpeed < 0 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) e->xSpeed = 0;
		if (e->xSpeed > 0 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))) e->xSpeed = 0;
		if (e->ySpeed < 0 && ((e->blocko & B_WAND) || (e->blocko & B_GEGNERWAND))) e->ySpeed = 0;
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND))) e->ySpeed = 0;

		e->f[2] -= SYNC(FX(1));
		if (e->f[2] < 0 && e->Value1 != 99 && abs(FINT(winkel - e->f[0])) < 10 && abst < 350)
		{
			e->f[2] = FX(GetRandom(10) + 10);
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimPhase = 0;
			e->AnimEnde = 3;
			e->AnimStart = 0;
			e->xSpeed = 0;
			e->ySpeed = 0;
			proj_push(e->xPos + FX(21), e->yPos + FX(13), SUCHSCHUSS);
			snd(SOUND_CANON);
		}
		break;
	}

	case GEGNER_FALLEN:
		e->f[0] += SYNC(FX(e->Value2));
		while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
		while (e->f[0] < 0) e->f[0] += FX(360);
		if ((e->blocko & B_WAND) || (e->blocku & B_WAND) || (e->blockl & B_WAND) || (e->blockr & B_WAND)) e->Energy = 0;
		e->f[2] -= SYNC(FX(1));
		if (e->f[2] < 0) e->f[2] = SYNC(FX(0.2));    /* (ROCKETSMOKE: dropped type) */
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);

	/* shot: an explosion, and the spider sails down */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Energy = FX(100);
		e->Handlung = GEGNER_FALLEN;
		e->xSpeed = FX(GetRandom(15)) - FX(7);
		e->ySpeed = -FX(GetRandom(8)) - FX(8);
		e->yAcc = FX(3);
		e->Value2 = (int16_t)(GetRandom(20) + 20);
		if (GetRandom(2) == 0) e->Value2 = (int16_t)-e->Value2;
		snd(SOUND_EXPLOSION1);
		part_push(e->xPos + FX(5), e->yPos, EXPLOSION_MEDIUM2);
		e->f[2] = FX(1);
	}
}

static void climber_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	part_push(e->xPos + FX(5), e->yPos, EXPLOSION_MEDIUM2);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos + FX(GetRandom(40)), e->yPos + FX(GetRandom(30)), SPIDERSPLITTER);
	pl.Score += 250;
}

TYPEDEF(CLIMBSPIDER) = { climber_init, climber_ki, climber_explode, climber_draw, 0 };
