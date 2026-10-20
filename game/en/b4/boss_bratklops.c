/*
 * BRATKLOPS - the disgusting Bratklops (bosses/Boss_Bratklops.cpp): slides in from the side,
 * spits green slime balls (BRATKLOPSSHOT), vomits maggots (MADE), sweeps a green laser over
 * the floor (both ways), charges a flare and fires a column of nine slime balls.
 * Own members: i[0] = Shots, i[1] = the laser sound is on, f[0] = ShotDelay,
 * f[1] = ActionDelay, f[2] = FlareDelay, f[3] = the time left of the vomit sound (Hurrican
 * waits until SOUND::KOTZEN has ended: kotzen.wav is 5.13 s = 51 time units).
 * Hurrican moves the laser, hurts the player with it and ends the laser attack in DoDraw;
 * those parts are done here at the end of DoKI (bk_laser), DoDraw only draws.
 * Drawing: the body (six single pictures, bratklops0000..0005.png), the flare (plain, not
 * rotated or additive), the laser beam as three lines (Hurrican: a rotated textured quad).
 * The pictures are in no stage pack (the boss is in none of the 9 maps): it is invisible
 * unless a pack has them.
 * Left out: the music fades (stop / start instead).
 */
static Pic P_bk_gfx[6], P_bk_flare;
static int bk_oldaction;                 /* STEHEN's static oldaction */

static void bk_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void bk_init(Ent *e)
{
	e->f[0] = e->f[1] = e->f[2] = e->f[3] = 0;
	e->i[0] = e->i[1] = 0;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(8000);
	e->Destroyable = 1;
	e->AnimPhase = 0;
	e->AnimStart = 0;
	e->AnimEnde = 10;
	e->AnimCount = 0;
	e->AnimSpeed = FX(1);
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

/* the laser's angle (degrees) for FlareDelay */
static int bk_winkel(const Ent *e)
{
	int w = FINT((e->f[2] - FX(128)) * 2 / 9) - 20;   /* (FlareDelay - 128) / 4.5 - 20 */
	while (w < 0) w += 360;
	return w;
}

static int bk_laser_on(const Ent *e)
{
	return e->f[2] > FX(150) &&
	       ((e->Handlung == GEGNER_SPECIAL && FINT(e->f[2]) % 90 < 60) || e->Handlung == GEGNER_SPECIAL3);
}

static void bk_draw(Ent *e)
{
	int a = e->AnimPhase;
	if (a > 5) a = 10 - a;
	if (a < 0) a = 0;
	draw_sprite(&P_bk_gfx[a], 0, e->xPos, e->yPos, 0);
	if (e->f[2] > 0)
	{
		draw_sprite(&P_bk_flare, 0, e->xPos + FX(64), e->yPos + FX(122), 0);
		if (bk_laser_on(e))
		{
			const int t = 360 - bk_winkel(e);
			const fx x0 = e->xPos + FX(155), y0 = e->yPos + FX(215);
			const fx dx = -sin_d(t) * 585, dy = cos_d(t) * 585;
			for (int k = -1; k <= 1; k++)
				draw_line(x0 + FX(2 * k), y0, x0 + FX(2 * k) + dx, y0 + dy, k ? col_green : col_light);
		}
	}
}

/* the laser's work (Hurrican: in DoDraw) */
static void bk_laser(Ent *e)
{
	if (e->f[2] <= 0) return;
	if (bk_laser_on(e))
	{
		if (!e->i[1]) { snd(SOUND_BRATLASER); e->i[1] = 1; }
		const int w = bk_winkel(e);
		static const Box r = { 0, 0, 24, 24 };   /* a piece of the laser */
		fx xs = e->xPos + FX(145), ys = e->yPos + FX(203);
		const fx sx = cos_d(360 - w + 90) * 24, sy = sin_d(360 - w + 90) * 24;
		for (int i = 0; i < 25; i++)
		{
			fx x = xs, y = ys;
			if (pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN &&
			    SpriteCollision(pl.xpos, pl.ypos, &pl.CollideRect, x, y, &r))
				DamagePlayer(SYNC(FX(10)), 0);
			xs += sx;
			ys += sy;
			if (BlockUnten(x, &y, x, &y, &r, 0) & B_WAND)
			{
				/* sparks and smoke on the floor */
				if (GetRandom(2) == 0) part_push(x + FX(GetRandom(24)), y + FX(GetRandom(24)), FUNKE2);
				if (GetRandom(2) == 0) part_push(x - FX(15) + FX(GetRandom(24)), y - FX(40) + FX(GetRandom(8)), SMOKE2);
			}
		}
	}
	else e->i[1] = 0;                    /* StopWave */
	if (e->f[2] > FX(800))
	{
		e->f[2] = 0;
		e->Handlung = GEGNER_STEHEN;
	}
}

static void bk_ki(Ent *e)
{
	/* wobbles faster */
	e->AnimSpeed = (fx)((int64_t)e->Energy * 18 / 80000);
	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
		}
	}

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(8000));

	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		e->xPos -= FX(232);                 /* and the boss out of the screen */
		music_stop();
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(50));
	else e->DamageTaken = 0;

	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->f[2] = 0;
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_EINFLIEGEN:
		e->Energy = FX(8000);
		e->DamageTaken = 0;
		e->xPos += SYNC(FX(4));
		if (e->xPos > FX(e->Value1))
		{
			e->xPos = FX(e->Value1);
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_STEHEN:
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			int j;
			do j = GetRandom(6); while (j == bk_oldaction);
			bk_oldaction = j;
			switch (j)
			{
			case 0:                          /* slime balls */
				e->Handlung = GEGNER_BOMBARDIEREN;
				e->f[1] = FX(8);
				proj_push(e->xPos + FX(146), e->yPos + FX(186), BRATKLOPSSHOT);
				snd(SOUND_GRANATE);
				e->i[0] = GetRandom(3) + 3;
				break;
			case 1:                          /* vomit */
				e->Handlung = GEGNER_SCHIESSEN;
				e->f[0] = FX(1);
				snd(SOUND_KOTZEN);
				e->f[3] = FX(51.3);
				break;
			case 2:                          /* laser */
				e->Handlung = GEGNER_SPECIAL;
				e->f[2] = 0;
				snd(SOUND_SPIDERSCREAM);
				break;
			case 3:                          /* the big ones */
				e->Handlung = GEGNER_SPECIAL2;
				e->f[2] = 0;
				snd(SOUND_SPIDERSCREAM);
				break;
			case 4:                          /* laser right to left */
				e->Handlung = GEGNER_SPECIAL3;
				e->f[2] = FX(800);
				snd(SOUND_SPIDERSCREAM);
				break;
			case 5:                          /* pause */
				e->f[1] = FX(16);
				break;
			}
		}
		break;

	case GEGNER_BOMBARDIEREN:
		e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			e->f[1] = FX(8);
			proj_push(e->xPos + FX(146), e->yPos + FX(186), BRATKLOPSSHOT);
			snd(SOUND_GRANATE);
			if (--e->i[0] == 0) e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_SPECIAL:                 /* the laser walks over the floor (left to right) */
		e->f[2] += SYNC((FX(10000) - e->Energy) / 1000);
		break;

	case GEGNER_SPECIAL3:                /* (right to left) */
		e->f[2] -= SYNC((FX(10000) - e->Energy) / 750);
		if (e->f[2] < FX(210))
		{
			e->f[2] = 0;
			e->Handlung = GEGNER_STEHEN;
			e->f[1] = FX(8);
		}
		break;

	case GEGNER_SPECIAL2:                /* charge, then nine slime balls */
		e->f[2] += SYNC((FX(10000) - e->Energy) / 1000);
		if (e->f[2] > FX(255))
		{
			static const int16_t bk_col[9][2] = { { 60, 50 }, { 84, 80 }, { 108, 115 }, { 130, 150 }, { 145, 185 },
			                                      { 130, 220 }, { 108, 255 }, { 84, 290 }, { 60, 320 } };
			e->f[2] = 0;
			e->Handlung = GEGNER_STEHEN;
			e->f[1] = FX(8);
			for (int i = 0; i < 9; i++) proj_push(e->xPos + FX(bk_col[i][0]), e->yPos + FX(bk_col[i][1]), BRATKLOPSSHOT);
			snd(SOUND_GRANATE);
			snd(SOUND_BRATLASER);
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = FX(100);
		if (GetRandom(5) == 0) part_push(e->xPos + FX(GetRandom(180)), e->yPos + FX(GetRandom(500)), EXPLOSION_GREEN);
		if (GetRandom(3) == 0) part_push(e->xPos + FX(GetRandom(150)), e->yPos + FX(100 + GetRandom(200)), MADEBLUT);
		if (GetRandom(8) == 0) snd(SOUND_EXPLOSION1);
		e->xPos -= SYNC(FX(3));
		if (e->xPos - cam_xoff() <= FX(-300)) e->Energy = 0;
		break;

	case GEGNER_SCHIESSEN:
		/* the vomit sound over? */
		e->f[3] -= SYNC(FX(1));
		if (e->f[3] <= 0)
		{
			e->f[1] = FX(10);
			e->Handlung = GEGNER_STEHEN;
		}
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.30);            /* SKILL_MEDIUM */
			gegner_push(e->xPos + FX(121 + GetRandom(6)), e->yPos + FX(105 + GetRandom(6)), MADE, 0, 0);
		}
		break;

	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(4)), 0);
	TestDamagePlayers(e, SYNC(FX(4)), 0);

	bk_laser(e);
}

static void bk_explode(Ent *e)
{
	snd(SOUND_EXPLOSION2);
	bk_after_boss();
	gegner_default_explode(e);
}

static void bk_load(void)
{
	static const char *const names[6] = { "bratklops0000.png", "bratklops0001.png", "bratklops0002.png",
	                                      "bratklops0003.png", "bratklops0004.png", "bratklops0005.png" };
	for (int i = 0; i < 6; i++) P_bk_gfx[i] = pic(names[i]);
	P_bk_flare = pic("bratklopsshot2.png");
}

TYPEDEF(BRATKLOPS) = { bk_init, bk_ki, bk_explode, bk_draw, bk_load };
