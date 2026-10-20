/*
 * DECKENKRABBE - the ceiling crab (enemies/Gegner_DeckenKrabbe.cpp): hangs upside down on the
 * ceiling, drops (spinning) when shot or when the player passes below, then turns towards the
 * player and crawls at him (also up slopes).
 * Own members: f[0] = zRot (fx degrees), f[1] = zSpeed.
 * Drawing: no rotated sprites here - turned by more than 90 degrees it is drawn upside down
 * (flipped), else upright (the slope tilt of up to 45 degrees is not shown).
 * Hurrican tests the player's touch in DoDraw (once a frame, while on the screen); here it is
 * done in the step (while on the screen), so the damage per second is the same.
 */
static void deckenkrabbe_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(80);
	e->Destroyable = 1;
	e->f[0] = FX(180);
	e->OwnDraw = 1;
	e->AnimSpeed = FX(0.75);
}

static void deckenkrabbe_draw(Ent *e)
{
	const int mirror = e->blick != -1;
	const int rot = FINT(e->f[0]);
	if (rot > 90 && rot < 270) draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, !mirror);
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, mirror);
}

static void deckenkrabbe_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	while (e->f[0] < 0) e->f[0] += FX(360);
	while (e->f[0] >= FX(360)) e->f[0] -= FX(360);

	/* falling down? */
	if (e->Handlung != GEGNER_STEHEN)
	{
		if (!(e->blocku & B_SCHRAEGE_R) && !(e->blocku & B_SCHRAEGE_L))
		{
			e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
			if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
			{
				if (e->ySpeed == 0) e->ySpeed = FX(5);
				e->yAcc = FX(3);
			}
			else
			{
				e->ySpeed = 0;
				e->yAcc = 0;
			}
		}
	}

	if (e->Handlung == GEGNER_LAUFEN || e->Handlung == GEGNER_DREHEN || e->Handlung == GEGNER_DREHEN2)
		if (e->ySpeed >= 0) e->blocku = (uint16_t)BlockSlopes(e->xPos, &e->yPos, rect, e->ySpeed);

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:   /* on the ceiling */
		if (e->DamageTaken != 0 ||
		    (PlayerAbstandHoriz(e) < 100 && PlayerAbstandVert(e) < 800 && pl.ypos > e->yPos))
		{
			/* the spin from the distance to the floor */
			const int tx = FINT(e->xPos) / TILESIZE, ty = FINT(e->yPos) / TILESIZE;
			int a = 0;
			int b = TileBlock(tx, ty + a);
			while (a < 20 && !(b & B_WAND))
			{
				b = TileBlock(tx, ty + a);
				a++;
			}
			e->f[1] = FX(50) - a * FX(1.8);
			e->ySpeed = FX(5);
			e->yAcc = FX(5);
			e->Handlung = GEGNER_FALLEN;
		}
		break;
	case GEGNER_DREHEN:   /* landed, turns towards the player */
		e->f[0] = 0;
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase--;
			if (e->AnimPhase <= 0)
			{
				e->AnimPhase = 0;
				e->AnimCount = 0;
				e->Handlung = GEGNER_DREHEN2;
				e->blick = (int8_t)-e->blick;
			}
		}
		break;
	case GEGNER_DREHEN2:
		e->f[0] = 0;
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= 4)
			{
				e->AnimPhase = 5;
				e->AnimCount = 0;
				e->Handlung = GEGNER_LAUFEN;
			}
		}
		break;
	case GEGNER_LAUFEN:   /* crawls at the player */
		if (e->blick == 1)
		{
			if (e->blockl & B_SCHRAEGE_L)
			{
				if (e->f[0] < FX(45)) { e->f[0] += SYNC(FX(20)); if (e->f[0] > FX(45)) e->f[0] = FX(45); }
			}
			else if (e->f[0] > 0) { e->f[0] -= SYNC(FX(20)); if (e->f[0] < 0) e->f[0] = 0; }
		}
		if (e->blick == -1)
		{
			if (e->blockr & B_SCHRAEGE_R)
			{
				if (e->f[0] < FX(45)) { e->f[0] += SYNC(FX(20)); if (e->f[0] > FX(45)) e->f[0] = FX(45); }
			}
			else if (e->f[0] > 0) { e->f[0] -= SYNC(FX(20)); if (e->f[0] < 0) e->f[0] = 0; }
		}
		e->xPos += SYNC(FX(7) * -e->blick);
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= 14) e->AnimPhase = 5;
		}
		if ((e->blick == -1 && e->xPos + FX(35) > pl.xpos + FX(35)) ||
		    (e->blick == 1 && e->xPos + FX(35) < pl.xpos + FX(35)))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimCount = 0;
			e->AnimPhase = 4;
		}
		break;
	case GEGNER_FALLEN:   /* drops from the ceiling */
		if (e->ySpeed > FX(30)) { e->ySpeed = FX(30); e->yAcc = 0; }
		if (e->f[0] >= FX(180)) e->f[0] += SYNC(e->f[1]);
		else e->f[0] = 0;
		if (e->blocku & B_WAND)
		{
			e->Handlung = GEGNER_DREHEN2;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimPhase = 1;
			e->AnimEnde = 4;
			e->AnimSpeed = FX(0.75);
			e->Destroyable = 1;
			if (e->xPos + FX(35) < pl.xpos + FX(35)) e->blick = -1;
			if (e->xPos + FX(35) > pl.xpos + FX(35)) e->blick = 1;
		}
		break;
	default:
		break;
	}

	/* (DoDraw's test) */
	if (IsOnScreen(e)) TestDamagePlayers(e, SYNC(FX(8)), 0);
}

static void deckenkrabbe_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	part_push(e->xPos + FX(15), e->yPos + FX(10), LAVAKRABBE_KOPF);
	for (int i = 0; i < 4; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(10) + FX(GetRandom(10)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 4; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(10) + FX(GetRandom(10)), LAVAKRABBE_BEIN);
}

TYPEDEF(DECKENKRABBE) = { deckenkrabbe_init, deckenkrabbe_ki, deckenkrabbe_explode, deckenkrabbe_draw, 0 };
