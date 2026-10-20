/*
 * BROCKELROCK - the crumbling rock (enemies/Gegner_BrockelRock.cpp): crumbles once the player
 * stands on it, then falls and breaks on the ground.
 * Own members: none (AnimCount is the crumble counter, 255 -> 0, as Hurrican).
 * Simplified: Hurrican cross-fades frame 0 into frame 1 with AnimCount as alpha; here the
 * frame nearer to the blend is drawn (0 while AnimCount >= 128, then 1).
 */
static void brockelrock_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(10);
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->AnimCount = FX(255);
}

static void brockelrock_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimCount >= FX(128) ? 0 : 1, e->xPos, e->yPos, 0);
}

static void brockelrock_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	PlattformTest(e, &GegnerRect[e->GegnerArt]);
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		if (pl.AufPlattform == e) e->Handlung = GEGNER_SPRINGEN;
		break;
	case GEGNER_SPRINGEN:
		e->AnimCount -= SYNC(FX(50));
		if (e->AnimCount < 0)
		{
			e->AnimCount = 0;
			for (int i = 0; i < 5; i++)
			{
				part_push(e->xPos + FX(GetRandom(60)), e->yPos + FX(20), ROCKSPLITTERSMALL);
				part_push(e->xPos - FX(10) + FX(i * 10), e->yPos, SMOKEBIG);
			}
			snd(SOUND_STONEFALL);
			e->Handlung = GEGNER_FALLEN;
			e->yAcc = FX(4);
		}
		break;
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(50)) e->yAcc = 0;
		if (e->blocku & B_WAND) e->Energy = 0;
		break;
	}
}

static void brockelrock_explode(Ent *e)
{
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	for (int i = 0; i < 10; i++)
	{
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(20) + FX(GetRandom(20)), SMOKE);
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(GetRandom(40)), ROCKSPLITTER);
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(GetRandom(40)), ROCKSPLITTERSMALL);
	}
	snd(SOUND_STONEEXPLODE);
}

TYPEDEF(BROCKELROCK) = { brockelrock_init, brockelrock_ki, brockelrock_explode, brockelrock_draw, 0 };
