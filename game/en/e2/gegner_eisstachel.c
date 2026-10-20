/*
 * EISSTACHEL - the turning ice spike (enemies/Gegner_EisStachel.cpp): swings left and right
 * (accelerating up to Value1), pushes the player away. Indestructible.
 * Own members: none.
 */
static void eisstachel_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->xAcc = FX(1.7);
	e->AnimSpeed = FX(1);
	e->AnimEnde = 19;
	e->Active = 1;
}

static void eisstachel_ki(Ent *e)
{
	E2DBG(e);
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:                /* turn at the speed limit */
		if (e->xSpeed > FX(e->Value1) && e->xAcc > 0) e->xAcc = -e->xAcc;
		if (e->xSpeed < -FX(e->Value1) && e->xAcc < 0) e->xAcc = -e->xAcc;
		break;
	default:
		break;
	}
	Wegschieben(e, &GegnerRect[e->GegnerArt], FX(10));
}

static void eisstachel_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(EISSTACHEL) = { eisstachel_init, eisstachel_ki, eisstachel_explode, 0, 0 };
