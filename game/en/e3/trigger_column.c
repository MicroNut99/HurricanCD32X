/*
 * COLUMN - the column (triggers/Trigger_Column.cpp): stands in the level and falls over when
 * shot, then breaks apart.
 * Own members: f[0] = FallSpeed, f[1] = FallValue. AnimCount is the fall angle (as in Hurrican).
 * Left out: the screen's tilt while it falls (ScreenWinkel), the white thunder flash
 * (SetThunderColor), the screen shake. Drawn: the plain frame (Hurrican rotates it about its
 * foot as it falls). (Its picture column.png is in no stage pack: the type is in no map either.)
 */
static void column_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(120);
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->f[0] = 0;
	e->f[1] = 0;
}

static void column_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
}

static void column_ki(Ent *e)
{
	/* shot over? */
	if (e->Energy < FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		for (int i = 0; i < 20; i++)
			part_push(e->xPos + FX(GetRandom(40)), e->yPos - FX(20) + FX(GetRandom(15)), SMOKE);
		for (int i = 0; i < 10; i++)
			part_push(e->xPos + FX(GetRandom(40)), e->yPos - FX(5) + FX(GetRandom(10)), ROCKSPLITTERSMALL);
		e->f[0] = FX(2);
		snd(SOUND_STONEFALL);
		snd(SOUND_COLUMN);
	}

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		break;
	case GEGNER_EXPLODIEREN:     /* falling over */
		e->f[0] += SYNC(FX(0.5));
		e->AnimCount += SYNC(e->f[0]);
		e->f[1] += SYNC(FX(0.5));
		e->Energy = FX(100);
		if (e->AnimCount >= FX(90) || e->AnimCount <= FX(-90)) e->Energy = 0;
		break;
	}

	if (e->Handlung == GEGNER_STEHEN) Wegschieben(e, &GegnerRect[e->GegnerArt], 0);
}

static void column_explode(Ent *e)
{
	for (int i = 0; i < 30; i++)
		part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(80) + FX(GetRandom(40)), SMOKE);
	for (int i = 0; i < 40; i++)
	{
		part_push(e->xPos + FX(10) + FX(GetRandom(90)), e->yPos + FX(60) + FX(GetRandom(40)), ROCKSPLITTERSMALL);
		part_push(e->xPos + FX(10) + FX(GetRandom(90)), e->yPos + FX(60) + FX(GetRandom(40)), ROCKSPLITTER);
	}
	snd(SOUND_STONEEXPLODE);
	shake_screen(2);
	gegner_default_explode(e);
}

TYPEDEF(COLUMN) = { column_init, column_ki, column_explode, column_draw, 0 };
