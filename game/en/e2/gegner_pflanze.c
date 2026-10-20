/*
 * PFLANZE - the Super Mario plant (enemies/Gegner_PFlanze.cpp): sways and spits fire balls
 * (PFLANZESHOT) when the player is near (or at random).
 * Own members: none. (Skill medium: AnimSpeed 2.2 - 0.4.)
 */
static void pflanze_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->HitSound = 1;
	e->Energy = FX(80);
	e->AnimPhase = (int16_t)GetRandom(8);
	e->AnimEnde = 9;
	e->AnimSpeed = FX(2.2) - FX(0.4) * SKILL;
	e->AnimCount = 0;
	e->Destroyable = 1;
}

static void pflanze_ki(Ent *e)
{
	E2DBG(e);
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase == 14)        /* spit */
		{
			if (e->blick == 1) proj_push(e->xPos + FX(84), e->yPos + FX(16), PFLANZESHOT);
			else proj_push(e->xPos + FX(12), e->yPos + FX(16), PFLANZESHOT);
		}
		if (e->AnimPhase >= e->AnimEnde)
		{
			if (PlayerAbstand(e) < 300 || GetRandom(2) == 0)   /* shoot */
			{
				e->Handlung = GEGNER_SCHIESSEN;
				e->AnimEnde = 17;
				e->AnimSpeed = FX(2.2) - FX(0.4) * SKILL;
				e->AnimPhase = 9;
			}
			else                       /* or just sway */
			{
				e->Handlung = GEGNER_LAUFEN;
				e->AnimEnde = 8;
				e->AnimSpeed = FX(1.8);
				e->AnimPhase = 0;
			}
		}
	}

	/* look at the player */
	e->blick = (int8_t)(pl.xpos + FX(35) < e->xPos + FX(55) ? -1 : 1);

	TestDamagePlayers(e, SYNC(FX(1.5)), 0);
}

static void pflanze_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos + FX(GetRandom(50)), e->yPos + FX(GetRandom(40)), EXPLOSION_GREEN);
	for (int i = 0; i < 30; i++)       /* leaves */
		part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(10) + FX(GetRandom(50)), BLATT);
	pl.Score += 300;
	gegner_default_explode(e);
}

TYPEDEF(PFLANZE) = { pflanze_init, pflanze_ki, pflanze_explode, 0, 0 };
