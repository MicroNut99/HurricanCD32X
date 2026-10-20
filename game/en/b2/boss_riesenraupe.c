/*
 * RIESENRAUPE - the giant caterpillar (bosses/Boss_RiesenRaupe.cpp): crawls towards the player
 * and fires two homing shots now and then.
 * Own members: i[0] = ShotCount.
 * Notes: its picture (riesenraupe.png) is in none of the stage packs (it is on no map), so it
 * draws nothing unless a pack gets it. Not on any of the 9 maps; tested with HB_B2_SPAWN.
 */
static void riesenraupe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(800);
	e->Destroyable = 1;
	e->AnimEnde = 10;
	e->AnimSpeed = FX(1.5);
	e->blick = -1;
	e->i[0] = 0;
}

static void riesenraupe_ki(Ent *e)
{
	b2test_hurt(e);
	/* animate */
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase > e->AnimEnde)
		{
			e->AnimPhase = e->AnimStart;
			e->xSpeed = 0;
			if (PlayerAbstand(e) > 400 || e->i[0] == 0)
			{
				e->i[0] = GetRandom(5);
				e->AnimEnde = 10;
				e->AnimSpeed = FX(1.5);
			}
			else
			{
				/* shoot */
				e->i[0]--;
				e->AnimPhase = 10;
				e->AnimEnde = 13;
				e->AnimSpeed = FX(1.5);
				part_push(e->xPos + FX(30), e->yPos - FX(2), SMOKE);
				part_push(e->xPos + FX(120), e->yPos - FX(2), SMOKE);
				proj_push(e->xPos + FX(30), e->yPos - FX(2), PFLANZESHOT);
				proj_push(e->xPos + FX(120), e->yPos - FX(2), PFLANZESHOT);
				snd(SOUND_ROCKET);
			}
		}
	}

	if (e->AnimPhase < 10) e->xSpeed = e->blick == -1 ? FX(-1) : FX(1);

	/* fall? */
	if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
	{
		e->yAcc = FX(3);
		if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
	}
	else
	{
		e->ySpeed = 0;
		e->yAcc = 0;
	}

	/* turn? */
	if (((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) && e->xSpeed < 0)
	{
		e->blick = 1;
		e->xSpeed = -e->xSpeed;
	}
	if (((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) && e->xSpeed > 0)
	{
		e->blick = -1;
		e->xSpeed = -e->xSpeed;
	}

	TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void riesenraupe_explode(Ent *e)
{
	int i;
	for (i = 0; i < 10; i++)
	{
		part_push(e->xPos - FX(30) + FX(GetRandom(180)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_BIG);
		part_push(e->xPos - FX(30) + FX(GetRandom(180)), e->yPos - FX(30) + FX(GetRandom(60)), SPIDERSPLITTER);
	}
	for (i = 0; i < 20; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(180)), e->yPos - FX(10) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (i = 0; i < 300; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(180)), e->yPos + FX(GetRandom(50)), ROCKETSMOKE);
	snd(SOUND_EXPLOSION3);
	pl.Score += 250;
	gegner_default_explode(e);
}

TYPEDEF(RIESENRAUPE) = { riesenraupe_init, riesenraupe_ki, riesenraupe_explode, 0, 0 };
