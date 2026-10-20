/*
 * STAHLFAUST - the giant steel fist, mid boss of the jungle (bosses/Boss_Stahlfaust.cpp): hovers
 * above the player and tries to crush him; now and then flies out at the top and comes down on
 * him.
 * Own members: f[0] = SmokeDelay. (AnimCount is its special-action counter, as in Hurrican.)
 * TileEngine.ScrolltoX / ScrolltoY are Value1 / Value2 (the screen it locks to).
 * Left out: screen shaking, rumble. Hurrican fades the music: here it stops / starts.
 */
static void stahlfaust_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(4000);
	e->AnimPhase = 0;
	e->AnimCount = FX(70);
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->f[0] = 0;
}

/* TileEngine.BlockUnten(xPos, yPos, xPos, yPos, rect): without moving it */
static int stahlfaust_unten(Ent *e, const Box *rect)
{
	fx y = e->yPos, yo = e->yPos;
	return BlockUnten(e->xPos, &y, e->xPos, &yo, rect, 0);
}

static void stahlfaust_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	const fx sx = FX(e->Value1), sy = FX(e->Value2);   /* ScrolltoX / ScrolltoY */

	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN)
	{
		if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EINFLIEGEN2) b2test_hurt(e);
		boss_bar(e->Energy, FX(4000));
	}

	/* lock the screen on the fist as soon as it is seen */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(sx, sy, TS_SCROLLTOLOCK);
		e->yPos -= FX(300);
		e->Handlung = GEGNER_INIT;
		music_stop();
	}

	/* the mid boss does not flash as long */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	/* badly hurt? then it smokes */
	if (e->Energy < FX(2000) && GetRandom(2) == 0)
		part_push(e->xPos + FX(GetRandom(200) + 20), e->yPos + FX(GetRandom(200) + 60), SMOKE);

	/* no energy left? then it explodes */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		e->AnimCount = FX(50);
		e->f[0] = FX(1);
		music_stop();
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:              /* wait until the screen is in place */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN2;
		}
		break;

	case GEGNER_EINFLIEGEN2:       /* comes in the first time */
		e->Energy = FX(4000);
		e->DamageTaken = 0;
		/* fall through */
	case GEGNER_EINFLIEGEN:
		e->yPos += SYNC(FX(8));
		if (e->yPos >= sy)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->xAcc = FX(-8);
		}
		break;

	case GEGNER_LAUFEN:            /* hover above the player and maybe come down */
		e->AnimCount -= SPEEDFACTOR;
		if (pl.xpos + FX(pl.CollideRect.r) < e->xPos || e->xPos > sx + FX(480)) e->xAcc = FX(-8);
		if (pl.xpos > e->xPos + FX(rect->r) || e->xPos < sx - FX(90)) e->xAcc = FX(8);
		if (e->xSpeed < FX(-18)) e->xSpeed = FX(-18);
		if (e->xSpeed > FX(18)) e->xSpeed = FX(18);
		/* the player below? crush him */
		if (pl.xpos < e->xPos + FX(rect->r - 115) && pl.xpos + FX(pl.CollideRect.r) > e->xPos + FX(155) &&
		    GetRandom(2) == 0)
		{
			e->Handlung = GEGNER_CRUSHEN;
			e->ySpeed = 0;
			e->yAcc = FX(10);
			e->xSpeed = 0;
			e->xAcc = 0;
		}
		/* the special: out at the top */
		if (e->AnimCount <= 0)
		{
			e->AnimCount = FX(70);
			e->Handlung = GEGNER_SPRINGEN;
			e->xAcc = 0;
			e->xSpeed = 0;
			e->ySpeed = FX(-5);
			e->yAcc = FX(-5);
		}
		break;

	case GEGNER_CRUSHEN:
		e->blocku = (uint16_t)stahlfaust_unten(e, rect);
		if (e->blocku & B_WAND)
		{
			snd(SOUND_EXPLOSION1);
			snd(SOUND_LANDEN);
			for (int i = 0; i < 25; i++)
				part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(rect->b - 20), SMOKE);
			e->yAcc = FX(-1.5);
			e->ySpeed = 0;
			shake_screen(2);
			e->Handlung = GEGNER_CRUSHENERHOLEN;
		}
		break;

	case GEGNER_CRUSHENERHOLEN:    /* back up after crushing */
		if (e->ySpeed < 0 && e->yPos <= sy)
		{
			e->Handlung = GEGNER_LAUFEN;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->yPos = sy;
			e->xAcc = FX(-8);
		}
		break;

	case GEGNER_SPRINGEN:          /* out at the top */
		if (e->yPos <= sy - FX(280))
		{
			e->Handlung = GEGNER_FALLEN;
			e->AnimPhase = 1;
			e->yAcc = 0;
			e->ySpeed = FX(50);
			e->xPos = pl.xpos - FX(110);
		}
		break;

	case GEGNER_FALLEN:            /* down on the player, then out at the top again */
		e->blocku = (uint16_t)stahlfaust_unten(e, rect);
		if (e->blocku & B_WAND)
		{
			snd(SOUND_EXPLOSION1);
			snd(SOUND_LANDEN);
			for (int i = 0; i < 25; i++)
				part_push(e->xPos + FX(GetRandom(180) + 30), e->yPos + FX(rect->b - 20), SMOKE);
			e->yAcc = FX(-0.5);
			e->ySpeed = 0;
			shake_screen(5);
			e->Handlung = GEGNER_STEHEN;
			/* hurts the player even as a wheel */
			if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
				DamagePlayer(FX(100), 1);
		}
		break;

	case GEGNER_STEHEN:            /* up and out after the crush */
		if (e->yPos <= sy - FX(280))
		{
			e->AnimPhase = 0;
			e->Handlung = GEGNER_EINFLIEGEN;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->yPos = sy - FX(250);
		}
		break;

	case GEGNER_EXPLODIEREN:       /* blows up bit by bit */
		e->AnimCount -= SYNC(FX(1));
		e->f[0] -= SYNC(FX(1));
		e->Energy = FX(100);
		if (e->f[0] < 0)
		{
			e->f[0] = FX(1);
			part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(20) + FX(GetRandom(200)), EXPLOSION_MEDIUM2);
			snd(SOUND_EXPLOSION1);
			if (GetRandom(8) == 0)
				part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(20) + FX(GetRandom(200)), EXPLOSION_BIG);
			if (GetRandom(20) == 0)
				part_push(e->xPos + FX(60) + FX(GetRandom(100)), e->yPos + FX(60) + FX(GetRandom(100)), SPLITTER);
		}
		if (e->AnimCount <= 0) e->Energy = 0;
		break;
	}

	if (e->Handlung != GEGNER_EXPLODIEREN) TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void stahlfaust_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION2);
	for (i = 0; i < 20; i++)
		part_push(e->xPos + FX(60) + FX(GetRandom(60)), e->yPos + FX(80) + FX(GetRandom(40)), SPLITTER);
	for (i = 0; i < 60; i++)
		part_push(e->xPos + FX(20) + FX(GetRandom(100)), e->yPos + FX(40) + FX(GetRandom(100)), SPIDERSPLITTER);
	shake_screen(4);
	pl.Score += 5000;
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

TYPEDEF(STAHLFAUST) = { stahlfaust_init, stahlfaust_ki, stahlfaust_explode, 0, 0 };
