/*
 * SCHWABBEL - the wobbling sack (enemies/Gegner_Schwabbel.cpp): wobbles (faster the less energy
 * it has) and now and then throws up maggots (MADE); bursts into 30 of them.
 * Own members: f[0] = shotdelay.
 * (schwabbel.png is in none of the stage packs: no stage of this port has the Schwabbel.)
 */
static void schwabbel_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->HitSound = 1;
	e->Energy = FX(300);
	e->Destroyable = 1;
	e->f[0] = FX(5);
	e->AnimSpeed = FX(0.3);
	e->AnimEnde = 20;
}

static void schwabbel_ki(Ent *e)
{
	/* less energy -> faster */
	e->AnimSpeed = e->Energy / 1000 + FX(0.1);

	if (e->Handlung == GEGNER_STEHEN)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			/* maggots */
			if (e->AnimPhase == 14)
			{
				e->f[0] -= FX(1);
				if (e->f[0] <= 0)
				{
					e->Handlung = GEGNER_SCHIESSEN;
					e->f[0] = FX(40);
					e->AnimCount = FX(1);
					snd(SOUND_KOTZEN);
				}
			}
			if (e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_SCHIESSEN:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			e->AnimCount += FX(1);
			gegner_push(e->xPos + FX(16) + FX(GetRandom(6)), e->yPos - FX(10) + FX(GetRandom(4)), MADE, 1, 0);
		}
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] < 0)
		{
			e->f[0] = FX(GetRandom(4) + 6);
			e->Handlung = GEGNER_STEHEN;
		}
		break;
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void schwabbel_explode(Ent *e)
{
	int i;
	snd(SOUND_MADE);
	snd(SOUND_EXPLOSION1);
	for (i = 0; i < 80; i++)
		part_push(e->xPos + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(60)), MADEBLUT);
	for (i = 0; i < 30; i++)
		gegner_push(e->xPos + FX(GetRandom(40)), e->yPos - FX(10) + FX(GetRandom(60)), MADE, 1, 0);
	pl.Score += 400;
	gegner_default_explode(e);
}

TYPEDEF(SCHWABBEL) = { schwabbel_init, schwabbel_ki, schwabbel_explode, 0, 0 };
