/*
 * COLUMN2 - the second column (triggers/Trigger_Column2.cpp): stands in the level; shot, it
 * topples over (turning about its foot) and bursts.
 * Own members: f[0] = FallSpeed, f[1] = FallValue (AnimCount = the angle, as Hurrican).
 * Left out: the turning of the picture while it falls (no rotated sprites: drawn upright until
 * it bursts), the level's screen rotation (ScreenWinkel), the screen shake, the thunder
 * flash (SetThunderColor).
 */
static void column2_init(Ent *e)
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

static void column2_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
}

static void column2_ki(Ent *e)
{
	/* shot down? */
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
	case GEGNER_EXPLODIEREN:   /* falling over */
		e->f[0] += SYNC(FX(0.5));
		e->AnimCount += SYNC(e->f[0]);
		e->f[1] += SYNC(FX(0.5));
		/* (Hurrican turns the screen back here while FallValue < 6) */
		e->Energy = FX(100);
		if (e->AnimCount >= FX(90) || e->AnimCount <= FX(-90)) e->Energy = 0;
		break;
	default:
		break;
	}

	if (e->Handlung == GEGNER_STEHEN) Wegschieben(e, &GegnerRect[e->GegnerArt], 0);
}

static void column2_explode(Ent *e)
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
}

TYPEDEF(COLUMN2) = { column2_init, column2_ki, column2_explode, column2_draw, 0 };
