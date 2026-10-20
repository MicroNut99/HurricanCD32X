/*
 * MUTANT - the mutant from the tube (enemies/Gegner_Mutant.cpp): crawls after the player and
 * jumps at him. Value1 0: placed in the level; else it drops out of a TUBE first.
 * Own members: f[0] = JumpDelay.
 */
static void mutant_init(Ent *e)
{
	e->HitSound = 1;
	e->Destroyable = 1;
	e->Energy = FX(100);
	e->TestBlock = 1;
	e->f[0] = 0;
	if (e->Value1 == 0)
	{
		e->Handlung = GEGNER_LAUFEN;
		e->AnimSpeed = FX(0.8);
		e->AnimStart = 4;
		e->AnimPhase = 4;
		e->AnimEnde = 17;
	}
	else
	{
		e->Handlung = GEGNER_EINFLIEGEN;
		e->AnimSpeed = FX(3);
		e->AnimStart = 1;
		e->AnimPhase = 1;
		e->AnimEnde = 99;
		e->ySpeed = FX(2);
		e->yAcc = FX(3);
	}
}

static void mutant_walk(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->AnimSpeed = FX(0.8);
	e->AnimStart = 4;
	e->AnimPhase = 4;
	e->AnimEnde = 17;
}

static void mutant_ki(Ent *e)
{
	E2DBG(e);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (PlayerAbstandHoriz(e) > 20)
		{
			SimpleAnimation(e, 0);
			/* crawl towards the player */
			e->blick = (int8_t)(e->xPos + FX(50) < pl.xpos + FX(35) ? 1 : -1);
			e->xSpeed = FX(5) * e->blick;
		}
		else
		{
			e->xSpeed = 0;
			e->AnimPhase = 3;
		}

		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM)) e->yAcc = FX(5);   /* falling */
		else
		{
			e->ySpeed = 0;
			e->yAcc = 0;
			if (PlayerAbstand(e) < 400) e->f[0] -= SYNC(FX(1));
			if (e->f[0] <= 0)
			{
				e->Handlung = GEGNER_SPRINGEN;
				e->xSpeed = FX(15) * e->blick;
				e->AnimStart = 18;
				e->AnimPhase = 18;
				e->AnimEnde = 26;
				e->AnimSpeed = FX(1.5);
				e->f[0] = FX(8 + GetRandom(5));
				e->ySpeed = FX(-30);
				e->yAcc = FX(6);
				snd(SOUND_MUTANT);
			}
		}
		break;

	case GEGNER_SPRINGEN:
		if (e->AnimPhase < 24) SimpleAnimation(e, 0);
		else
		{
			e->AnimPhase = 24;
			e->yAcc = FX(15);
		}
		if (e->ySpeed > 0 && ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))) mutant_walk(e);
		break;

	case GEGNER_EINFLIEGEN:
		SimpleAnimation(e, 0);
		if (e->AnimPhase > 2) e->AnimSpeed = FX(4);     /* waits a moment on the floor */
		if ((e->blocku & B_WAND) && e->ySpeed > 0)
		{
			snd(SOUND_SCHLEIM);
			e->ySpeed = 0;
			e->yAcc = 0;
		}
		if (e->AnimPhase > 3) mutant_walk(e);
		break;

	default:
		break;
	}

	if (e->Handlung != GEGNER_EINFLIEGEN) TestDamagePlayers(e, SYNC(FX(6)), 0);
}

static void mutant_explode(Ent *e)
{
	for (int i = 0; i < 10; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(60)), e->yPos + FX(GetRandom(60)), EXPLOSION_GREEN);
	for (int i = 0; i < 10; i++)
		part_push(e->xPos + FX(15) + FX(GetRandom(60)), e->yPos + FX(10) + FX(GetRandom(60)), SPIDERSPLITTER);
	for (int i = 0; i < 15; i++)
		part_push(e->xPos + FX(15) + FX(GetRandom(60)), e->yPos + FX(10) + FX(GetRandom(60)), SCHLEIM);
	snd(SOUND_MUTANT);
	snd(SOUND_EXPLOSION4);
	snd(SOUND_SCHLEIM);
	pl.Score += 500;
	gegner_default_explode(e);
}

TYPEDEF(MUTANT) = { mutant_init, mutant_ki, mutant_explode, 0, 0 };
