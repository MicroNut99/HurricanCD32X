/*
 * LAVAMANN - the lava man (enemies/Gegner_Lavamann.cpp): runs towards the player leaving a wall
 * of fire behind, explodes when it touches him.
 * Own members: f[0] = FlameCount.
 * Drawing: Hurrican draws it additively (glowing); here plainly.
 */
static void lavamann_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->AnimStart = 0;
	e->AnimEnde = 20;
	e->AnimSpeed = FX(0.15);
	e->xSpeed = FX(24);
	e->ySpeed = 0;
	e->Energy = FX(20);
	e->Destroyable = 1;
	e->OwnDraw = 1;
	e->f[0] = 0;
}

static void lavamann_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == -1);
}

static void lavamann_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	/* slopes */
	if (e->ySpeed >= 0) e->blocku = (uint16_t)BlockSlopes(e->xPos, &e->yPos, rect, e->ySpeed);

	/* the wall of fire */
	e->f[0] -= SYNC(FX(1));
	if (e->f[0] < 0)
	{
		e->f[0] = FX(0.5);
		proj_push(e->xPos - FX(10) - FMUL(e->xSpeed, FX(0.75)), e->yPos, FEUERFALLE_LAVAMANN);
	}

	e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);

	/* towards the player when hit */
	if (e->DamageTaken > 0)
	{
		if (pl.xpos < e->xPos) { e->blick = -1; e->xSpeed = FX(-24); }
		else { e->blick = 1; e->xSpeed = FX(24); }
	}

	/* falling? */
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

			/* turn at walls */
			if (e->blick == -1)
				if ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))
				{
					e->blick = 1;
					e->xSpeed = -e->xSpeed;
				}
			if (e->blick == 1)
				if ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))
				{
					e->blick = -1;
					e->xSpeed = -e->xSpeed;
				}
		}
	}

	switch (e->Handlung)
	{
	/* first time on the screen: its sound */
	case GEGNER_INIT:
		snd(SOUND_FEUERFALLE);
		e->Handlung = GEGNER_LAUFEN;
		break;

	case GEGNER_FALLEN:
		if (e->ySpeed > FX(25)) e->ySpeed = FX(25);
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->yAcc = 0;
			e->ySpeed = 0;
		}
		break;
	}

	if (e->Handlung != GEGNER_WATSCHELN && e->Handlung != GEGNER_SPRINGEN) TestDamagePlayers(e, FX(35), 1);
}

static void lavamann_explode(Ent *e)
{
	int i;
	for (i = 0; i < 8; i++)
	{
		part_push(e->xPos - FX(30) + FX(GetRandom(40)), e->yPos - FX(30) + FX(GetRandom(50)), SMOKEBIG);
		part_push(e->xPos - FX(10) + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(50)), SMOKE3);
		part_push(e->xPos - FX(60) + FX(GetRandom(80)), e->yPos - FX(50) + FX(GetRandom(60)), EXPLOSIONFLARE);
		part_push(e->xPos - FX(40) + FX(GetRandom(40)), e->yPos - FX(30) + FX(GetRandom(50)), EXPLOSION_MEDIUM2);
		part_push(e->xPos - FX(20) + FX(GetRandom(50)), e->yPos - FX(20) + FX(GetRandom(50)), MINIFLARE);
	}
	snd(SOUND_EXPLOSION1);
	pl.Score += 350;
	gegner_default_explode(e);
}

TYPEDEF(LAVAMANN) = { lavamann_init, lavamann_ki, lavamann_explode, lavamann_draw, 0 };
