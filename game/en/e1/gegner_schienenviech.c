/*
 * SCHIENENVIECH - the rail creature (enemies/Gegner_SchienenViech.cpp): rides its rail towards
 * the player and fires straight up or down at the end of each animation loop.
 * Own members: none.
 * Left out: the grey tint (0xFFCCCCCC) of its picture.
 */
static void schienenviech_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(80);
	e->AnimSpeed = FX(1);
	e->AnimEnde = 4;
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->xAcc = FX(4);
}

static void schienenviech_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
	e->AlreadyDrawn = 1;
}

static void schienenviech_shoot(Ent *e)
{
	if (pl.ypos < e->yPos) proj_push(e->xPos + FX(18), e->yPos - FX(8), STRAIGHTSCHUSS);
	else proj_push(e->xPos + FX(18), e->yPos + FX(48), STRAIGHTSCHUSS2);
}

static void schienenviech_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	/* the walls (only to turn: it must not stick in its rail) */
	fx x = e->xPos;
	const fx y = e->yPos;
	e->blockl = (uint16_t)BlockLinks(&x, y, &x, y, rect, 0);
	x = e->xPos;
	e->blockr = (uint16_t)BlockRechts(&x, y, &x, y, rect, 0);

	/* (Hurrican tests blockl's wall for the right side too) */
	if ((((e->blockl & B_GEGNERWAND) || (e->blockl & B_WAND)) && e->xSpeed < 0) ||
	    (((e->blockr & B_GEGNERWAND) || (e->blockl & B_WAND)) && e->xSpeed > 0))
	{
		e->Handlung = GEGNER_LAUFEN;
		e->xSpeed = -e->xSpeed;
		e->xAcc = -e->xAcc;
	}

	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		/* forwards or backwards, as it rides */
		if (e->xSpeed < 0)
		{
			if (++e->AnimPhase >= e->AnimEnde) { e->AnimPhase = e->AnimStart; schienenviech_shoot(e); }
		}
		else
		{
			if (--e->AnimPhase < e->AnimStart) { e->AnimPhase = e->AnimEnde; schienenviech_shoot(e); }
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:      /* not following: player ahead? then follow again */
		if ((e->xSpeed > 0 && pl.xpos + FX(35) > e->xPos + FX(24)) || (e->xSpeed < 0 && pl.xpos + FX(35) < e->xPos + FX(24)))
			e->Handlung = GEGNER_VERFOLGEN;
		break;
	case GEGNER_VERFOLGEN:   /* follows the player */
		if (PlayerAbstandHoriz(e) < 250) e->xAcc = pl.xpos + FX(35) > e->xPos + FX(24) ? FX(4) : FX(-4);
		break;
	}

	if (e->xSpeed < FX(-20)) e->xSpeed = FX(-20);
	if (e->xSpeed > FX(20)) e->xSpeed = FX(20);

	/* slower when at the player */
	if (PlayerAbstandHoriz(e) < 60)
	{
		if (e->xSpeed > FX(15)) { e->xAcc = 0; e->xSpeed = FX(15); }
		if (e->xSpeed < FX(-15)) { e->xAcc = 0; e->xSpeed = FX(-15); }
	}
}

static void schienenviech_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(30)), e->yPos - FX(30) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
	pl.Score += 500;
}

TYPEDEF(SCHIENENVIECH) = { schienenviech_init, schienenviech_ki, schienenviech_explode, schienenviech_draw, 0 };
