/*
 * RIESENWASP - the giant robot wasp (bosses/Boss_RiesenWespe.cpp, class GegnerRiesenWasp):
 * flies towards the player, shoots homing shots, crashes down smoking when out of energy and
 * bursts when it hits a wall. (No map of the game has one; its picture wasp.png is not in the
 * packs, so it is invisible here - Hurrican's data does not list it either.)
 * Own members: f[0] = ShotDelay.
 */
static void riesenwespe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(125);
	e->Destroyable = 1;
	e->AnimEnde = 5;
	e->AnimSpeed = FX(1);
	e->blick = -1;
	e->f[0] = FX(10);
}

static void riesenwespe_ki(Ent *e)
{
	/* animate (AnimPhase > AnimEnde: one frame more than SimpleAnimation) */
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase > e->AnimEnde) e->AnimPhase = e->AnimStart;
	}

	e->blick = e->xSpeed > 0 ? 1 : -1;

	/* towards the player */
	if (e->Handlung != GEGNER_FALLEN)
	{
		if (e->xPos + FX(50) < pl.xpos + FX(35)) e->xAcc = FX(2.5);
		if (e->xPos + FX(50) > pl.xpos + FX(35)) e->xAcc = FX(-2.5);
		if (e->yPos < pl.ypos - FX(100)) e->yAcc = FX(1.5);
		if (e->yPos > pl.ypos - FX(100)) e->yAcc = FX(-1.5);
		if (e->xSpeed < FX(-22)) e->xSpeed = FX(-22);
		if (e->xSpeed > FX(22)) e->xSpeed = FX(22);
		if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
		if (e->ySpeed > FX(10)) e->ySpeed = FX(10);
		/* (Hurrican's "turn at walls" multiplies the speeds by 1: no effect) */
	}

	/* shoot? */
	const int d = PlayerAbstand(e);
	if (d < 500 && d > 100 &&
	    ((e->blick == 1 && e->xPos + FX(50) < pl.xpos + FX(35)) ||
	     (e->blick == -1 && e->xPos + FX(50) > pl.xpos + FX(35))))
	{
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(10);
			snd(SOUND_CANON);
			proj_push(e->xPos + FX(25) + FX(e->blick * 56), e->yPos + FX(51), SUCHSCHUSS2);
			for (int i = 0; i < 5; i++)
				part_push(e->xPos + FX(50) + FX(GetRandom(5) + e->blick * 60), e->yPos + FX(75) + FX(GetRandom(5)), SMOKE3);
		}
	}

	switch (e->Handlung)
	{
	case GEGNER_FALLEN:                     /* crashing down */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND) || (e->blocko & B_WAND) ||
		    (e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
			e->Energy = 0;
		if (e->ySpeed > FX(35)) e->yAcc = 0;
		if (e->AnimCount >= FX(0.5))
		{
			e->AnimCount = 0;
			part_push(e->xPos + FX(30) + FX(GetRandom(80)), e->yPos + FX(30) + FX(GetRandom(50)), SMOKE);
			part_push(e->xPos + FX(30) + FX(GetRandom(80)), e->yPos + FX(30) + FX(GetRandom(50)), SMOKE3);
		}
		break;
	default:
		break;
	}

	TestDamagePlayers(e, SYNC(FX(5)), 0);

	/* out of energy: it crashes */
	if (e->Energy <= 0 && e->Handlung != GEGNER_FALLEN)
	{
		e->Handlung = GEGNER_FALLEN;
		e->AnimCount = 0;
		e->Energy = FX(50);
		e->ySpeed = FX(1);
		e->yAcc = FX(2);
		e->xAcc = 0;
	}
}

static void riesenwespe_explode(Ent *e)
{
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(110)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_BIG);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(110)), e->yPos - FX(10) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 200; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(110)), e->yPos + FX(GetRandom(50)), ROCKETSMOKE);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos + FX(GetRandom(100)), e->yPos + FX(30) + FX(GetRandom(50)), SPLITTER);
	snd(SOUND_EXPLOSION1);
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(RIESENWASP) = { riesenwespe_init, riesenwespe_ki, riesenwespe_explode, 0, 0 };
