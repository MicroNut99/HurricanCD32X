/*
 * FADEMUSIC - the music fade trigger (triggers/Trigger_FadeMusic.cpp): within 150 pixels of
 * the player Value1 0 fades the stage music out, 1 fades it in, 2 / 3 fade the sound trigger
 * Value2's looping sound out / in.
 * Own members: i[0] = the player was in range last step.
 * Simplified: no fading - the music stops (music_stop) or starts (music_stage) once when the
 * player comes into range (Hurrican calls FadeSong every step, which does nothing new while
 * the fade runs). Value1 2 / 3 left out: there are no looping trigger sounds here.
 */
static void fademusic_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Value2 = (int16_t)(SOUND_TRIGGER_START + e->Value2);
	e->Destroyable = 0;
	e->Energy = FX(100);
	e->DontMove = 1;
}

static void fademusic_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const int in = PlayerAbstand(e) <= 150;
	if (in && !e->i[0])
	{
		if (e->Value1 == 0) music_stop();
		if (e->Value1 == 1) music_stage();
	}
	e->i[0] = in;
}

static void fademusic_explode(Ent *e) { (void)e; }

TYPEDEF(FADEMUSIC) = { fademusic_init, fademusic_ki, fademusic_explode, 0, 0 };
