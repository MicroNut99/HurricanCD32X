/*
 * WANDKANNONE - the wall cannon (enemies/Gegner_WandKannone.cpp): hangs on a wall and fires a
 * laser every Value1 (tenths of a second); Value2 == 0 looks left, else right.
 * Own members: none (AnimCount is the shot timer, as in Hurrican).
 */
static void wandkannone_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(30);
	e->AnimStart = 0;
	e->AnimEnde = 0;
	e->AnimSpeed = 0;
	e->AnimCount = FX(e->Value1);
	e->Destroyable = 1;
}

static void wandkannone_ki(Ent *e)
{
	e->blick = e->Value2 == 0 ? -1 : 1;

	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde)
			{
				e->AnimPhase = e->AnimStart;
				e->AnimEnde = 0;
				e->AnimCount = FX(e->Value1);
				e->Handlung = GEGNER_LAUFEN;
			}
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->AnimCount = 0;
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimEnde = 3;
			e->AnimSpeed = FX(0.5);
			e->AnimStart = 0;
			e->AnimPhase = 0;
			if (PlayerAbstand(e) < 600)
			{
				snd(SOUND_LASERSHOT);
				if (e->blick == -1)
				{
					proj_push(e->xPos - FX(18), e->yPos + FX(15), WALKER_LASER);
					part_push(e->xPos - FX(24), e->yPos - FX(10), LASERFLAME);
				}
				else
				{
					proj_push(e->xPos + FX(30), e->yPos + FX(15), WALKER_LASER2);
					part_push(e->xPos + FX(21), e->yPos - FX(10), LASERFLAME);
				}
			}
		}
		break;
	default:
		break;
	}
}

static void wandkannone_explode(Ent *e)
{
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(42)), e->yPos - FX(30) + FX(GetRandom(32)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 150;
	gegner_default_explode(e);
}

TYPEDEF(WANDKANNONE) = { wandkannone_init, wandkannone_ki, wandkannone_explode, 0, 0 };
