/*
 * KRABBLEROBEN - the ceiling crawler (enemies/Gegner_KrabblerOben.cpp): crawls along the ceiling
 * at speed Value2, turns at (enemy) walls and shoots down every Value1.
 * Own members: f[0] = ShotDelay.
 * (Its picture krabbleroben.png is in no stage pack: the type is in no map either.)
 */
static void krabbleroben_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(75);
	e->xSpeed = FX(e->Value2);
	e->blick = -1;
	e->f[0] = FX(e->Value1);
	e->AnimSpeed = FX(0.5);
	e->AnimStart = 0;
	e->AnimEnde = 9;
	e->Destroyable = 1;
}

static void krabbleroben_ki(Ent *e)
{
	e->blick = -1;
	SimpleAnimation(e, 0);

	if ((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) { e->xSpeed = FX(e->Value2); e->blick = 1; }
	if ((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) { e->xSpeed = FX(-e->Value2); e->blick = -1; }

	e->f[0] -= SYNC(FX(1));

	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(e->Value1);
			e->Handlung = GEGNER_SCHIESSEN;
			proj_push(e->xPos + FX(24), e->yPos + FX(48), KRABBLERLASER1);
			part_push(e->xPos - FX(1), e->yPos + FX(44), LASERFLAME);
			snd(SOUND_LASERSHOT);
			e->AnimPhase = 10;
			e->AnimEnde = 14;
			e->AnimStart = 0;
			e->AnimSpeed = FX(0.75);
		}
		break;
	case GEGNER_SCHIESSEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->AnimEnde = 9;
			e->AnimPhase = 0;
			e->AnimSpeed = FX(0.5);
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	default:
		break;
	}
}

static void krabbleroben_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(15) + FX(GetRandom(20)), e->yPos - FX(15) + FX(GetRandom(40)), EXPLOSION_MEDIUM3);
	snd(SOUND_EXPLOSION1);
	pl.Score += 300;
	gegner_default_explode(e);
}

TYPEDEF(KRABBLEROBEN) = { krabbleroben_init, krabbleroben_ki, krabbleroben_explode, 0, 0 };
