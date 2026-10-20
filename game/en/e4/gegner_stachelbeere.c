/*
 * STACHELBEERE - the spiked berry (enemies/Gegner_Stachelbeere.cpp): rolls through the cave,
 * now and then stops, opens up and fires four homing mini rockets; only open it can be hurt.
 * Own members: i[0] = ShotCount, f[0] = RollCount.
 * Drawing: stachelbeere.png is cut into 60 x 60 frames (6 a row); Hurrican's 120 x 60 open
 * frames (SetRect) are two of them side by side.
 */
static void stachelbeere_init(Ent *e)
{
	e->f[0] = 0;
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(120);
	e->blick = -1;
	e->AnimSpeed = FX(0.3);
	e->AnimStart = 0;
	e->AnimEnde = 5;
	e->Destroyable = 0;
	e->OwnDraw = 1;
}

static void stachelbeere_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	const int mirrored = e->blick == 1;
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
	case GEGNER_LAUFEN2:
		draw_sprite(p, e->AnimPhase, e->xPos, e->yPos, mirrored);
		break;
	case GEGNER_OEFFNEN:
	case GEGNER_SCHLIESSEN:
	case GEGNER_SCHIESSEN:
	{
		const int k = e->AnimPhase < 0 ? 0 : e->AnimPhase;
		int l = (k / 3 + 1) * 6 + (k % 3) * 2;
		if (l > 34) l = 34;
		if (!mirrored)
		{
			/* RenderMirroredSprite at x - 60: the halves swapped and mirrored */
			draw_sprite(p, l + 1, e->xPos - FX(60), e->yPos, 1);
			draw_sprite(p, l, e->xPos, e->yPos, 1);
		}
		else
		{
			draw_sprite(p, l, e->xPos, e->yPos, 0);
			draw_sprite(p, l + 1, e->xPos + FX(60), e->yPos, 0);
		}
	} break;
	}
}

static void stachelbeere_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];

	/* slopes */
	if (e->ySpeed >= 0) e->blocku = (uint16_t)BlockSlopes(e->xPos, &e->yPos, rect, e->ySpeed);

	/* falling? */
	if (!(e->blocku & B_SCHRAEGE_R) && !(e->blocku & B_SCHRAEGE_L))
	{
		e->blocku = (uint16_t)BlockUnten(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM)) e->yAcc = FX(8);
	}

	/* the animation speed follows the speed */
	if (e->Handlung == GEGNER_LAUFEN || e->Handlung == GEGNER_LAUFEN2)
		e->AnimSpeed = FX(2.8) - FABS(e->xSpeed) / 4;

	if (e->ySpeed > 0)
	{
		if (e->ySpeed > FX(40)) e->ySpeed = FX(40);
		/* bounce on the ground? */
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			int i;
			for (i = 0; i < 10; i++) part_push(e->xPos + FX(GetRandom(60)), e->yPos + FX(50), FUNKE);
			for (i = 0; i < 5; i++) part_push(e->xPos + FX(GetRandom(40)), e->yPos + FX(40), SMOKE);
			e->ySpeed = -e->ySpeed / 2;
			if (e->ySpeed > FX(-5))
			{
				e->ySpeed = 0;
				e->yAcc = 0;
			}
		}
	}

	switch (e->Handlung)
	{
	/* opens up */
	case GEGNER_OEFFNEN:
		SimpleAnimation(e, 0);
		if (e->AnimPhase >= e->AnimEnde - 1)
		{
			e->i[0] = 4;
			e->f[0] = FX(4);
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimStart = -1;
			e->AnimPhase = (int16_t)(e->AnimEnde - 2);
		}
		break;

	/* fires */
	case GEGNER_SCHIESSEN:
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] < 0)
		{
			const int off = e->blick == -1 ? -60 : 25;
			snd(SOUND_GRANATE);
			gegner_push(e->xPos + FX(e->i[0] * 15 + off), e->yPos + FX(10), MINIROCKET, 0, 99);
			e->f[0] = FX(2);
			e->i[0]--;
			if (e->i[0] <= 0) e->Handlung = GEGNER_SCHLIESSEN;
		}
		break;

	/* closes again */
	case GEGNER_SCHLIESSEN:
		SimpleAnimation(e, 1);
		if (e->AnimPhase <= e->AnimStart + 1)
		{
			e->AnimStart = 0;
			e->AnimEnde = 5;
			e->Handlung = GEGNER_LAUFEN;
			e->f[0] = FX(50);
			e->Destroyable = 0;
		}
		break;

	/* brakes */
	case GEGNER_LAUFEN2:
	{
		int stop = 0;
		SimpleAnimation(e, 0);
		if (e->xSpeed > 0)
		{
			e->xSpeed -= SYNC(FX(1));
			if (e->xSpeed < 0) stop = 1;
		}
		else if (e->xSpeed < 0)
		{
			e->xSpeed += SYNC(FX(1));
			if (e->xSpeed > 0) stop = 1;
		}
		else stop = 1;
		/* stopped? */
		if (stop)
		{
			e->xSpeed = 0;
			e->Handlung = GEGNER_OEFFNEN;
			e->Destroyable = 1;
			e->AnimPhase = 0;
			e->AnimEnde = 16;
			e->AnimSpeed = FX(1);
			snd(SOUND_STEAM);
		}
	} break;

	/* rolls */
	case GEGNER_LAUFEN:
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		SimpleAnimation(e, 0);
		e->xAcc = e->blick == -1 ? FX(-10) : FX(10);
		if (e->xSpeed < FX(-10)) e->xSpeed = FX(-10);
		if (e->xSpeed > FX(10)) e->xSpeed = FX(10);
		/* turn at walls, or at the screen's edge when the screen is locked */
		if ((tile_zustand() == TS_LOCKED && e->blick == -1 && e->xPos < cam_xoff()) ||
		    (tile_zustand() == TS_LOCKED && e->blick == 1 && e->xPos > cam_xoff() + FX(RENDERWIDTH - 50)) ||
		    (e->blick == -1 && ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND))) ||
		    (e->blick == 1 && ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND))))
		{
			e->xSpeed = -e->xSpeed;
			e->blick = (int8_t)-e->blick;
		}
		if (e->ySpeed == 0 && e->f[0] <= 0 && (e->blocku & B_WAND) && PlayerAbstand(e) < 400)
		{
			e->Handlung = GEGNER_LAUFEN2;
			e->xAcc = 0;
		}
		break;
	}

	Wegschieben(e, rect, FX(4));
}

static void stachelbeere_explode(Ent *e)
{
	int i;
	for (i = 0; i < 5; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(60)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	part_push(e->xPos - FX(15), e->yPos - FX(15), EXPLOSION_BIG);
	snd(SOUND_EXPLOSION4);
	pl.Score += 300;
	/* a few more rockets */
	gegner_push(e->xPos + FX(15), e->yPos + FX(35), MINIROCKET, 0, 99);
	gegner_push(e->xPos + FX(45), e->yPos + FX(35), MINIROCKET, 0, 99);
	gegner_push(e->xPos + FX(30), e->yPos + FX(20), MINIROCKET, 0, 99);
	gegner_default_explode(e);
}

TYPEDEF(STACHELBEERE) = { stachelbeere_init, stachelbeere_ki, stachelbeere_explode, stachelbeere_draw, 0 };
