/*
 * PUNISHER (enemies/Gegner_Punisher.cpp): in Hurrican it appears when the time limit runs
 * out, follows the player slowly; touching it costs a life and a level of each weapon. There
 * is no time limit here, so nothing pushes it (ported anyway, for a later use).
 * Own members: f[0] = alpha (the fade in / out, 0..255), f[1] = ShotDelay (unused, as Hurrican).
 * Simplified / left out: the fade in / out in wobbling stripes (drawn here as the plain frame
 * 15 while alpha >= 128), the Punisher music (the stage music stops while it is there and
 * comes back after), the time-limit refill (TileEngine.resetTimelimit / Timelimit += 100),
 * PunisherActive, vanishing when the boss HUD or the summary screen shows (no engine
 * function tells this).
 */
static void punisher_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(20000);
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->TestBlock = 0;
	e->Active = 1;
	e->f[0] = 0;
	e->f[1] = 0;
	e->AnimPhase = 15;
	e->AnimCount = FX(15);
}

static void punisher_draw(Ent *e)
{
	switch (e->Handlung)
	{
	case GEGNER_INIT2:
	case GEGNER_SPECIAL:
		if (e->f[0] >= FX(128)) draw_sprite(&GegnerPic[e->GegnerArt], 15, e->xPos, e->yPos, 0);
		break;
	case GEGNER_LAUFEN:
		draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
		break;
	}
}

static void punisher_follow(Ent *e)
{
	if (e->xPos + FX(50) < pl.xpos + FX(35)) e->xAcc = FX(2);
	if (e->xPos + FX(50) > pl.xpos + FX(35)) e->xAcc = FX(-2);
	if (e->yPos + FX(100) < pl.ypos + FX(40)) e->yAcc = FX(2);
	if (e->yPos + FX(100) > pl.ypos + FX(40)) e->yAcc = FX(-2);
	if (e->xSpeed < FX(-15)) e->xSpeed = FX(-15);
	if (e->xSpeed > FX(15)) e->xSpeed = FX(15);
	if (e->ySpeed < FX(-15)) e->ySpeed = FX(-15);
	if (e->ySpeed > FX(15)) e->ySpeed = FX(15);
}

static void punisher_vanish(Ent *e)
{
	e->Handlung = GEGNER_SPECIAL;
	e->Energy = FX(1);
	e->Destroyable = 0;
}

static void punisher_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	switch (e->Handlung)
	{
	case GEGNER_INIT:                  /* centred on the screen */
		e->xPos = cam_xoff() + FX(320) - FX(50);
		e->yPos = cam_yoff() + FX(240) - FX(47.5);
		e->f[0] = 0;
		e->Handlung = GEGNER_INIT2;
		music_stop();                    /* (Hurrican: the Punisher song) */
		break;

	case GEGNER_INIT2:                 /* fade in */
		e->f[0] += SYNC(FX(5));
		if (e->f[0] > FX(255))
		{
			e->f[0] = FX(255);
			e->Handlung = GEGNER_LAUFEN;
			e->Destroyable = 1;
		}
		break;

	case GEGNER_LAUFEN:
		punisher_follow(e);
		if (pl.Handlung != PA_TOT && pl.Handlung != PA_EXPLODIEREN &&
		    SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
		{
			pl.Energy = 0;                   /* the player dies */
			for (int i = 0; i < 4; i++)       /* and loses weapons */
				if (pl.CurrentWeaponLevel[i] > 1) pl.CurrentWeaponLevel[i]--;
			e->Energy = 0;                   /* fade out */
		}
		/* the frame */
		if (e->xAcc < 0)
		{
			if (e->AnimCount > FX(20)) e->AnimCount = FX(20);
			e->AnimCount -= SYNC(FX(1));
			if (e->AnimCount < 0) e->AnimCount = FX(10);
		}
		else
		{
			if (e->AnimCount < FX(10)) e->AnimCount = FX(10);
			e->AnimCount += SYNC(FX(1));
			if (e->AnimCount > FX(30)) e->AnimCount = FX(20);
		}
		e->AnimPhase = (int16_t)FINT(e->AnimCount);
		break;

	case GEGNER_SPECIAL:               /* fade out */
		e->xAcc = e->yAcc = e->xSpeed = e->ySpeed = 0;
		e->f[0] -= SYNC(FX(5));
		if (e->f[0] < 0) { e->f[0] = 0; e->Energy = 0; }
		break;
	}
	if (e->Energy <= 0 && e->f[0] > 0 && e->Handlung != GEGNER_SPECIAL) punisher_vanish(e);
}

static void punisher_explode(Ent *e)
{
	(void)e;
	music_stage();                     /* the stage music fades back in */
}

TYPEDEF(PUNISHER) = { punisher_init, punisher_ki, punisher_explode, punisher_draw, 0 };
