/*
 * LAFASS - the barrel on the chain (enemies/Gegner_LaFass.cpp): swings along (speed Value1,
 * made by the LaFassSpawner); shot, it drops the barrel (the LAFASSSHOT projectile) and its
 * chain; at a wall it just goes (Value2 = 99).
 * Own members: none.
 */
static void lafass_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->Energy = FX(1);
	e->Destroyable = 1;
	e->AnimPhase = (int16_t)GetRandom(15);
	e->AnimSpeed = FX(1.2);
	e->AnimEnde = 16;
	e->Active = 1;
	e->TestBlock = 1;
	e->xSpeed = FX(e->Value1);
}

static void lafass_ki(Ent *e)
{
	SimpleAnimation(e, 0);
	e->blick = -1;

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND))
		{
			e->Energy = 0;
			e->Value2 = 99;
		}
		break;
	case GEGNER_INIT:
		e->Handlung = GEGNER_LAUFEN;
		break;
	}
}

static void lafass_explode(Ent *e)
{
	/* shot: the barrel falls */
	if (e->Value2 != 99)
	{
		int i;
		proj_winkel = e->xSpeed;
		/* the chain's pieces */
		part_push(e->xPos, e->yPos + FX(78), KETTENTEILE);
		part_push(e->xPos + FX(39), e->yPos + FX(78), KETTENTEILE2);
		part_push(e->xPos + FX(16), e->yPos + FX(43), KETTENTEILE3);
		for (i = 0; i < 3; i++) part_push(e->xPos + FX(33), e->yPos + FX(i * 18), KETTENTEILE4);
		/* the barrel */
		proj_push(e->xPos + FX(7), e->yPos + FX(90), LAFASSSHOT);
	}
	gegner_default_explode(e);
}

TYPEDEF(LAFASS) = { lafass_init, lafass_ki, lafass_explode, 0, 0 };
