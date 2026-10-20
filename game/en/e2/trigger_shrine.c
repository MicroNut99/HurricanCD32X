/*
 * SHRINE - the Manfred Trenz shrine (triggers/Trigger_Shrine.cpp): duck in front of it and it
 * rains extras (two 1-ups and eight extras), once.
 * Own members: none (AnimCount counts down as Hurrican's; the rock rain on shooting is commented
 * out in Hurrican too).
 * Left out: TileEngine.MaxOneUps += 2 (the level statistics).
 */
static void shrine_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->Value1 = 1;
	e->AnimCount = 0;
}

static void shrine_ki(Ent *e)
{
	E2DBG(e);
	e->blick = -1;
	if (e->AnimCount > 0) e->AnimCount -= SYNC(FX(1));
	else e->AnimCount = 0;

	/* the player kneels down */
	if (SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect) &&
	    pl.Handlung == PA_DUCKEN && e->Value1 == 1)
	{
		e->Value1 = 0;
		for (int i = 0; i < 2; i++) gegner_push(e->xPos - FX(20) + FX(i * 110), e->yPos - FX(100), ONEUP, 0, 0);
		for (int i = 0; i < 8; i++)
			gegner_push(e->xPos + FX(GetRandom(100)), e->yPos - FX(50) + FX(GetRandom(100)), EXTRAS, 3, 0);
		for (int i = 0; i < 6; i++) snd(SOUND_COLLECT);
	}
}

static void shrine_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(SHRINE) = { shrine_init, shrine_ki, shrine_explode, 0, 0 };
