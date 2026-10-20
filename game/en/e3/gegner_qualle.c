/*
 * QUALLE - the jellyfish (enemies/Gegner_Qualle.cpp): pushes itself up in the water, sinks, and
 * swims towards the player when he is in the water above it; shoots homing shots at him there.
 * Own members: f[0] = ShotDelay.
 * (Its picture qualle.png is in no stage pack: the type is in no map either.)
 */
static void qualle_init(Ent *e)
{
	e->Handlung = GEGNER_SPRINGEN;
	e->HitSound = 1;
	e->Energy = FX(35);
	e->AnimSpeed = FX(8 + GetRandom(3)) / 10;
	e->AnimPhase = 1;
	e->AnimStart = 0;
	e->AnimEnde = 16;
	e->Destroyable = 1;
	e->ySpeed = FX(-7);
	e->yAcc = FX(0.6);
	e->f[0] = FX(1);
}

static void qualle_push_off(Ent *e)
{
	e->AnimPhase = 0;
	e->AnimEnde = 16;
	e->AnimSpeed = FX(8 + GetRandom(3)) / 10;
	e->AnimCount = 0;
	e->ySpeed = FX(-7);
	e->yAcc = FX(0.6);
	e->Handlung = GEGNER_SPRINGEN;
}

static void qualle_ki(Ent *e)
{
	if (e->AnimPhase != e->AnimStart) SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_SPRINGEN:        /* pushed up */
		if (e->ySpeed > 0) e->yAcc = FX(0.25);
		if (e->ySpeed > FX(7)) e->yAcc = 0;
		if (e->AnimPhase == e->AnimStart)
		{
			e->AnimPhase = 16;
			e->AnimStart = 16;
			e->AnimEnde = 17;
			e->AnimSpeed = FX(4);
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	case GEGNER_LAUFEN:          /* sinks */
		if (pl.InLiquid && e->yPos - FX(20) > pl.ypos)
		{
			part_push(e->xPos + FX(20), e->yPos + FX(30), BUBBLE);
			e->xSpeed = e->xPos < pl.xpos ? FX(1.2) : FX(-1.2);
			qualle_push_off(e);
		}
		else if ((!pl.InLiquid && GetRandom(100) == 0) || (e->blocku & B_WAND) || (e->blocku & B_GEGNERWAND))
		{
			e->xSpeed = FX(1.2);
			if (GetRandom(2) == 0) e->xSpeed = -e->xSpeed;
			qualle_push_off(e);
		}
		break;
	default:
		break;
	}

	if (!(e->blocko & B_LIQUID) && e->ySpeed < 0) e->ySpeed = 0;

	if (pl.InLiquid)
	{
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		else
		{
			e->f[0] = FX(20 + GetRandom(10));
			proj_push(e->xPos + FX(10), e->yPos + FX(10), SUCHSCHUSS);
		}
	}

	TestDamagePlayers(e, SYNC(FX(2)), 0);
}

static void qualle_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(45)), e->yPos + FX(10) + FX(GetRandom(30)), BUBBLE);
	part_push(e->xPos + FX(2), e->yPos - FX(5), PIRANHABLUT);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
	gegner_default_explode(e);
}

TYPEDEF(QUALLE) = { qualle_init, qualle_ki, qualle_explode, 0, 0 };
