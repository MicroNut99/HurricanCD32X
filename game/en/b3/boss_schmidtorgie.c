/*
 * GEGNERSTURM - T.R. Schmidt's enemy orgy in the cave stage (bosses/Boss_SchmidtOrgie.cpp):
 * an invisible boss; when the player comes near, the screen locks and enemies hop in from both
 * sides (bats, slime balls, now and then eyes, a spiked ball, a fire walker, a big slime ball)
 * until its "energy" (one spawn = 10) has run down; then a one-up and the screen goes back to
 * the player.
 * Own members: f[0] = ActionCounter.
 * Left out: the lens effect's alpha (ShadowAlpha: drawing only, nothing drawn here), the music
 * fades (stage music stops on the lock, the boss music starts once locked, the stage music
 * comes back at the end).
 *
 * PC check (not on the 32X): HB_B3_SPAWN=art[,dx,dy] - when this object first runs, it pushes
 * an object of type art at the player + (dx, dy), prints its state every second and does
 * nothing itself (to test types no map has: FLUGBOSS 126, RIESENWASP 103).
 * HB_B3_TRACE=1 prints its state every second.
 */
static void schmidtorgie_init(Ent *e)
{
	e->Handlung = GEGNER_VERFOLGEN;
	e->blick = -1;
	e->Energy = FX(4500);
	e->Destroyable = 0;
	e->f[0] = FX(1);
	e->DontMove = 1;
}

#ifndef OJ32X_MARS
static int schmidtorgie_test_hook(Ent *e)
{
	const char *s = getenv("HB_B3_SPAWN");
	if (!s) return 0;
	int art = 0, dx = 200, dy = -100;
	sscanf(s, "%d,%d,%d", &art, &dx, &dy);
	if (!e->i[0])
	{
		e->i[0] = 1;
		e->i[1] = (int32_t)(gegner_push(pl.xpos + FX(dx), pl.ypos + FX(dy), art, 0, 0) - ents);
	}
	static int n;
	const Ent *o = e->i[1] >= 0 && e->i[1] < MAX_LIVE ? &ents[e->i[1]] : 0;
	if (n++ % 60 == 0 && o)
		fprintf(stderr, "B3 SPAWN: art %d Handlung %d Energy %d pos %d,%d\n", o->GegnerArt, o->Handlung, FINT(o->Energy), FINT(o->xPos), FINT(o->yPos));
	return 1;
}
#endif

static void schmidtorgie_ki(Ent *e)
{
#ifndef OJ32X_MARS
	if (schmidtorgie_test_hook(e)) return;
	if (getenv("HB_B3_TRACE"))
	{
		static int n;
		if (n++ % 60 == 0) fprintf(stderr, "GEGNERSTURM: Handlung %d Energy %d counter %d\n", e->Handlung, FINT(e->Energy), FINT(e->f[0] * 10));
	}
#endif
	/* the energy bar */
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_VERFOLGEN && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(4500));

	/* lock the screen, music */
	if (e->Active && e->Handlung != GEGNER_VERFOLGEN && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();                       /* FadeSong(STAGEMUSIC, -2, 0, pause) */
		gegner_push(e->xPos, e->yPos - FX(40), EXTRAS, 10, 0);
	}

	/* done? */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		music_stop();                       /* FadeSong(BOSS, -2, 0) */
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:                       /* wait until the screen is locked */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_STEHEN:
	{
		e->f[0] -= SYNC(FX(1.5));
		const int x = GetRandom(2) == 0 ? -40 : 620;
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(1.2) + e->Energy / 600;
			/* an enemy chosen at random */
			const int spawn = GetRandom(2);
			if (spawn == 0)
				gegner_push(FX(e->Value1 + x), FX(e->Value2 + 50 + GetRandom(200)), FLEDERMAUS, 0, 1);
			if (spawn == 1)
				gegner_push(FX(e->Value1 + x), FX(e->Value2 + 200 + GetRandom(50)), SCHLEIMBOLLER, 24, 0);
			e->Energy -= FX(10);
			/* now and then a pair of eyes */
			if (FINT(e->Energy) % 1000 == 0)
			{
				gegner_push(FX(e->Value1 - 20), FX(e->Value2 + 130), AUGE, GetRandom(10), 0);
				gegner_push(FX(e->Value1 + 620), FX(e->Value2 + 200), AUGE, GetRandom(10), 0);
			}
			/* or a spiked ball, a fire walker or a big slime ball */
			if (FINT(e->Energy) % 500 == 0)
			{
				switch (GetRandom(3))
				{
				case 0: gegner_push(FX(e->Value1 + x), FX(e->Value2 + 100), STACHELBEERE, 0, 0); break;
				case 1: gegner_push(FX(e->Value1 + x), FX(e->Value2 + 100), SCHLEIMBOLLER, 46, 0); break;
				case 2: gegner_push(FX(e->Value1 + x), FX(e->Value2 + 250), FIESERWALKER2, 0, 0); break;
				}
			}
		}
		break;
	}

	case GEGNER_VERFOLGEN:                  /* wait for the player */
		if (PlayerAbstand(e) < 600 && pl.xpos + FX(80) > e->xPos) e->Handlung = GEGNER_INIT;
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = 0;
		break;

	default:
		break;
	}
}

static void schmidtorgie_explode(Ent *e)
{
#ifndef OJ32X_MARS
	if (getenv("HB_B3_SPAWN")) return;
#endif
	gegner_push(e->xPos, e->yPos - FX(250), ONEUP, 10, 0);   /* an extra life */
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(GEGNERSTURM) = { schmidtorgie_init, schmidtorgie_ki, schmidtorgie_explode, 0, 0 };
