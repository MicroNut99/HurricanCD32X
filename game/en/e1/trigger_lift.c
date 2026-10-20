/*
 * LIFT - the lift (triggers/Trigger_Lift.cpp): rises when the player stands on it and blows
 * up when its countdown (the animation, speed Value1 / 10) runs out, then waits again where it
 * started (Value2 = the start height).
 * Own members: f[0] = SmokeDelay (unused, as Hurrican).
 */
static void lift_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->f[0] = 0;
	e->AnimSpeed = FX(e->Value1) / 10;
}

static void lift_ki(Ent *e)
{
	e->blick = -1;
	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde)   /* the countdown ran out: it blows up */
			{
				for (int i = 0; i < 10; i++)
					part_push(e->xPos - FX(30) + FX(GetRandom(80)), e->yPos - FX(20) + FX(GetRandom(20)), EXPLOSION_MEDIUM2);
				e->Handlung = GEGNER_STEHEN;
				e->AnimPhase = 0;
				e->AnimEnde = 0;
				e->yPos = FX(e->Value2);
				e->ySpeed = 0;
				e->yAcc = 0;
				pl.AufPlattform = 0;
				snd(SOUND_EXPLOSION1);
			}
		}
	}

	PlattformTest(e, &GegnerRect[e->GegnerArt]);

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:   /* waits for the player */
		e->Value2 = (int16_t)FINT(e->yPos);
		if (pl.AufPlattform == e)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = FX(-3);
			e->yAcc = FX(-3);
			e->AnimPhase = 0;
			e->AnimEnde = 10;
			e->AnimStart = 0;
		}
		break;
	case GEGNER_LAUFEN:   /* going up */
		if (e->ySpeed < FX(-30)) { e->yAcc = 0; e->ySpeed = FX(-30); }
		break;
	}
}

static void lift_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(LIFT) = { lift_init, lift_ki, lift_explode, 0, 0 };
