/*
 * SCHLEIMALIEN - the slime alien ball (enemies/Gegner_SchleimAlien.cpp): hangs around as cannon
 * fodder; spawned by the SchleimMaul with Value1 = its start size, it grows to 60 first. When
 * grown the player can stand on it (it bobs) and is pushed away from its sides.
 * Own members: f[0] = Size.
 * Drawn: Hurrican stretches it to Size x Size; here the plain frame, centred where Hurrican
 * centres it. (Its picture schleimrot.png is in no stage pack.)
 */
static void schleimalien_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(30);
	e->HitSound = 1;
	e->Destroyable = 1;
	e->AnimSpeed = FX(GetRandom(4) + 2) / 5;
	e->AnimEnde = 15;
	e->OwnDraw = 1;
	e->f[0] = e->Value1 == 0 ? FX(60) : FX(e->Value1);
}

static void schleimalien_draw(Ent *e)
{
	const Pic *p = &GegnerPic[e->GegnerArt];
	draw_sprite(p, e->AnimPhase, e->xPos + FX(30 - p->fw), e->yPos + FX(30 - p->fh), 0);
}

static void schleimalien_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];
	SimpleAnimation(e, 0);

	if (e->f[0] < FX(60))      /* growing */
	{
		e->DamageTaken = 0;
		e->Energy = FX(2);
		e->Destroyable = 0;
		e->f[0] += SYNC(FX(2.5));
	}
	else
	{
		e->Destroyable = 1;
		e->f[0] = FX(60);
		PlattformTest(e, rect);
		if (pl.AufPlattform == e)
		{
			const int off = e->AnimPhase < 8 ? -e->AnimPhase : e->AnimPhase - 15;
			pl.ypos = e->yPos - FX(pl.CollideRect.b) + FX(rect->t) - FX(off);
		}
		else
			Wegschieben(e, rect, 0);
	}

	TestDamagePlayers(e, SYNC(FX(4)), 0);
}

static void schleimalien_explode(Ent *e)
{
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	part_push(e->xPos, e->yPos, EXPLOSION_ALIEN);
	for (int i = 0; i < 16; i++)
		part_push(e->xPos + FX(15) + FX(GetRandom(20)), e->yPos + FX(15) + FX(GetRandom(20)), SCHLEIM2);
	snd(SOUND_SCHLEIM);
	pl.Score += 120;
}

TYPEDEF(SCHLEIMALIEN) = { schleimalien_init, schleimalien_ki, schleimalien_explode, schleimalien_draw, 0 };
