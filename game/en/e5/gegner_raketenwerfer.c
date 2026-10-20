/*
 * RAKETENWERFER - the rocket launcher (enemies/Gegner_Raketenwerfer.cpp): on the ceiling or
 * floor, fires rockets when the player comes near (below it).
 * Own members: none.
 */
static void raketenwerfer_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(60);
	e->Destroyable = 1;
	e->AnimSpeed = FX(1.3);
	e->AnimEnde = 9;
	e->TestBlock = 0;
}

static void raketenwerfer_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	if (PlayerAbstand(e) < 600 && pl.ypos > e->yPos) e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase == 6)
		{
			snd(SOUND_GRANATE);
			proj_push(e->xPos + FX(2), e->yPos + FX(5), ROCKETWERFER);
		}
		if (e->AnimPhase > e->AnimEnde)
		{
			e->AnimPhase = e->AnimStart;
			e->AnimCount = FX(-2);           /* a short pause */
		}
	}
}

static void raketenwerfer_explode(Ent *e)
{
	part_push(e->xPos - FX(20), e->yPos - FX(15), EXPLOSION_BIG);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos - FX(8) + FX(GetRandom(40)), e->yPos - FX(8) + FX(GetRandom(20)), SMOKE);
	snd(SOUND_EXPLOSION4);
	pl.Score += 400;
}

TYPEDEF(RAKETENWERFER) = { raketenwerfer_init, raketenwerfer_ki, raketenwerfer_explode, 0, 0 };
