/*
 * STELZSACK - the stilt sack (enemies/Gegner_StelzSack.cpp): falls into the elevator, lands on
 * it and shoots its laser, turning to the player.
 * Own members: none (AnimCount is also the shot counter, as Hurrican).
 * Uses g_Fahrstuhl_yPos / g_Fahrstuhl_Speed (the elevator's, hur.h).
 * Left out: the STELZFLARE glow (a dropped particle type).
 */
static void stelzsack_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->Energy = FX(80);
	if (e->Value1 == 0) e->Value1 = (int16_t)FINT(g_Fahrstuhl_Speed + FX(25));
	e->Destroyable = 1;
	e->AnimStart = 0;
	e->AnimSpeed = FX(0.75);
	e->ySpeed = g_Fahrstuhl_Speed;
	e->yAcc = FX(10);
	/* look at the player (the constructor sees xPos 0, as Hurrican's) */
	e->blick = e->xPos + FX(50) > pl.xpos + FX(35) ? -1 : 1;
	e->TestBlock = 0;
}

static void stelzsack_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_FALLEN) e->yPos = g_Fahrstuhl_yPos - FX(147);
	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:            /* put above the player, to fall from there */
		e->Handlung = GEGNER_FALLEN;
		e->yPos -= FX(RENDERHEIGHT + 160);
		if (e->yPos < 0) e->yPos = 0;
		break;

	case GEGNER_FALLEN:
		if (e->ySpeed > FX(80) + g_Fahrstuhl_Speed) { e->ySpeed = FX(80) + g_Fahrstuhl_Speed; e->yAcc = 0; }
		if (e->yPos + FX(147) > g_Fahrstuhl_yPos)
		{
			e->ySpeed = 0;
			e->yPos = g_Fahrstuhl_yPos - FX(147);
			e->Handlung = GEGNER_STEHEN;
			e->AnimEnde = 10;
			e->AnimStart = 9;
			e->AnimPhase = 2;
			e->AnimSpeed = FX(0.75);
		}
		break;

	case GEGNER_STEHEN:                /* landing */
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= 9)
			{
				e->AnimPhase = 9;
				e->AnimEnde = 0;
				e->AnimCount = 0;
				e->Handlung = GEGNER_LAUFEN;
			}
		}
		break;

	case GEGNER_LAUFEN:                /* stand and shoot, or turn */
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount >= FX(12))
		{
			e->AnimCount = 0;
			snd(SOUND_LASERSHOT);
			if (e->blick == -1) proj_push(e->xPos - FX(30), e->yPos + FX(25), STELZLASER);
			else proj_push(e->xPos + FX(50), e->yPos + FX(25), STELZLASER2);
		}
		if ((e->blick == -1 && e->xPos + FX(50) < pl.xpos + FX(35)) ||
		    (e->blick == 1 && e->xPos + FX(50) > pl.xpos + FX(35)))
		{
			e->Handlung = GEGNER_DREHEN;
			e->AnimCount = 0;
		}
		break;

	case GEGNER_DREHEN:
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			if (++e->AnimPhase >= 15)
			{
				e->AnimPhase = 13;
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
			if (--e->AnimPhase <= 9)
			{
				e->AnimPhase = 9;
				e->AnimCount = 0;
				e->Handlung = GEGNER_LAUFEN;
			}
		}
		break;
	}
	TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void stelzsack_explode(Ent *e)
{
	part_push(e->xPos, e->yPos, STELZHEAD);
	part_push(e->xPos - FX(20), e->yPos - FX(10), STELZE);
	part_push(e->xPos + FX(20), e->yPos - FX(20), STELZE);
	snd(SOUND_EXPLOSION4);
	pl.Score += 500;
}

TYPEDEF(STELZSACK) = { stelzsack_init, stelzsack_ki, stelzsack_explode, 0, 0 };
