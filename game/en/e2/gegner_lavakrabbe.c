/*
 * LAVAKRABBE - the lava crab (enemies/Gegner_LavaKrabbe.cpp): flies in from the background onto
 * the elevator (g_Fahrstuhl_yPos), then crawls after the player on it.
 * Own members: f[0] = zPos (how far it has come out of the background).
 * Flying in, Hurrican draws frame 0 scaled up and rotating with an orange tint: here the plain
 * frame 0 once it is half its size (zPos >= 175), centred where the scaled one is.
 */
static void lavakrabbe_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->Energy = FX(90);
	e->Destroyable = 0;
	e->f[0] = 0;
	e->OwnDraw = 1;
}

static void lavakrabbe_draw(Ent *e)
{
	if (e->Handlung == GEGNER_FALLEN)
	{
		fx size = e->f[0] / 5;
		if (size < 0) size = 0;
		if (size > FX(70)) size = FX(70);
		size = FX(70) - size;
		if (size <= FX(35)) draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos - size / 4, 0);
	}
	else draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == 1);
}

static void lavakrabbe_ki(Ent *e)
{
	E2DBG(e);
	if (e->Handlung == GEGNER_STEHEN || e->Handlung == GEGNER_DREHEN || e->Handlung == GEGNER_DREHEN2)
		e->yPos = g_Fahrstuhl_yPos - FX(34);

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:
		e->Handlung = GEGNER_FALLEN;
		e->yPos = cam_yoff() + FX(200);
		e->yAcc = FX(1);
		e->ySpeed = g_Fahrstuhl_Speed - FX(15.05);
		break;

	case GEGNER_DREHEN:                /* landed, turns to the player */
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (--e->AnimPhase <= 0)
			{
				e->AnimPhase = 0;
				e->AnimCount = 0;
				e->Handlung = GEGNER_DREHEN2;
				e->blick = (int8_t)-e->blick;
			}
		}
		break;

	case GEGNER_DREHEN2:
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= 4)
			{
				e->AnimPhase = 5;
				e->AnimCount = 0;
				e->Handlung = GEGNER_STEHEN;
			}
		}
		break;

	case GEGNER_STEHEN:                /* crawls towards the player */
		e->xPos += SYNC(FX(5) * -e->blick);
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= 14) e->AnimPhase = 5;
		}
		if ((e->blick == -1 && e->xPos + FX(35) > pl.xpos + FX(35)) ||
		    (e->blick == 1 && e->xPos + FX(35) < pl.xpos + FX(35)))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimCount = 0;
			e->AnimPhase = 4;
		}
		break;

	case GEGNER_FALLEN:                /* flies in from the background */
		if (e->ySpeed > g_Fahrstuhl_Speed + FX(60))
		{
			e->ySpeed = g_Fahrstuhl_Speed + FX(60);
			e->yAcc = 0;
		}
		e->f[0] += SYNC(FX(9.2));
		if (e->f[0] > FX(360)) e->f[0] = FX(360);
		if (e->f[0] > FX(200)) e->Destroyable = 1;
		/* landed on the elevator? */
		if (e->yPos >= g_Fahrstuhl_yPos - FX(34) && e->ySpeed > 0)
		{
			e->Handlung = GEGNER_DREHEN2;
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimPhase = 1;
			e->AnimEnde = 4;
			e->AnimSpeed = FX(1);
			e->Destroyable = 1;
			if (e->xPos + FX(35) < pl.xpos + FX(35)) e->blick = -1;
			if (e->xPos + FX(35) > pl.xpos + FX(35)) e->blick = 1;
		}
		break;

	default:
		break;
	}

	if (e->Handlung == GEGNER_STEHEN || e->Handlung == GEGNER_DREHEN || e->Handlung == GEGNER_DREHEN2)
		TestDamagePlayers(e, SYNC(FX(8)), 0);
}

static void lavakrabbe_explode(Ent *e)
{
	snd(SOUND_EXPLOSION3);
	part_push(e->xPos + FX(15), e->yPos + FX(10), LAVAKRABBE_KOPF);
	for (int i = 0; i < 4; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(10) + FX(GetRandom(10)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 4; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(10) + FX(GetRandom(10)), LAVAKRABBE_BEIN);
	gegner_default_explode(e);
}

TYPEDEF(LAVAKRABBE) = { lavakrabbe_init, lavakrabbe_ki, lavakrabbe_explode, lavakrabbe_draw, 0 };
