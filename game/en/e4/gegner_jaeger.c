/*
 * JAEGER - the interceptor (enemies/Gegner_Jaeger.cpp): first flies up past the player in the
 * background (small), then dives down from above.
 * Own members: f[0] = smokeDelay.
 * Drawing: Hurrican draws it scaled to 35 x 26 while flying up (here the plain frame) and turned
 * by 180 degrees while diving (here flipped both ways).
 * (jaeger.png is in none of the stage packs: no stage of this port has the Jaeger.)
 */
static void jaeger_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->ySpeed = FX(-20);
	e->Energy = FX(120);
	e->Destroyable = 0;
	e->OwnDraw = 1;
	e->AnimEnde = 15;
	e->AnimStart = 0;
	e->AnimSpeed = FX(0.5);
	e->f[0] = FX(0.1);
}

static void jaeger_draw(Ent *e)
{
	if (e->Handlung == GEGNER_DREHEN)
		draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
	else if (e->Handlung == GEGNER_DREHEN2)
		draw_sprite_flipv(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 1);
}

static void jaeger_ki(Ent *e)
{
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	/* activated: set below the player so it can fly up from there */
	case GEGNER_INIT:
		e->Handlung = GEGNER_DREHEN;
		e->yPos += FX(512);
		break;

	/* flies up */
	case GEGNER_DREHEN:
		e->Destroyable = 0;
		if (e->yPos - cam_yoff() < FX(-53))
		{
			e->Handlung = GEGNER_STEHEN;
			e->f[0] = FX(50);
		}
		break;

	/* dives down */
	case GEGNER_DREHEN2:
		if (e->yPos - cam_yoff() > FX(RENDERHEIGHT)) e->Energy = 0;
		e->f[0] -= SYNC(FX(1));
		while (e->f[0] <= 0)
		{
			e->f[0] += FX(0.1);
			part_push(e->xPos + FX(18), e->yPos - FX(8), ROCKETSMOKE);
			part_push(e->xPos + FX(40), e->yPos - FX(8), ROCKETSMOKE);
		}
		break;

	/* waits, then dives */
	case GEGNER_STEHEN:
		e->f[0] -= SYNC(FX(10));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.1);
			e->Destroyable = 1;
			e->ySpeed = FX(22);
			e->Handlung = GEGNER_DREHEN2;
		}
		break;
	}

	if (e->Handlung == GEGNER_DREHEN2) TestDamagePlayers(e, FX(50), 1);
}

static void jaeger_explode(Ent *e)
{
	int i;
	snd(SOUND_EXPLOSION1);
	for (i = 0; i < 6; i++)
		part_push(e->xPos - FX(10) + FX(GetRandom(30)), e->yPos - FX(8) + FX(GetRandom(30)), EXPLOSION_MEDIUM2);
	for (i = 0; i < 20; i++)
	{
		part_push(e->xPos + FX(GetRandom(30)), e->yPos + FX(GetRandom(64)), MINIFLARE);
		part_push(e->xPos + FX(GetRandom(25)), e->yPos + FX(GetRandom(50)), SPIDERSPLITTER);
	}
	pl.Score += 100;
	gegner_default_explode(e);
}

TYPEDEF(JAEGER) = { jaeger_init, jaeger_ki, jaeger_explode, jaeger_draw, 0 };
