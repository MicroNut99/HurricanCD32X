/*
 * BALLERDRONE - the big flying drone (enemies/Gegner_BallerDrone.cpp): flies above the player
 * and fires two homing shots; shot down it sails down smoking and explodes at a wall.
 * Own members: f[0] = shotdelay.
 * Left out: the red tint while falling, the lava flare (an additive glow) above it.
 */
static void ballerdrone_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(120);
	e->Destroyable = 1;
	e->f[0] = FX(10);
	e->AnimCount = FX(0.1);
	e->OwnDraw = 1;
}

static void ballerdrone_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
	e->AlreadyDrawn = 1;
}

static void ballerdrone_ki(Ent *e)
{
	e->blick = -1;
	e->AnimCount -= SYNC(FX(1));
	if (e->AnimCount < 0)
	{
		e->AnimCount += FX(0.2);
		part_push(e->xPos + FX(37 + GetRandom(3)), e->yPos + FX(35), ROBOMANSMOKE);
	}

	if (e->xSpeed < FX(-20)) e->xSpeed = FX(-20);
	if (e->xSpeed > FX(20)) e->xSpeed = FX(20);
	if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);

	/* the frame from the speed */
	e->AnimPhase = (int16_t)(6 + FINT(e->xSpeed / 2));
	if (e->AnimPhase < 0) e->AnimPhase = 0;
	if (e->AnimPhase > 12) e->AnimPhase = 12;

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:   /* fly and shoot */
		if (e->xPos + FX(45) < pl.xpos + FX(35)) e->xAcc = FX(2.5);
		if (e->xPos + FX(45) > pl.xpos + FX(35)) e->xAcc = FX(-2.5);
		if (e->yPos < pl.ypos - FX(140)) e->yAcc = FX(2.5);
		if (e->yPos > pl.ypos - FX(140)) e->yAcc = FX(-2.5);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);
		if (PlayerAbstand(e) < 800) e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(15);
			proj_push(e->xPos + FX(12), e->yPos + FX(50), SUCHSCHUSS2);
			proj_push(e->xPos + FX(64), e->yPos + FX(50), SUCHSCHUSS2);
			snd(SOUND_GOLEMSHOT);
		}
		break;
	case GEGNER_FALLEN:   /* shot down: falls until it hits something */
		if ((e->blocko & B_WAND) || (e->blocku & B_WAND) || (e->blockl & B_WAND) ||
		    (e->blockr & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] < 0)
		{
			e->f[0] = SYNC(FX(5));
			part_push(e->xPos + FX(35 + GetRandom(5)), e->yPos + FX(20 + GetRandom(5)), ROCKETSMOKE);
			part_push(e->xPos + FX(30 + GetRandom(5)), e->yPos + FX(20 + GetRandom(5)), SMOKE3);
			part_push(e->xPos + FX(30 + GetRandom(5)), e->yPos + FX(20 + GetRandom(5)), FUNKE);
		}
		break;
	}

	if (e->Handlung != GEGNER_FALLEN) TestDamagePlayers(e, SYNC(FX(4)), 0);

	/* shot down: an explosion, then it falls */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Energy = FX(100);
		e->Handlung = GEGNER_FALLEN;
		e->yAcc = FX(3);
		snd(SOUND_EXPLOSION1);
		part_push(e->xPos - FX(20), e->yPos - FX(40), EXPLOSION_BIG);
		e->f[0] = FX(1);
	}
}

static void ballerdrone_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(90)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 30; i++)
		part_push(e->xPos - FX(40) + FX(GetRandom(90)), e->yPos - FX(30) + FX(GetRandom(60)), SMOKEBIG);
	for (int i = 0; i < 40; i++)
		part_push(e->xPos + FX(GetRandom(90)), e->yPos + FX(GetRandom(60)), FUNKE);
	pl.Score += 300;
}

TYPEDEF(BALLERDRONE) = { ballerdrone_init, ballerdrone_ki, ballerdrone_explode, ballerdrone_draw, 0 };
