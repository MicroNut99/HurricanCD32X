/*
 * FIESERWALKER2 - the nasty walker with the flame thrower (enemies/Gegner_FieserFireWalker.cpp).
 * Own members: f[0] = ShotDelay, f[1] = ShotDelay2.
 * Left out: the LavaFlare glow while shooting (a glow), the red hit tint (the engine's hit
 * flash is used).
 */
static void fieserfirewalker_init(Ent *e)
{
	e->Handlung = GEGNER_LAUFEN;
	e->Energy = FX(120);
	e->Destroyable = 1;
	e->AnimSpeed = FX(0.5);
	e->AnimEnde = 14;
	e->xSpeed = FX(5);
	e->f[0] = FX(30);
	e->f[1] = 0;
	e->OwnDraw = 1;
}

static void fieserfirewalker_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick != -1);
}

static void fieserfirewalker_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const Box *r = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_LAUFEN:
		if (!(e->blocku & B_WAND) && !(e->blocku & B_PLATTFORM))
		{
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = FX(2);
			e->yAcc = FX(3);
		}
		if (e->f[0] <= 0 && e->AnimPhase == 3 && PlayerAbstand(e) < 400 && PlayerAbstandVert(e) < 150 &&
		    ((e->blick == 1 && FINT(e->xPos) + r->r < FINT(pl.xpos) + pl.CollideRect.l) ||
		     (e->blick == -1 && FINT(e->xPos) + r->l > FINT(pl.xpos) + pl.CollideRect.r)))
		{
			e->f[0] = FX(40);
			e->f[1] = 0;
			e->Handlung = GEGNER_STEHEN;
			e->xSpeed = 0;
			e->AnimEnde = 0;
			snd(SOUND_FEUERFALLE);
		}
		if (e->f[0] > 0) e->f[0] -= SYNC(FX(1));
		if (TurnonShot(e)) e->f[0] = FX(-1);
		break;

	case GEGNER_STEHEN:
		if (e->f[1] > 0) e->f[1] -= SYNC(FX(1));
		if (e->f[1] <= 0)
		{
			e->f[1] = FX(0.4);
			e->f[0] -= FX(1);
			proj_push(e->xPos + FX(5) + FX(e->blick * 38), e->yPos - FX(7), WALKERFIRE);
		}
		if ((e->blick == 1 && e->xPos + FX(60) >= pl.xpos) || (e->blick == -1 && e->xPos <= pl.xpos + FX(80)))
			e->f[0] = FX(-1);
		if (e->f[0] < 0)
		{
			e->f[0] = FX(20);
			e->Handlung = GEGNER_LAUFEN;
			e->AnimEnde = 14;
			e->xSpeed = FX(5) * e->blick;
		}
		break;

	case GEGNER_FALLEN:
		if (e->ySpeed > FX(35)) { e->ySpeed = FX(35); e->yAcc = 0; }
		if ((e->blocku & B_WAND) || (e->blocku & B_PLATTFORM))
		{
			e->ySpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_LAUFEN;
		}
		break;
	}
	TurnonWall(e);
	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void fieserfirewalker_explode(Ent *e)
{
	part_push(e->xPos - FX(30), e->yPos - FX(30), EXPLOSION_BIG);
	for (int i = 0; i < 8; i++)
		part_push(e->xPos - FX(30) + FX(GetRandom(60)), e->yPos - FX(30) + FX(GetRandom(60)), EXPLOSION_MEDIUM2);
	for (int i = 0; i < 12; i++)
		part_push(e->xPos + FX(GetRandom(50)), e->yPos + FX(GetRandom(50)), SPIDERSPLITTER);
	snd(SOUND_EXPLOSION4);
	pl.Score += 80;
}

TYPEDEF(FIESERWALKER2) = { fieserfirewalker_init, fieserfirewalker_ki, fieserfirewalker_explode, fieserfirewalker_draw, 0 };
