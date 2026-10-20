/*
 * ROBORAUPE - the robot caterpillar (enemies/Gegner_RoboRaupe.cpp): creeps along and, near the
 * player, fires three arcing shots now and then.
 * Own members: none.
 * Left out: the 300 ROCKETSMOKEBLUE of the explosion (a dropped particle type).
 */
static void roboraupe_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(60);
	e->Destroyable = 1;
	e->AnimEnde = 9;
	e->AnimSpeed = FX(1);
	e->blick = -1;
}

static void roboraupe_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		e->AnimPhase++;
		if (e->AnimPhase > e->AnimEnde) e->AnimPhase = e->AnimStart;
		if (e->AnimPhase == 5)               /* a step */
		{
			if (e->blick == -1) e->xPos -= FX(18);
			else e->xPos += FX(18);
			if (PlayerAbstand(e) < 200 && GetRandom(2) == 0)
			{
				if (e->blick == 1)
				{
					proj_push(e->xPos + FX(31), e->yPos + FX(10), ARCSHOT);
					proj_winkel = FX(1.5);
					proj_push(e->xPos + FX(15), e->yPos + FX(10), ARCSHOTLEFT);
					proj_push(e->xPos + FX(51), e->yPos + FX(10), ARCSHOTRIGHT);
				}
				else
				{
					proj_push(e->xPos + FX(50), e->yPos + FX(10), ARCSHOT);
					proj_winkel = FX(1.5);
					proj_push(e->xPos + FX(32), e->yPos + FX(10), ARCSHOTLEFT);
					proj_push(e->xPos + FX(70), e->yPos + FX(10), ARCSHOTRIGHT);
				}
				snd(SOUND_GRANATE);
			}
		}
	}
	/* falling? */
	if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
	{
		e->yAcc = FX(3);
		if (e->ySpeed > FX(20)) e->ySpeed = FX(20);
	}
	else { e->ySpeed = 0; e->yAcc = 0; }
	/* turn */
	if ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) e->blick = 1;
	if ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) e->blick = -1;
	TestDamagePlayers(e, SYNC(FX(5)), 0);
}

static void roboraupe_explode(Ent *e)
{
	for (int i = 0; i < 50; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(90)), e->yPos - FX(10) + FX(GetRandom(40)), BLUE_EXPLOSION);
	snd(SOUND_EXPLOSION3);
	pl.Score += 250;
}

TYPEDEF(ROBORAUPE) = { roboraupe_init, roboraupe_ki, roboraupe_explode, 0, 0 };
