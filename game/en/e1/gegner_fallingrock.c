/*
 * FALLINGROCK - the falling rock (enemies/Gegner_FallingRock.cpp): rests in the ceiling until
 * the player walks below, then falls and shatters on the floor.
 * Own members: none.
 */
static void fallingrock_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(10);
	e->Destroyable = 0;
}

static void fallingrock_ki(Ent *e)
{
	switch (e->Handlung)
	{
	case GEGNER_FALLEN:   /* falling */
		if (e->ySpeed > FX(50)) e->yAcc = 0;
		TestDamagePlayers(e, FX(40), 1);
		if (e->ySpeed > FX(10) && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))) e->Energy = 0;
		break;
	case GEGNER_LAUFEN:   /* waits for the player */
		if (pl.xpos + FX(pl.CollideRect.r) > e->xPos && pl.xpos < e->xPos + FX(80) &&
		    pl.ypos > e->yPos && PlayerAbstand(e) < 480)
		{
			e->Active = 1;
			for (int i = 0; i < 3; i++)
			{
				part_push(e->xPos - FX(10), e->yPos + FX(i * 10), SMOKE);
				part_push(e->xPos + FX(63), e->yPos + FX(i * 10), SMOKE);
			}
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

static void fallingrock_explode(Ent *e)
{
	for (int i = 0; i < 10; i++)
	{
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(20) + FX(GetRandom(20)), SMOKE);
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(GetRandom(40)), ROCKSPLITTER);
		part_push(e->xPos - FX(12) + FX(GetRandom(80)), e->yPos + FX(GetRandom(40)), ROCKSPLITTERSMALL);
	}
	snd(SOUND_STONEEXPLODE);
	pl.Score += 100;
}

TYPEDEF(FALLINGROCK) = { fallingrock_init, fallingrock_ki, fallingrock_explode, 0, 0 };
