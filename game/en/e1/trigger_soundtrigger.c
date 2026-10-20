/*
 * SOUNDTRIGGER - a sound source (triggers/Trigger_SoundTrigger.cpp): plays SOUND_TRIGGER_START +
 * Value1. Value2: 0 = loops, louder the nearer the player; 1 / 2 = plays when the player
 * comes within 100 pixels (then the trigger goes).
 * Own members: i[0] = steps until the loop is asked for again.
 * Here: no volume / panning per sound and no looping channels - the loop (Value2 0) asks for
 * its sound every second while it would be audible. (The console's sound set has none of the
 * trigger sounds, SOUND_WASSERFALL ..: snd() plays nothing for them.)
 */
static void soundtrigger_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Destroyable = 0;
	e->Energy = FX(100);
	e->DontMove = 1;
	e->Active = 1;
	e->i[0] = 0;
}

static void soundtrigger_ki(Ent *e)
{
	secret_test_hook();
	switch (e->Value2)
	{
	case 0:   /* loop, its volume from the distance */
		if (PlayerAbstand(e) < 700)
		{
			const int xd = FINT(pl.xpos) + 45 - FINT(e->xPos), yd = FINT(pl.ypos) + 45 - FINT(e->yPos);
			const int abstand = isqrt(xd * xd + yd * yd);
			const int vol = 100 - abstand / 6;
			if (e->i[0] > 0) e->i[0]--;
			if (vol > 0 && e->i[0] <= 0)
			{
				snd(SOUND_TRIGGER_START + e->Value1);
				e->i[0] = 60;
			}
		}
		break;
	case 1:   /* once, when near */
	case 2:
		if (PlayerAbstand(e) < 100)
		{
			snd(SOUND_TRIGGER_START + e->Value1);
			e->Energy = 0;
		}
		break;
	}
}

static void soundtrigger_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(SOUNDTRIGGER) = { soundtrigger_init, soundtrigger_ki, soundtrigger_explode, 0, 0 };
