/*
 * EISZAPFEN - the icicle (enemies/Gegner_Eiszapfen.cpp): hangs from the ceiling and falls when
 * the player passes under it (Value1 == 1: falls at once - the EisFaust boss).
 * Own members: none.
 */
static void eiszapfen_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(100);
	e->Destroyable = 0;
	if (e->Value1 == 1)
	{
		e->Handlung = GEGNER_FALLEN;
		e->ySpeed = FX(30);
		e->yAcc = FX(5);
		snd(SOUND_STONEFALL);
	}
}

static void eiszapfen_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
		if (pl.ypos > e->yPos &&
		    pl.xpos + FX(35) > e->xPos + FX(10 - 60) &&
		    pl.xpos + FX(35) < e->xPos + FX(10 + 60))
		{
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = FX(30);
			e->yAcc = FX(5);
			snd(SOUND_STONEFALL);
			for (int i = 0; i < 15; i++)
			{
				part_push(e->xPos - FX(20) + FX(GetRandom(35)), e->yPos - FX(10) + FX(GetRandom(20)), WATERFLUSH2);
				part_push(e->xPos - FX(20) + FX(GetRandom(35)), e->yPos - FX(10) + FX(GetRandom(20)), SMOKE);
			}
		}
		break;
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(40))
		{
			e->ySpeed = FX(40);
			e->yAcc = 0;
		}
		if (e->blocku & B_WAND) e->Energy = 0;
		break;
	default:
		break;
	}

	TestDamagePlayers(e, FX(30), 1);
}

static void eiszapfen_explode(Ent *e)
{
	snd(SOUND_EXPLOSION1);
	for (int i = 0; i < 30; i++)
	{
		part_push(e->xPos - FX(20) + FX(GetRandom(35)), e->yPos - FX(10) + FX(GetRandom(60)), WATERFLUSH2);
		part_push(e->xPos - FX(20) + FX(GetRandom(35)), e->yPos - FX(10) + FX(GetRandom(60)), SMOKE);
	}
	pl.Score += 100;
}

TYPEDEF(EISZAPFEN) = { eiszapfen_init, eiszapfen_ki, eiszapfen_explode, 0, 0 };
