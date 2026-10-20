/*
 * BIGFISH - the giant fish in the temple level (bosses/Boss_BigFish.cpp).
 * Swims about, spits out small piranhas (one at a time or nine in a fan), and has small
 * balls (KUGELKLEIN ..) and swim walkers come in from the sides.
 * Own members: i[0] = ShotCount, i[1] = Moving, f[0] = MaulWinkel, f[1] = SinOff (both fx
 * DEGREES - Hurrican's radians * 180 / pi), f[2] = NewX, f[3] = NewY, f[4] = KugelCount.
 * Drawing: the mouth, the fins and the body as Hurrican places them, unrotated and unscaled
 * (the mouth's angle and the fins' rotation / scaling are left out); the glow flare is left out.
 * Left out: rumble, screen shaking, the music fades (stop / start instead).
 */
static Pic P_bf_maul, P_bf_klein, P_bf_gross, P_bf_oben, P_bf_unten;
static int bf_lastaction = -1;           /* NewAction's static */
static int bf_iskugel;                   /* DoKI's static IsKugel */
static int bf_shot_s, bf_shot_p;         /* SCHIESSEN's and SPECIAL's static shot */
static fx bf_flossensin, bf_flossensin2; /* DoDraw's statics (fx degrees) */

#define BF_RAD(r) FX((r) * 57.29578)     /* radians -> fx degrees */

static void bf_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void bf_init(Ent *e)
{
	e->i[0] = 0;
	e->f[0] = 0;
	e->f[1] = 0;
	e->f[2] = e->f[3] = 0;
	e->f[4] = FX(150);
	e->i[1] = 0;
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(2000);
	e->AnimPhase = 0;
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

static void bf_moveto(Ent *e, fx x, fx y)
{
	e->i[1] = 1;
	e->f[2] = x;
	e->f[3] = y;
	e->xSpeed = e->ySpeed = 0;
	if (e->xPos < x) e->xSpeed = FX(10);
	if (e->xPos > x) e->xSpeed = FX(-10);
	if (e->yPos < y) e->ySpeed = FX(10);
	if (e->yPos > y) e->ySpeed = FX(-10);
}

static void bf_domove(Ent *e)
{
	if (e->xSpeed < 0 && e->f[2] > e->xPos) { e->xSpeed += SYNC(FX(1)); if (e->xSpeed > 0) e->xSpeed = 0; }
	if (e->xSpeed > 0 && e->f[2] < e->xPos) { e->xSpeed -= SYNC(FX(1)); if (e->xSpeed < 0) e->xSpeed = 0; }
	if (e->ySpeed < 0 && e->f[3] > e->yPos) { e->ySpeed += SYNC(FX(1)); if (e->ySpeed > 0) e->ySpeed = 0; }
	if (e->ySpeed > 0 && e->f[3] < e->yPos) { e->ySpeed -= SYNC(FX(1)); if (e->ySpeed < 0) e->ySpeed = 0; }
	if (e->xSpeed == 0 && e->ySpeed == 0) e->i[1] = 0;
}

static void bf_newaction(Ent *e)
{
	/* the player right of the fish: move the fish */
	if (pl.xpos > e->xPos)
	{
		fx newx = pl.xpos;
		if (newx > FX(e->Value1 + 400)) newx = FX(e->Value1) + FX(400);
		bf_moveto(e, newx, FX(e->Value2) + FX(200));
	}
	int j = GetRandom(3);
	while (bf_lastaction == j) j = GetRandom(3);
	bf_lastaction = j;
	switch (j)
	{
	case 0: e->i[0] = 1; e->Handlung = GEGNER_SPECIAL; break;
	case 1: e->i[0] = 5; e->Handlung = GEGNER_SCHIESSEN; break;
	case 2: e->i[0] = 3; e->Handlung = GEGNER_LAUFEN; break;
	}
}

static void bf_draw(Ent *e)
{
	const int fs2 = FINT(sin_d(FINT(bf_flossensin2)) * 10);
	const fx yoff = sin_d(FINT(e->f[1])) * 5;
	const fx x = e->xPos, y = e->yPos + yoff;

	draw_sprite(&P_bf_maul, 0, x + FX(51), y + FX(94), 0);           /* the mouth */
	draw_sprite(&P_bf_gross, 0, x + FX(203), y - FX(9), 0);          /* the tail */
	draw_sprite(&P_bf_oben, 0, x + FX(151), y - FX(20) + FX(fs2), 0); /* the upper fin */
	draw_sprite(&P_bf_unten, 0, x + FX(140), y + FX(120), 0);        /* the lower fin */
	draw_sprite(&GegnerPic[e->GegnerArt], 0, x, y, 0);               /* the fish */
	draw_sprite(&P_bf_klein, 0, x + FX(184), y + FX(80), 0);         /* the small fin */

	/* the fins move */
	const fx xs = e->xSpeed, ys = e->ySpeed;
	const fx fact = (FMUL(xs, xs) + FMUL(ys, ys) + FX(20)) / 200;
	const fx dg = fact * 573 / 10;                                   /* -> degrees */
	bf_flossensin += SYNC(dg);
	if (bf_flossensin > FX(360)) bf_flossensin = 0;
	bf_flossensin2 += SYNC(dg);
	if (bf_flossensin2 > FX(360)) bf_flossensin2 = 0;
}

static void bf_ki(Ent *e)
{
	Box *rect = &GegnerRect[BIGFISH];

	e->f[4] -= SYNC(FX(1));
	if (e->f[4] <= 0)
	{
		int lr = 1, art = 2;
		e->f[4] = FX(80);
		if (e->Energy > FX(500)) art = 1;
		if (e->Energy > FX(1500)) art = 0;
		if (GetRandom(2) == 0) lr = -1;
		if (bf_iskugel) gegner_push(cam_xoff() + FX(300 + 300 * lr), cam_yoff() + FX(200), KUGELKLEIN + art, 5, 0);
		else gegner_push(cam_xoff() + FX(300 + 320 * lr), cam_yoff() + FX(350), SWIMWALKER, 5, 0);
		bf_iskugel = !bf_iskugel;
	}

	/* swimming */
	e->f[1] += SYNC(BF_RAD(0.2));
	if (e->f[1] > FX(360)) e->f[1] = 0;

	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(2000));

	if (e->Active && e->Handlung != GEGNER_EXPLODIEREN && tile_zustand() == TS_SCROLLBAR && e->Energy > 0)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
		e->xPos += FX(400);
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* badly hit: it bleeds */
	if (e->Energy < FX(1000) && GetRandom(20) == 0)
		part_push(e->xPos + FX(40 + GetRandom(200)), e->yPos + FX(50 + GetRandom(100)), PIRANHABLUT);

	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		music_stop();
		bf_after_boss();
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = 0;
		e->ySpeed = FX(2);
		e->xAcc = 0;
		e->yAcc = FX(1);
		e->AnimCount = FX(50);
		e->i[1] = 0;
	}

	if (e->i[1]) bf_domove(e);
	else switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_EINFLIEGEN:
		e->Energy = FX(2000);
		e->DamageTaken = 0;
		bf_newaction(e);
		bf_moveto(e, FX(e->Value1 + 450), e->yPos);
		break;

	case GEGNER_LAUFEN:
		bf_moveto(e, FX(e->Value1 + 50 + GetRandom(300)), FX(e->Value2 + 50 + GetRandom(200)));
		if (--e->i[0] <= 0) bf_newaction(e);
		break;

	case GEGNER_SCHIESSEN:               /* spits single fish */
		e->f[0] += SYNC(BF_RAD(0.6));
		if (e->f[0] > FX(180) && !bf_shot_s)
		{
			bf_shot_s = 1;
			gegner_push(e->xPos + FX(80), e->yPos + FX(90), PIRANHA, 99, 1);
			e->i[0]--;
		}
		if (e->f[0] > FX(360))
		{
			bf_shot_s = 0;
			e->f[0] = 0;
			snd(SOUND_KLONG);
			if (e->i[0] <= 0) bf_newaction(e);
		}
		break;

	case GEGNER_SPECIAL:                 /* spits nine fish */
		e->f[0] += SYNC(BF_RAD(0.1));
		if (e->f[0] > FX(180) && !bf_shot_p)
		{
			bf_shot_p = 1;
			for (int i = 0; i < 9; i++) gegner_push(e->xPos + FX(80), e->yPos + FX(90), PIRANHA, 98, i - 4);
			e->i[0]--;
		}
		if (e->f[0] > FX(360))
		{
			bf_shot_p = 0;
			e->f[0] = 0;
			snd(SOUND_KLONG);
			if (e->i[0] <= 0) bf_newaction(e);
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = FX(100);
		if (e->yPos > FX(e->Value2 + 300)) e->Energy = 0;
		break;

	default:
		break;
	}

	/* touching the fish: the big rectangle */
	const int so = FINT(sin_d(FINT(e->f[1])) * 5);
	rect->l = 60;
	rect->r = 280;
	rect->t = (int16_t)(28 + so);
	rect->b = (int16_t)(156 + so);
	TestDamagePlayers(e, SYNC(FX(8)), 0);
	/* the normal rectangle (where it can be hit) */
	rect->l = 0;
	rect->r = 17;
	rect->t = (int16_t)(28 + so);
	rect->b = (int16_t)(50 + so);
}

static void bf_explode(Ent *e)
{
	snd(SOUND_EXPLOSION2);
	for (int i = 0; i < 80; i++)
		part_push(e->xPos + FX(40 + GetRandom(200)), e->yPos + FX(50 + GetRandom(100)), PIRANHABLUT);
	for (int i = 0; i < 20; i++)
		gegner_push(e->xPos + FX(40 + GetRandom(200)), e->yPos + FX(30 + GetRandom(80)), PIRANHA, 99, 0);
	shake_screen(4);
	pl.Score += 5000;
	gegner_default_explode(e);
}

static void bf_load(void)
{
	P_bf_maul = pic("bigfishmaul.png");
	P_bf_klein = pic("bigfishflosseklein.png");
	P_bf_gross = pic("bigfishflossebig.png");
	P_bf_oben = pic("bigfishflosseoben.png");
	P_bf_unten = pic("bigfishflosseunten.png");
}

TYPEDEF(BIGFISH) = { bf_init, bf_ki, bf_explode, bf_draw, bf_load };
