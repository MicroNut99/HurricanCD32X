/*
 * FEUERSPUCKER - the flame thrower in the wall (triggers/Trigger_Feuerspucker.cpp): spits
 * flames (FEUERFALLE .. FEUERFALLE4) in direction Value1 for Value2 time units, pauses 20.
 * Own members: f[0] = ShotDelay (AnimCount = the timer, as Hurrican).
 */
static void feuerspucker_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(10000);
	e->AnimPhase = e->Value1;
	e->Destroyable = 0;
	e->f[0] = FX(1);
	e->TestBlock = 0;
	e->Active = 1;
}

static void feuerspucker_ki(Ent *e)
{
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:     /* pause */
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount >= FX(20))
		{
			e->AnimCount = 0;
			e->Handlung = GEGNER_SCHIESSEN;
			snd(SOUND_FEUERFALLE);
		}
		break;
	case GEGNER_SCHIESSEN:  /* spit fire */
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount >= FX(e->Value2))
		{
			e->AnimCount = 0;
			e->Handlung = GEGNER_STEHEN;
		}
		if (PlayerAbstand(e) < 800)
		{
			e->f[0] -= SYNC(FX(1));
			if (e->f[0] <= 0)
			{
				e->f[0] = FX(0.3);
				switch (e->Value1)
				{
				case 0: proj_push(e->xPos, e->yPos, FEUERFALLE); break;
				case 1: proj_push(e->xPos, e->yPos, FEUERFALLE2); break;
				case 2: proj_push(e->xPos, e->yPos, FEUERFALLE3); break;
				case 3: proj_push(e->xPos, e->yPos, FEUERFALLE4); break;
				default: break;
				}
			}
		}
		break;
	default:
		break;
	}
}

static void feuerspucker_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(50) + FX(GetRandom(48)), e->yPos - FX(50) + FX(GetRandom(56)), EXPLOSION_BIG);
	for (int i = 0; i < 8; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(48)), e->yPos - FX(30) + FX(GetRandom(56)), EXPLOSION_MEDIUM);
}

TYPEDEF(FEUERSPUCKER) = { feuerspucker_init, feuerspucker_ki, feuerspucker_explode, 0, 0 };
