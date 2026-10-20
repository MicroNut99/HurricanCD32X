/*
 * PRESSE - the steel press (triggers/Trigger_Presse.cpp): goes down at speed Value2, stamps on
 * the ground, goes back up at half the speed and waits Value2 (tenths of a second) at the top.
 * It pushes the player aside and hurts him.
 * Own members: none (AnimCount is the wait, as in Hurrican).
 */
static void presse_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->ySpeed = FX(e->Value2);
	e->AnimPhase = 0;
	e->Destroyable = 0;
}

static void presse_ki(Ent *e)
{
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:          /* pressing */
		/* at the bottom? stamp and go back up */
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND)))
		{
			e->ySpeed = -FX(e->Value2) / 2;
			for (int i = 0; i < 8; i++) part_push(e->xPos + FX(i * 10), e->yPos + FX(244), SMOKE);
			if (PlayerAbstand(e) < 600) snd(SOUND_PRESSE);
		}
		/* at the top? wait */
		if (e->blocko & B_WAND)
		{
			e->Handlung = GEGNER_STEHEN;
			e->AnimCount = FX(e->Value2);
			e->ySpeed = 0;
		}
		break;
	case GEGNER_STEHEN:          /* waiting */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			e->AnimCount = 0;
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = FX(e->Value2);
			if (PlayerAbstand(e) < 600) snd(SOUND_PRESSE);
			part_push(e->xPos + FX(14), e->yPos + FX(230), SMOKE);
			part_push(e->xPos + FX(54), e->yPos + FX(230), SMOKE);
		}
		break;
	}

	Wegschieben(e, &GegnerRect[e->GegnerArt], FX(10));
}

static void presse_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(PRESSE) = { presse_init, presse_ki, presse_explode, 0, 0 };
