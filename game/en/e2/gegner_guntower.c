/*
 * GUNTOWER - the gun tower (enemies/Gegner_GunTower.cpp): always turns towards the player and
 * shoots straight ahead when the player is within Value2 vertically; Value1 = reload time.
 * Own members: none (AnimCount is the reload counter, as Hurrican).
 */
static void guntower_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->AnimStart = 0;
	e->AnimEnde = 0;
	e->AnimSpeed = FX(1);
	e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
	e->Energy = FX(50);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void guntower_ki(Ent *e)
{
	E2DBG(e);
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:                /* look at the player */
		if (e->AnimCount > 0) e->AnimCount -= SPEEDFACTOR;
		else if (PlayerAbstandVert(e) < e->Value2 && PlayerAbstandHoriz(e) < 480)
		{
			if (e->blick == -1)
			{
				part_push(e->xPos - FX(10), e->yPos + FX(6), SMOKE);
				proj_push(e->xPos, e->yPos + FX(15), CANONBALL);
			}
			else
			{
				part_push(e->xPos + FX(35), e->yPos + FX(6), SMOKE);
				proj_push(e->xPos + FX(35), e->yPos + FX(15), CANONBALL2);
			}
			snd(SOUND_CANON);
			e->Handlung = GEGNER_SCHIESSEN;
			e->AnimCount = FX(1);
			e->AnimPhase = 1;
			e->AnimEnde = 5;
			e->AnimStart = 0;
		}

		if ((e->blick == -1 && e->xPos + FX(30) < pl.xpos) || (e->blick == 1 && e->xPos > pl.xpos + FX(30)))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimStart = 12;
			e->AnimPhase = 5;
			e->AnimEnde = 13;
		}
		break;

	case GEGNER_DREHEN:                /* turning to the player */
		if (e->AnimPhase == e->AnimStart)
		{
			e->blick = (int8_t)-e->blick;
			e->AnimEnde = 0;
			e->AnimPhase = 0;
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_SCHIESSEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->AnimPhase = 0;
			e->AnimEnde = 0;
			e->AnimStart = 0;
			e->AnimCount = FX(e->Value1);   /* the reload time from the editor */
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	default:
		break;
	}
}

static void guntower_explode(Ent *e)
{
	part_push(e->xPos - FX(20), e->yPos - FX(40), EXPLOSION_BIG);
	for (int i = 0; i < 2; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(60)), e->yPos - FX(40) + FX(GetRandom(20)), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
	gegner_default_explode(e);
}

TYPEDEF(GUNTOWER) = { guntower_init, guntower_ki, guntower_explode, 0, 0 };
