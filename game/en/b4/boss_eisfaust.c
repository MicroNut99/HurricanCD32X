/*
 * EISFAUST - the giant ice fist, the eis level's mid-boss (bosses/Boss_EisFaust.cpp).
 * Floats above the player and tries to crush him, flies out at the top and comes down on him,
 * or sweeps over the screen dropping icicles (EISZAPFEN).
 * Own members: none (AnimCount is the special-action counter as in Hurrican).
 * TileEngine.ScrolltoX / ScrolltoY: the screen is locked at (Value1, Value2) - that is what
 * Hurrican's ScrollLevel sets them to, so Value1 / Value2 are used for them.
 * Left out: the screen shaking, the music fades (stop / start instead).
 * The picture stahlfaust_eis.png is in no stage pack (the boss is in none of the 9 maps):
 * stahlfaust.png is used if a pack has it, so it can be seen in a test.
 */
#define EF_SCROLLTOX(e) FX((e)->Value1)
#define EF_SCROLLTOY(e) FX((e)->Value2)

static void ef_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void ef_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(7000);
	e->AnimPhase = 0;
	e->AnimCount = FX(50);              /* counter for the special action */
	e->Destroyable = 1;
}

static void ef_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(7000));

	/* lock the screen on the fist once it shows */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		e->yPos -= FX(300);                 /* and the fist out of the screen */
		music_stop();
	}

	/* the mid-boss does not flash as long as the others */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* badly hit: it smokes */
	if (e->Energy < FX(2000) && GetRandom(2) == 0)
		part_push(e->xPos + FX(20 + GetRandom(200)), e->yPos + FX(60 + GetRandom(200)), SMOKE);

	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		e->AnimCount = FX(50);
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

	case GEGNER_EINFLIEGEN2:
		e->Energy = FX(7000);
		e->DamageTaken = 0;
		e->yPos += SYNC(FX(8));
		if (e->yPos >= EF_SCROLLTOY(e)) { e->Handlung = GEGNER_LAUFEN; e->xAcc = FX(-8); }
		break;

	case GEGNER_EINFLIEGEN:
		e->yPos += SYNC(FX(8));
		if (e->yPos >= EF_SCROLLTOY(e)) { e->Handlung = GEGNER_LAUFEN; e->xAcc = FX(-8); }
		break;

	case GEGNER_LAUFEN:
		e->AnimCount -= SPEEDFACTOR;
		if (pl.xpos + FX(pl.CollideRect.r) < e->xPos || e->xPos > EF_SCROLLTOX(e) + FX(480)) e->xAcc = FX(-8);
		if (pl.xpos > e->xPos + FX(rect->r) || e->xPos < EF_SCROLLTOX(e) - FX(90)) e->xAcc = FX(8);
		if (e->xSpeed < FX(-18)) e->xSpeed = FX(-18);
		if (e->xSpeed > FX(18)) e->xSpeed = FX(18);
		/* the player below the fist: crush */
		if (pl.xpos < e->xPos + FX(rect->r - 115) && pl.xpos + FX(pl.CollideRect.r) > e->xPos + FX(155) &&
		    GetRandom(2) == 0)
		{
			e->Handlung = GEGNER_CRUSHEN;
			e->ySpeed = 0;
			e->yAcc = FX(10);
			e->xSpeed = 0;
			e->xAcc = 0;
		}
		/* special action: out at the top */
		if (e->AnimCount <= 0)
		{
			e->AnimCount = FX(50);
			e->Handlung = GEGNER_SPRINGEN;
			e->xAcc = 0;
			e->xSpeed = 0;
			e->ySpeed = FX(-5);
			e->yAcc = FX(-5);
		}
		break;

	case GEGNER_CRUSHEN:
		if (e->blocku & B_WAND)
		{
			snd(SOUND_EXPLOSION1);
			snd(SOUND_LANDEN);
			for (int i = 0; i < 80; i++)
				part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(rect->b - 40 + GetRandom(20)), WATERFLUSH2);
			e->yAcc = FX(-1.5);
			e->ySpeed = 0;
			shake_screen(2);
			e->Handlung = GEGNER_CRUSHENERHOLEN;
		}
		break;

	case GEGNER_CRUSHENERHOLEN:
		if (e->ySpeed < 0 && e->yPos <= EF_SCROLLTOY(e))
		{
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->yPos = EF_SCROLLTOY(e);
			e->xAcc = FX(-8);
		}
		break;

	case GEGNER_SPRINGEN:
		if (e->yPos <= EF_SCROLLTOY(e) - FX(280))
		{
			if (GetRandom(2) == 0)
			{
				e->Handlung = GEGNER_FALLEN;
				e->AnimPhase = 1;
				e->yAcc = 0;
				e->ySpeed = FX(50);
				e->xPos = pl.xpos - FX(110);
			}
			else
			{
				e->Handlung = GEGNER_BOMBARDIEREN;
				e->xPos = FX(e->Value1) - FX(250);
				e->yPos = FX(e->Value2) - FX(200);
				e->ySpeed = FX(10);
				e->yAcc = FX(-0.4);
				e->xSpeed = FX(20);
				e->AnimCount = FX(8);
			}
		}
		break;

	case GEGNER_BOMBARDIEREN:
		e->blick = e->xSpeed > 0 ? 1 : -1;
		e->AnimCount -= SYNC(FX(1));
		while (e->AnimCount <= 0)
		{
			e->AnimCount += FX(4);
			/* (Hurrican compares the y with XOffset as well: kept) */
			if (e->xPos + FX(100) > cam_xoff() && e->xPos + FX(100) < cam_xoff() + FX(620) &&
			    e->yPos + FX(220) > cam_xoff() && e->yPos + FX(220) < cam_xoff() + FX(440))
				gegner_push(e->xPos + FX(100), e->yPos + FX(220), EISZAPFEN, 1, 0);
		}
		if (e->xSpeed > 0 && e->xPos > FX(e->Value1 + RENDERWIDTH) + FX(30))
		{
			e->xSpeed = FX(-20);
			e->ySpeed = FX(10);
			e->yPos = FX(e->Value2) - FX(100);
		}
		if (e->xSpeed < 0 && e->xPos < FX(e->Value1) - FX(300))
		{
			e->AnimCount = FX(50);
			e->AnimPhase = 0;
			e->Handlung = GEGNER_EINFLIEGEN;
			e->xSpeed = e->xAcc = e->ySpeed = e->yAcc = 0;
			e->yPos = EF_SCROLLTOY(e) - FX(400);
			e->xPos = FX(e->Value1) + FX(200);
		}
		break;

	case GEGNER_FALLEN:
		if (e->blocku & B_WAND)
		{
			snd(SOUND_EXPLOSION1);
			snd(SOUND_LANDEN);
			for (int i = 0; i < 80; i++)
				part_push(e->xPos + FX(30 + GetRandom(180)), e->yPos + FX(rect->b - 40 + GetRandom(20)), WATERFLUSH2);
			e->yAcc = FX(-0.5);
			e->ySpeed = 0;
			shake_screen(5);
			e->Handlung = GEGNER_STEHEN;
		}
		break;

	case GEGNER_STEHEN:
		if (e->yPos <= EF_SCROLLTOY(e) - FX(280))
		{
			e->AnimPhase = 0;
			e->Handlung = GEGNER_EINFLIEGEN;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->yPos = EF_SCROLLTOY(e) - FX(250);
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->AnimCount -= SPEEDFACTOR;
		e->Energy = FX(100);
		if (GetRandom(5) == 0)
		{
			part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(20 + GetRandom(200)), EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
		}
		if (GetRandom(8) == 0)
			part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(20 + GetRandom(200)), EXPLOSION_BIG);
		if (GetRandom(20) == 0)
			part_push(e->xPos + FX(60 + GetRandom(100)), e->yPos + FX(60 + GetRandom(100)), SPLITTER);
		if (e->AnimCount <= 0) e->Energy = 0;
		break;

	default:
		break;
	}

	if (e->Handlung != GEGNER_EXPLODIEREN) TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void ef_explode(Ent *e)
{
	for (int i = 0; i < 20; i++)
		part_push(e->xPos + FX(60 + GetRandom(60)), e->yPos + FX(80 + GetRandom(40)), SPLITTER);
	for (int i = 0; i < 60; i++)
	{
		part_push(e->xPos + FX(20 + GetRandom(100)), e->yPos + FX(40 + GetRandom(100)), SPIDERSPLITTER);
		part_push(e->xPos + FX(60 + GetRandom(60)), e->yPos + FX(80 + GetRandom(40)), WATERFLUSH2);
	}
	pl.Score += 5000;
	ef_after_boss();
	gegner_default_explode(e);
}

static void ef_load(void)
{
	if (!GegnerPic[EISFAUST].count) GegnerPic[EISFAUST] = pic("stahlfaust.png");
#ifndef OJ32X_MARS
	b4t_inject();                        /* group B4's PC test hook (b4_test.c) */
#endif
}

TYPEDEF(EISFAUST) = { ef_init, ef_ki, ef_explode, 0, ef_load };
