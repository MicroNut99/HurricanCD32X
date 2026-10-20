/*
 * SCHLEUSEH - the horizontal lock gate (triggers/Trigger_SchleuseH.cpp): blocks the way (the
 * player can stand on it) until a switch sets it to GEGNER_OEFFNEN; then it slides 201 pixels
 * to the left.
 * Own members: i[1] = the PC check HB_E3_OPEN's step count (Value1 keeps its start x while it
 * opens, as in Hurrican). Hurrican stops
 * the door sound when it is open (StopWave): sounds cannot be stopped here.
 * (Its picture schleuse.png is in no stage pack: the type is in no map either.)
 */
static void schleuseh_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(50);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void schleuseh_ki(Ent *e)
{
#ifndef OJ32X_MARS
	{   /* PC check (HB_E3_OPEN=n): opens after n steps, as if a switch was pressed */
		const int n = e3test_env("HB_E3_OPEN");
		if (n >= 0 && e->Handlung == GEGNER_STEHEN && e->i[1] >= 0 && ++e->i[1] > n) { e->Handlung = GEGNER_OEFFNEN; e->i[1] = -1; }
	}
#endif
	e->Energy = FX(100);
	e->DamageTaken = 0;
	PlattformTest(e, &GegnerRect[e->GegnerArt]);

	switch (e->Handlung)
	{
	case GEGNER_OEFFNEN:         /* starts opening */
		e->Handlung = GEGNER_LAUFEN;
		e->Value1 = (int16_t)FINT(e->xPos);
		e->xAcc = FX(-0.5);
		snd(SOUND_DOOR);
		break;
	case GEGNER_LAUFEN:          /* opening */
		if (e->xSpeed < FX(-5))
		{
			e->xSpeed = FX(-5);
			e->xAcc = 0;
		}
		if (e->xPos < FX(e->Value1 - 201))
		{
			e->xSpeed = 0;
			e->xAcc = 0;
			e->Handlung = GEGNER_STEHEN;
			snd(SOUND_DOORSTOP);
		}
		break;
	}
}

static void schleuseh_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(SCHLEUSEH) = { schleuseh_init, schleuseh_ki, schleuseh_explode, 0, 0 };
