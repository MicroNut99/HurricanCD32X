/*
 * EIERMANN - the spider egg man (enemies/Gegner_EierMann.cpp): lays eggs (EIERBOMBE) that roll
 * towards the player and explode.
 * Own members: none.  Skill medium: AnimSpeed 1.6.
 * Left out: the SPLITTER of the explosion (a dropped particle type).
 */
static void eiermann_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(50);
	e->AnimStart = 0;
	e->AnimEnde = 10;
	e->AnimSpeed = FX(1.6);              /* SKILL_MEDIUM */
	e->Destroyable = 1;
}

static void eiermann_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	if (e->AnimEnde > 0)
	{
		e->AnimCount += SPEEDFACTOR;
		if (e->AnimCount > e->AnimSpeed)
		{
			e->AnimCount = 0;
			e->AnimPhase++;
			if (e->AnimPhase >= e->AnimEnde)
			{
				e->AnimPhase = e->AnimStart;
				if (PlayerAbstand(e) <= 700) proj_push(e->xPos + FX(16), e->yPos + FX(40), EIERBOMBE);
			}
		}
	}
}

static void eiermann_explode(Ent *e)
{
	part_push(e->xPos, e->yPos, EXPLOSION_GIANT);
	for (int i = 0; i < 10; i++)
	{
		part_push(e->xPos - FX(30) + FX(GetRandom(100)), e->yPos - FX(30) + FX(GetRandom(80)), EXPLOSION_MEDIUM2);
		part_push(e->xPos + FX(10) + FX(GetRandom(40)), e->yPos + FX(10) + FX(GetRandom(40)), SPIDERSPLITTER);
	}
	snd(SOUND_EXPLOSION4);
	pl.Score += 1500;
}

TYPEDEF(EIERMANN) = { eiermann_init, eiermann_ki, eiermann_explode, 0, 0 };
