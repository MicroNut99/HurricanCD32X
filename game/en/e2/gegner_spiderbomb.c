/*
 * SPIDERBOMB - the spider bomb (enemies/Gegner_SpiderBomb.cpp): runs towards the player, crouches
 * when near, jumps at him and explodes on touching him. Value1 99: spat out by the spider machine.
 * Own members: none.
 */
static void spiderbomb_init(Ent *e)
{
	e->AnimStart = 0;
	e->AnimEnde = 5;
	e->AnimSpeed = FX(0.75);
	e->blick = -1;
	e->xAcc = 0;
	e->Energy = FX(30);
	e->Destroyable = 1;
	if (e->Value1 == 99)               /* spat out by the spider machine */
	{
		e->Handlung = GEGNER_FALLEN;
		e->AnimPhase = 6;
		e->xSpeed = FX(GetRandom(10) + 20);
		e->ySpeed = -FX(GetRandom(10) + 30);
		e->yAcc = FX(5);
	}
	else
	{
		e->Handlung = GEGNER_LAUFEN;
		e->xSpeed = 0;
		e->ySpeed = 0;
		e->yAcc = 0;
	}
}

static void spiderbomb_ki(Ent *e)
{
	E2DBG(e);
	const Box *rect = &GegnerRect[e->GegnerArt];
	if (e->Handlung == GEGNER_LAUFEN) SimpleAnimation(e, 0);

	/* turn at walls */
	if (e->blick == -1 && (e->blockl & B_WAND)) { e->blick = 1; e->xSpeed = -e->xSpeed; }
	if (e->blick == 1 && (e->blockr & B_WAND)) { e->blick = -1; e->xSpeed = -e->xSpeed; }

	if (e->ySpeed >= 0) e->blocku = (uint16_t)BlockSlopes(e->xPos, &e->yPos, rect, e->ySpeed);
	if (!(e->blocku & B_SCHRAEGE_R) && !(e->blocku & B_SCHRAEGE_L))
		e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))   /* no floor: fall */
		{
			e->Handlung = GEGNER_FALLEN;
			e->yAcc = FX(5);
			e->ySpeed = FX(1);
		}
		if (pl.xpos + FX(35) < e->xPos + FX(25)) { e->xAcc = FX(-4); e->blick = -1; }
		else { e->xAcc = FX(4); e->blick = 1; }
		if (e->xSpeed > FX(10)) { e->xSpeed = FX(10); e->xAcc = 0; }
		if (e->xSpeed < FX(-10)) { e->xSpeed = FX(-10); e->xAcc = 0; }
		if (PlayerAbstand(e) < 150)    /* lie in wait */
		{
			e->Handlung = GEGNER_SPECIAL;
			e->AnimPhase = 5;
			e->AnimCount = FX(5);
			e->xSpeed = 0;
			e->xAcc = 0;
			e->ySpeed = 0;
		}
		break;

	case GEGNER_SPECIAL:               /* crouch a moment */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->Handlung = GEGNER_FALLEN;
			e->AnimPhase = 6;
			e->ySpeed = FX(-22);
			e->yPos -= FX(5);
			e->yAcc = FX(5);
			e->xSpeed = pl.xpos + FX(35) > e->xPos + FX(15) ? FX(25) : FX(-25);
		}
		break;

	case GEGNER_FALLEN:                /* back on the floor: run again */
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimPhase = 0;
		}
		break;

	default:
		break;
	}

	TestDamagePlayers(e, FX(20), 1);
}

static void spiderbomb_explode(Ent *e)
{
	shake_screen(5);
	for (int i = 0; i < 6; i++)
		part_push(e->xPos + FX(60) + FX(GetRandom(80)), e->yPos + FX(40) + FX(GetRandom(30)), EXPLOSION_MEDIUM2);
	part_push(e->xPos - FX(20), e->yPos - FX(40), EXPLOSION_BIG);
	for (int i = 0; i < 4; i++) part_push(e->xPos + FX(10), e->yPos + FX(10), SPLITTER);
	snd(SOUND_EXPLOSION3);
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(SPIDERBOMB) = { spiderbomb_init, spiderbomb_ki, spiderbomb_explode, 0, 0 };
