/*
 * WUXESPINNEN - the spider rush of the spider level (bosses/Boss_WuxeSpinnen.cpp): an invisible
 * boss that locks the screen and lets spiders come down from the ceiling; its energy runs down
 * with every spider.
 * Own members: f[0] = ActionCounter.
 * Notes: on none of the 9 maps; tested with HB_B2_SPAWN. The spiders (MITTELSPINNE), eyes
 * (AUGE), extras (EXTRAS) and the extra life (ONEUP) belong to other groups.
 * Hurrican fades the music (FadeSong): here it stops / starts.
 */
static void wuxespinnen_init(Ent *e)
{
	e->Handlung = GEGNER_VERFOLGEN;
	e->blick = -1;
	e->Energy = FX(2000);
	e->Destroyable = 0;
	e->f[0] = FX(1);
	e->DontMove = 1;
}

static void wuxespinnen_ki(Ent *e)
{
	/* the energy bar */
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_VERFOLGEN && e->Handlung != GEGNER_EXPLODIEREN)
	{
		b2test_hurt(e);
		boss_bar(e->Energy, FX(2000));
	}

	/* start the boss and its music */
	if (e->Active && e->Handlung != GEGNER_VERFOLGEN && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
		gegner_push(e->xPos, e->yPos - FX(40), EXTRAS, 10, 0);
	}

	/* no energy left? then it is over */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:              /* wait until the screen is in place */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_STEHEN:
		e->f[0] -= SYNC(FX(0.8));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(1.5) + e->Energy / 500;
			if (e->Energy > FX(500))
				gegner_push(FX(e->Value1 + GetRandom(520)), FX(e->Value2 - 100), MITTELSPINNE, 0, 1);
			e->Energy -= FX(20);
			/* a pair of eyes now and then */
			if (FINT(e->Energy) % 1000 == 0)
				for (int i = 0; i < 2; i++)
				{
					gegner_push(FX(e->Value1 - 20), FX(e->Value2 + i * 60 + 150), AUGE, GetRandom(10), 0);
					gegner_push(FX(e->Value1 + 620), FX(e->Value2 + i * 60 + 180), AUGE, GetRandom(10), 0);
				}
			/* or autofire ;) */
			if (FINT(e->Energy + FX(500)) % 600 == 0)
				gegner_push(e->xPos, e->yPos - FX(40), EXTRAS, 10, 0);
		}
		break;

	case GEGNER_VERFOLGEN:         /* wait for the player */
		if (PlayerAbstand(e) < 400) e->Handlung = GEGNER_INIT;
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = 0;
		break;
	}
}

static void wuxespinnen_explode(Ent *e)
{
	gegner_push(e->xPos, e->yPos - FX(250), ONEUP, 10, 0);
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(WUXESPINNEN) = { wuxespinnen_init, wuxespinnen_ki, wuxespinnen_explode, 0, 0 };
