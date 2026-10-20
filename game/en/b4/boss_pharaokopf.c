/*
 * PHARAOKOPF - the pharaoh's head, the temple's boss (bosses/Boss_PharaoKopf.cpp).
 * Rises from the floor, slides against the walls to crush the player, hops to make rocks
 * (FALLINGROCK) fall from the ceiling, shoots lasers from its eyes; it is also a platform.
 * Own members: none (AnimCount is its action counter, as in Hurrican).
 * Left out: rumble, screen shaking, the music fades (stop / start instead).
 */
#define PK_BORDER  25
#define PK_BORDER2 20

static void pk_after_boss(void)          /* ScrolltoPlayeAfterBoss */
{
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
}

static void pk_init(Ent *e)
{
	e->Handlung = GEGNER_NOTVISIBLE;
	e->blick = -1;
	e->Energy = FX(6000);
	e->AnimPhase = 0;
	e->AnimCount = FX(100);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static int pk_links(Ent *e)
{
	const int limit = e->Value1 + PK_BORDER;
	if (FINT(e->xPos) < limit) { e->xPos = FX(limit); return 1; }
	return 0;
}

static int pk_rechts(Ent *e)
{
	const int limit = e->Value1 + 652 - PK_BORDER;
	const int r = GegnerRect[e->GegnerArt].r;
	if (FINT(e->xPos) + r > limit) { e->xPos = FX(limit - r); return 1; }
	return 0;
}

static int pk_unten(Ent *e)
{
	const int limit = e->Value2 + RENDERHEIGHT - PK_BORDER2;
	const int b = GegnerRect[e->GegnerArt].b;
	if (FINT(e->yPos) + b > limit) { e->yPos = FX(limit - b); return 1; }
	return 0;
}

/* the dust where it hits */
static void pk_rocks(fx x, fx ys, fx yr)
{
	for (int i = 0; i < 20; i++)
	{
		part_push(x - FX(30) + FX(GetRandom(224)), ys, SMOKEBIG);
		part_push(x + FX(GetRandom(224)), yr, ROCKSPLITTER);
		part_push(x + FX(GetRandom(224)), yr, ROCKSPLITTERSMALL);
	}
}

static void pk_ki(Ent *e)
{
	const Box *rect = &GegnerRect[e->GegnerArt];

	if (e->Handlung != GEGNER_NOTVISIBLE && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(6000));

	/* the head is a platform as well */
	PlattformTest(e, rect);

	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
		music_stop();
	}

	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(100));
	else e->DamageTaken = 0;

	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		e->AnimCount = FX(20);
		snd(SOUND_EXPLOSION1);
		snd(SOUND_PHARAODIE);
		music_stop();
	}

	/* touching it costs energy */
	TestDamagePlayers(e, SYNC(FX(2)), 0);

	if (e->Energy >= FX(3000) && e->AnimPhase > 1) e->AnimPhase = 0;

	/* half the energy gone: cracks and a burst of rocks */
	if (e->Energy < FX(3000) && e->AnimPhase < 2)
	{
		e->AnimPhase = 2;
		for (int i = 0; i < 100; i++)
		{
			part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTER);
			part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTERSMALL);
		}
		snd(SOUND_PHARAORAMM);
		snd(SOUND_PHARAODIE);
	}

	switch (e->Handlung)
	{
	case GEGNER_NOTVISIBLE:
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
			e->AnimCount = FX(1);
		}
		break;

	case GEGNER_EINFLIEGEN:              /* rises out of the floor */
	{
		e->Energy = FX(6000);
		e->DamageTaken = 0;
		e->yPos -= SYNC(FX(4.5));
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			part_push(e->xPos + FX(GetRandom(230) - 30), FX(e->Value2 + 400 + GetRandom(12)), SMOKEBIG);
			part_push(e->xPos + FX(GetRandom(230) - 12), FX(e->Value2 + 405 + GetRandom(12)), ROCKSPLITTER);
			part_push(e->xPos + FX(GetRandom(230) - 12), FX(e->Value2 + 410 + GetRandom(12)), ROCKSPLITTERSMALL);
			e->AnimCount += FX(0.2);
		}
		const fx limit = FX(e->Value2 + RENDERHEIGHT - PK_BORDER2 - rect->b);
		if (e->yPos <= limit)
		{
			if (pl.AufPlattform == e) { e->ySpeed = FX(-10); e->yAcc = FX(-3); }
			else if (pl.xpos < e->xPos) { e->xSpeed = FX(-1); e->xAcc = FX(-3); }
			else { e->xSpeed = FX(1); e->xAcc = FX(3); }
			e->yPos = limit;
			snd(SOUND_STONEFALL);
			e->Handlung = GEGNER_LAUFEN;
		}
	} break;

	case GEGNER_LAUFEN:                  /* slide towards the player */
		e->xSpeed = 0;
		e->ySpeed = 0;
		if (pl.AufPlattform == e) { e->ySpeed = FX(-10); e->yAcc = FX(-3); }
		else
		{
			if (pl.xpos < e->xPos) { e->xSpeed = FX(-1); e->xAcc = FX(-3); }
			else { e->xSpeed = FX(1); e->xAcc = FX(3); }
			if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
			{
				e->xSpeed = -e->xSpeed;
				e->xAcc = -e->xAcc;
			}
		}
		e->Handlung = GEGNER_CRUSHEN;
		break;

	case GEGNER_CRUSHEN:                 /* against the wall and stop there */
		if (e->ySpeed == 0) part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(210), ROCKSPLITTERSMALL);

		if (e->xSpeed < 0)
		{
			/* pushes the player along */
			fx px = pl.xpos, pxo = pl.xposold;
			if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect) &&
			    !(BlockLinks(&px, pl.ypos, &pxo, pl.yposold, &pl.CollideRect, 0) & B_WAND))
				pl.xpos += SYNC(e->xSpeed);
			if (pk_links(e))
			{
				e->xSpeed = 0;
				e->xAcc = 0;
				for (int i = 0; i < 20; i++)
				{
					part_push(e->xPos - FX(10), e->yPos - FX(30) + FX(GetRandom(224)), SMOKEBIG);
					part_push(e->xPos - FX(10), e->yPos + FX(GetRandom(224)), ROCKSPLITTER);
					part_push(e->xPos - FX(10), e->yPos + FX(GetRandom(224)), ROCKSPLITTERSMALL);
				}
				snd(SOUND_PHARAORAMM);
				e->Handlung = GEGNER_CRUSHENERHOLEN;
				e->AnimCount = FX(10);
				TestDamagePlayers(e, FX(1000), 0);   /* caught the player? */
				shake_screen(3);
			}
		}

		if (e->xSpeed > 0)
		{
			if (SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect) && !(e->blockr & B_WAND))
				pl.xpos += SYNC(e->xSpeed);
			if (pk_rechts(e))
			{
				e->xSpeed = 0;
				e->xAcc = 0;
				for (int i = 0; i < 20; i++)
				{
					part_push(e->xPos + FX(175), e->yPos - FX(30) + FX(GetRandom(224)), SMOKEBIG);
					part_push(e->xPos + FX(212), e->yPos + FX(GetRandom(224)), ROCKSPLITTER);
					part_push(e->xPos + FX(212), e->yPos + FX(GetRandom(224)), ROCKSPLITTERSMALL);
				}
				snd(SOUND_PHARAORAMM);
				e->Handlung = GEGNER_CRUSHENERHOLEN;
				e->AnimCount = FX(10);
				TestDamagePlayers(e, FX(1000), 0);
				shake_screen(3);
			}
		}

		if (e->ySpeed < 0)
		{
			/* up at the ceiling */
			if (e->yPos < FX(e->Value2))
			{
				for (int i = 0; i < 20; i++)
				{
					part_push(e->xPos - FX(30) + FX(GetRandom(224)), e->yPos, SMOKEBIG);
					part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(10), ROCKSPLITTER);
					part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(10), ROCKSPLITTERSMALL);
				}
				snd(SOUND_PHARAORAMM);
				e->yPos = FX(e->Value2);
				e->ySpeed = FX(5);
				e->yAcc = FX(10);
				if (pl.AufPlattform == e) DamagePlayer(FX(200), 0);
				pl.AufPlattform = 0;
				shake_screen(3);
			}
		}
		else if (e->ySpeed > 0)
		{
			/* down on the floor: the player under it? */
			if (pl.Energy > 0 && pl.AufPlattform == 0 &&
			    SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
			{
				DamagePlayer(FX(25), 0);
				e->ySpeed = FX(-10);      /* up again */
				e->yAcc = FX(-3);
			}
			if (pk_unten(e))
			{
				e->ySpeed = 0;
				e->yAcc = 0;
				pk_rocks(e->xPos, e->yPos + FX(170), e->yPos + FX(220));
				snd(SOUND_PHARAORAMM);
				e->AnimCount = FX(20);
				e->Handlung = GEGNER_CRUSHENERHOLEN;
			}
			shake_screen(3);
		}
		break;

	case GEGNER_CRUSHENERHOLEN:
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			int j = GetRandom(3);
			if (j == 0 && e->AnimPhase > 1)
			{
				e->Handlung = GEGNER_SCHIESSEN;
				e->AnimPhase = 3;
				e->AnimCount = FX(8);
			}
			else j = GetRandom(2) + 1;

			if (j == 1)                    /* hop: rocks fall from the ceiling */
			{
				e->ySpeed = FX(-10);
				e->yAcc = FX(2);
				e->AnimPhase = e->AnimPhase == 0 ? 1 : 3;   /* angry look */
				e->Handlung = GEGNER_SPECIAL;
			}
			if (j == 2 || pl.AufPlattform == e)
			{
				e->Handlung = GEGNER_LAUFEN;
				if (j == 2) snd(SOUND_STONEFALL);
			}
		}
		break;

	case GEGNER_SPECIAL:                 /* hops; a rock falls on the player */
		if (pk_unten(e) && e->ySpeed > 0)
		{
			gegner_push(pl.xpos, FX(e->Value2), FALLINGROCK, 0, 0);
			pk_rocks(e->xPos, e->yPos + FX(170), e->yPos + FX(220));
			snd(SOUND_PHARAORAMM);
			e->ySpeed = 0;
			e->yAcc = 0;
			e->AnimCount = FX(10);
			e->Handlung = GEGNER_CRUSHENERHOLEN;
			e->AnimPhase = e->AnimPhase == 1 ? 0 : 2;
		}
		break;

	case GEGNER_SCHIESSEN:               /* lasers from the eyes */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount <= 0)
		{
			part_push(e->xPos + FX(68), e->yPos + FX(77), LASERFLAMEPHARAO);
			part_push(e->xPos + FX(68), e->yPos + FX(77), LASERFLAMEPHARAO);
			part_push(e->xPos + FX(112), e->yPos + FX(77), LASERFLAMEPHARAO);
			part_push(e->xPos + FX(112), e->yPos + FX(77), LASERFLAMEPHARAO);
			proj_push(e->xPos + FX(68), e->yPos + FX(77), PHARAOLASER);
			proj_push(e->xPos + FX(112), e->yPos + FX(77), PHARAOLASER);
			snd(SOUND_PHARAODIE);
			snd(SOUND_LASERSHOT);
			e->AnimCount = FX(8.5);         /* SKILL_MEDIUM */
			if (GetRandom(2) == 0)
			{
				e->AnimPhase = 2;
				e->Handlung = GEGNER_CRUSHENERHOLEN;
			}
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->AnimCount -= SPEEDFACTOR;
		e->Energy = FX(100);
		part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTER);
		part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTERSMALL);
		if (e->AnimCount <= 0) e->Energy = 0;
		break;

	default:
		break;
	}
}

static void pk_explode(Ent *e)
{
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	for (int i = 0; i < 50; i++)
	{
		part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTER);
		part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), ROCKSPLITTERSMALL);
		part_push(e->xPos + FX(GetRandom(224)), e->yPos + FX(GetRandom(224)), SMOKEBIG);
	}
	pl.Score += 8000;
	snd(SOUND_EXPLOSION2);
	snd(SOUND_PHARAORAMM);
	shake_screen(4);
	pk_after_boss();
}

TYPEDEF(PHARAOKOPF) = { pk_init, pk_ki, pk_explode, 0, 0 };
