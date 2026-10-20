/*
 * FLUGKANONE - the flying cannon (enemies/Gegner_FlugKanone.cpp): hovers above the player and
 * fires its laser down at him.
 * Own members: f[0] = ShotDelay.
 * (Hurrican's "speed *= 1" lines at walls do nothing and are left out.)
 */
static void flugkanone_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(30);
	e->Destroyable = 1;
	e->xSpeed = 0;
	e->ySpeed = 0;
	e->yAcc = 0;
	e->AnimSpeed = FX(1);
	e->AnimEnde = 19;
	e->f[0] = 0;
}

static void flugkanone_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	/* towards the player */
	if (e->xPos + FX(30) < pl.xpos + FX(35)) e->xAcc = FX(2.5);
	if (e->xPos + FX(30) > pl.xpos + FX(35)) e->xAcc = FX(-2.5);
	if (e->yPos < pl.ypos - FX(100)) e->yAcc = FX(2.5);
	if (e->yPos > pl.ypos - FX(100)) e->yAcc = FX(-2.5);
	if (e->xSpeed < FX(-20)) e->xSpeed = FX(-20);
	if (e->xSpeed > FX(20)) e->xSpeed = FX(20);
	if (e->ySpeed < FX(-10)) e->ySpeed = FX(-10);
	if (e->ySpeed > FX(10)) e->ySpeed = FX(10);

	if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
	if (e->f[0] <= 0)
	{
		e->f[0] = SKILL == 0 ? FX(10) : SKILL == 1 ? FX(7.5) : FX(5);
		if (abs(FINT(pl.xpos - e->xPos)) < 100 && PlayerAbstand(e) < 300)
		{
			proj_push(e->xPos + FX(30), e->yPos + FX(40), FLUGLASER);
			part_push(e->xPos + FX(24), e->yPos + FX(30), SMOKE);
			snd(SOUND_LASERSHOT);
		}
	}
	TestDamagePlayers(e, SYNC(FX(1)), 0);
}

static void flugkanone_explode(Ent *e)
{
	for (int i = 0; i < 5; i++)
		part_push(e->xPos - FX(20) + FX(GetRandom(45)), e->yPos - FX(20) + FX(GetRandom(45)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 20; i++)
		part_push(e->xPos + FX(GetRandom(65)), e->yPos + FX(GetRandom(35)), LASERFUNKE2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 150;
}

TYPEDEF(FLUGKANONE) = { flugkanone_init, flugkanone_ki, flugkanone_explode, 0, 0 };
