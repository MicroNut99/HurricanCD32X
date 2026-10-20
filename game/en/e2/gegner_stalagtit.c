/*
 * STALAGTIT - the stalactite (enemies/Gegner_Stalagtit.cpp): waits like the falling rock until
 * the player passes below, then falls; breaks on the floor.
 * Own members: none.
 */
static void stalagtit_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(10);
	e->Destroyable = 0;
}

static void stalagtit_ki(Ent *e)
{
	E2DBG(e);
	switch (e->Handlung)
	{
	case GEGNER_FALLEN:
		if (e->ySpeed > FX(50)) e->yAcc = 0;
		TestDamagePlayers(e, FX(50), 1);
		if (e->blocku & B_WAND) e->Energy = 0;   /* landed */
		break;

	case GEGNER_LAUFEN:                /* waits for the player */
		if (pl.xpos + FX(pl.CollideRect.r) > e->xPos && pl.xpos < e->xPos + FX(80) &&
		    pl.ypos > e->yPos && PlayerAbstand(e) < RENDERWIDTH)
		{
			for (int i = 0; i < 3; i++)
				part_push(e->xPos + FX(i * 10), e->yPos + FX(30), ROCKSPLITTERSMALLBLUE);
			snd(SOUND_STONEFALL);
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = 0;
			e->yAcc = FX(12);
		}
		break;

	default:
		break;
	}
}

static void stalagtit_explode(Ent *e)
{
	for (int i = 0; i < 20; i++)
	{
		part_push(e->xPos - FX(8) + FX(GetRandom(40)), e->yPos - FX(8) + FX(GetRandom(80)), SMOKE);
		part_push(e->xPos - FX(8) + FX(GetRandom(40)), e->yPos - FX(8) + FX(GetRandom(80)), ROCKSPLITTERSMALLBLUE);
	}
	snd(SOUND_STONEEXPLODE);
	pl.Score += 100;
	gegner_default_explode(e);
}

TYPEDEF(STALAGTIT) = { stalagtit_init, stalagtit_ki, stalagtit_explode, 0, 0 };
