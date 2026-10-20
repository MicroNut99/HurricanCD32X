/*
 * ZITRONE - the "Zieh-Drone" (enemies/Gegner_Zitrone.cpp): follows the player, picking a new
 * point above him now and then, turns its gun to him and fires FLUGLASER; shot down it falls
 * smoking and explodes on hitting a wall.
 * Own members: f[0] = SmokeDelay, f[1] = WackelOffset (fx degrees; Hurrican: radians),
 * f[2] = KnarreWinkel (fx degrees), f[3] = ShotDelay, f[4] = FollowCount, f[5] = NewX,
 * f[6] = NewY.
 * Drawing: the gun (Gegner.DroneGun, Zitronestiel.png) plain, not rotated, then the body.
 * Left out: the four additive LavaFlare glows at the engines.
 */
#define ZITRONE_RAD_DEG 57.29578

static void zitrone_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(200);
	e->Destroyable = 1;
	e->f[0] = FX(1);
	e->f[1] = 0;
	e->OwnDraw = 1;
	e->f[2] = 0;
	e->f[3] = 0;
	e->f[4] = 0;
}

/* the gun points at the player */
static void zitrone_knarre(Ent *e)
{
	int ydiv = FINT(pl.ypos + FX(40) - (e->yPos + FX(40)));
	const int xdiv = FINT(pl.xpos + FX(35) - (e->xPos + FX(60)));
	/* atan(xdiv / ydiv) in degrees (ydiv 0: a tiny positive number, as Hurrican) */
	int a = atan2_d(xdiv, ydiv);
	if (ydiv == 0) a = xdiv > 0 ? 90 : xdiv < 0 ? -90 : 0;
	if (a > 90) a -= 180;
	if (a < -90) a += 180;
	if (ydiv == 0) ydiv = 1;
	int w = a + 180;
	if (xdiv > 0 && ydiv < 0) w = 180 + w;
	else if (xdiv < 0 && ydiv > 0) w = 360 + w;
	else if (xdiv < 0 && ydiv < 0) w = 180 + w;
	while (w < 0) w += 360;
	while (w >= 360) w -= 360;
	if (w < 100) w = 100;
	if (w > 260) w = 260;
	if (e->f[2] < FX(w)) e->f[2] += SYNC(FX(20));
	if (e->f[2] > FX(w)) e->f[2] -= SYNC(FX(20));
}

static fx zitrone_wackel(const Ent *e)
{
	return sin_d(FINT(e->f[1])) * 10;  /* sin(WackelOffset) * 10 */
}

static void zitrone_draw(Ent *e)
{
	int a = e->AnimPhase;
	if (a < 3) a = 3;
	if (a > 7) a = 7;
	const fx yoff = zitrone_wackel(e);
	draw_sprite(&DroneGun, 0, e->xPos + FX(73 - a * 5), e->yPos + FX(48) + yoff, 0);
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos + yoff, 0);
}

static void zitrone_ki(Ent *e)
{
	E2DBG(e);
	const Box *rect = &GegnerRect[e->GegnerArt];
	e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 1);
	e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 1);
	e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 1);
	e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 1);

	zitrone_knarre(e);

	/* wobble */
	e->f[1] += SYNC(FX(0.25 * ZITRONE_RAD_DEG));
	while (e->f[1] > FX(360)) e->f[1] -= FX(360);

	/* the frame by the position to the player */
	int ap = 5 - FINT((pl.xpos + FX(35) - (e->xPos + FX(57))) / 50);
	if (ap < 0) ap = 0;
	if (ap > 10) ap = 10;
	e->AnimPhase = (int16_t)ap;

	if ((e->blockl & B_WAND) || (e->blockr & B_WAND)) e->xSpeed = 0;
	if ((e->blocko & B_WAND) || (e->blocku & B_WAND)) e->ySpeed = 0;

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:                /* hover */
		if (PlayerAbstand(e) > 300) e->f[4] = 0;   /* too far: follow at once */
		e->f[4] -= SYNC(FX(1));
		if (e->f[4] < 0)               /* a new point */
		{
			e->f[4] = FX(20);
			int dx = GetRandom(60) + 30;
			if (GetRandom(2) == 0) dx = -dx;
			e->f[5] = pl.xpos + FX(dx);
			e->f[6] = pl.ypos - FX(GetRandom(80)) - FX(80);
			e->xAcc = e->f[5] > e->xPos ? FX(5) : FX(-5);
			e->yAcc = e->f[6] > e->yPos ? FX(5) : FX(-5);
			e->Handlung = GEGNER_LAUFEN;
		}
		/* shoot */
		e->f[3] -= SYNC(FX(1));
		if (e->f[3] < 0)
		{
			e->f[3] = FX(5);
			proj_winkel = FX(360) - e->f[2];
			const int k = 180 - FINT(e->f[2]);
			proj_push(e->xPos + FX(53) - sin_d(k) * 45,
			          e->yPos + FX(56) + cos_d(k) * 45 + zitrone_wackel(e), FLUGLASER);
			snd(SOUND_LASERSHOT);
		}
		break;

	case GEGNER_LAUFEN:                /* fly to the new point */
		if (e->xSpeed < FX(-20)) e->xSpeed = FX(-20);
		if (e->xSpeed > FX(20)) e->xSpeed = FX(20);
		if (e->ySpeed < FX(-20)) e->ySpeed = FX(-20);
		if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
		if (e->xSpeed < 0 && e->xPos < e->f[5])
		{
			e->xAcc = 0;
			e->xSpeed += SYNC(FX(7));
			if (e->xSpeed > 0) e->xSpeed = 0;
		}
		if (e->xSpeed > 0 && e->xPos > e->f[5])
		{
			e->xAcc = 0;
			e->xSpeed -= SYNC(FX(7));
			if (e->xSpeed < 0) e->xSpeed = 0;
		}
		if (e->ySpeed < 0 && e->yPos < e->f[6])
		{
			e->yAcc = 0;
			e->ySpeed += SYNC(FX(7));
			if (e->ySpeed > 0) e->ySpeed = 0;
		}
		if (e->ySpeed > 0 && e->yPos > e->f[6])
		{
			e->yAcc = 0;
			e->ySpeed -= SYNC(FX(7));
			if (e->ySpeed < 0) e->ySpeed = 0;
		}
		if (e->xSpeed == 0 && e->ySpeed == 0)   /* there */
		{
			e->Handlung = GEGNER_STEHEN;
			e->f[4] = FX(20);
			e->f[3] = FX(5);
		}
		break;

	case GEGNER_FALLEN:                /* crashing */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.1);
			part_push(e->xPos + FX(20 + GetRandom(30)), e->yPos + FX(10 + GetRandom(40)), SMOKE);
			part_push(e->xPos + FX(20 + GetRandom(30)), e->yPos + FX(10 + GetRandom(40)), SMOKE3);
		}
		break;

	default:
		break;
	}

	/* shot down? */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->AnimCount = 0;
		e->Energy = FX(200);
		e->ySpeed = FX(4);
		e->yAcc = FX(2);
		e->xAcc = 0;
		e->xSpeed = GetRandom(2) == 0 ? FX(-5) : FX(5);
		snd(SOUND_EXPLOSION4);
		for (int i = 0; i < 8; i++)
		{
			part_push(e->xPos + FX(GetRandom(80)), e->yPos - FX(10) + FX(GetRandom(90)), EXPLOSION_MEDIUM2);
			part_push(e->xPos + FX(GetRandom(80)), e->yPos - FX(10) + FX(GetRandom(90)), SPIDERSPLITTER);
		}
	}
}

static void zitrone_explode(Ent *e)
{
	snd(SOUND_EXPLOSION4);
	for (int i = 0; i < 12; i++)
	{
		part_push(e->xPos + FX(GetRandom(80)), e->yPos - FX(10) + FX(GetRandom(90)), EXPLOSION_MEDIUM2);
		part_push(e->xPos + FX(GetRandom(80)), e->yPos - FX(10) + FX(GetRandom(90)), SPIDERSPLITTER);
		part_push(e->xPos + FX(10) + FX(GetRandom(80)), e->yPos - FX(10) + FX(GetRandom(90)), SCHROTT1);
	}
	for (int i = 0; i < 4; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(70)), e->yPos - FX(30) + FX(GetRandom(80)), SPLITTER);
	pl.Score += 400;
	gegner_default_explode(e);
}

TYPEDEF(ZITRONE) = { zitrone_init, zitrone_ki, zitrone_explode, zitrone_draw, 0 };
