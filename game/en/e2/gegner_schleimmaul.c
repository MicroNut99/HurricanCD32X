/*
 * SCHLEIMMAUL - the slime ball with a mouth (enemies/Gegner_SchleimMaul.cpp): hangs around,
 * keeps 8 baby slime aliens (SCHLEIMALIEN) around itself (a lost one comes again), spits
 * SCHLEIMSHOT.
 * Own members: f[0] = ShotDelay, f[1] = SpawnCount (unused, as in Hurrican), i[0] / i[1] = the
 * 8 children's slots (a byte each, 255 = none), i[2] = the first 8 spawned.
 * Hurrican's constructor spawns the children at (x, y); here the constructor does not know its
 * position yet, so they are spawned in the first DoKI. Left out: the transparency (drawn plain).
 */
#define SCHLEIMMAUL_NONE 255

static int schleimmaul_child(const Ent *e, int i)
{
	return (int)((uint32_t)e->i[i >> 2] >> (8 * (i & 3))) & 255;
}

static void schleimmaul_set_child(Ent *e, int i, int slot)
{
	uint32_t v = (uint32_t)e->i[i >> 2];
	v &= ~(255u << (8 * (i & 3)));
	v |= (uint32_t)(slot & 255) << (8 * (i & 3));
	e->i[i >> 2] = (int32_t)v;
}

/* a child at a random point around the mother */
static void schleimmaul_spawn(Ent *e, int i, int value1)
{
	const int r = GetRandom(360);
	const int r1 = GetRandom(40) + 30, r2 = GetRandom(40) + 30;
	Ent *c = gegner_push(e->xPos + sin_d(r) * r1, e->yPos + cos_d(r) * r2, SCHLEIMALIEN, value1, 0);
	schleimmaul_set_child(e, i, c->GegnerArt < 0 ? SCHLEIMMAUL_NONE : (int)(c - ents));
}

static void schleimmaul_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->HitSound = 1;
	e->Energy = FX(200);
	e->Destroyable = 1;
	e->AnimSpeed = FX(GetRandom(4) + 2) / 10;
	e->AnimEnde = 15;
	e->f[0] = 0;
	e->f[1] = 0;
	e->OwnDraw = 1;
	e->i[0] = e->i[1] = -1;            /* all 255 */
	e->i[2] = 0;
}

static void schleimmaul_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void schleimmaul_ki(Ent *e)
{
	E2DBG(e);
	/* the constructor's 8 baby slime balls */
	if (!e->i[2])
	{
		e->i[2] = 1;
		for (int i = 0; i < 8; i++) schleimmaul_spawn(e, i, 0);
	}

	SimpleAnimation(e, 0);

	/* a child gone? a new one */
	for (int i = 0; i < 8; i++)
	{
		const int s = schleimmaul_child(e, i);
		if (s == SCHLEIMMAUL_NONE || ents[s].GegnerArt != SCHLEIMALIEN || ents[s].Energy <= 0)
			schleimmaul_spawn(e, i, 20);
	}

	e->f[0] -= SYNC(FX(1));
	if (e->f[0] <= 0)
	{
		e->f[0] = FX(5) + FX(3) * (4 - SKILL);
		proj_push(e->xPos + FX(30), e->yPos + FX(18), SCHLEIMSHOT);
	}

	Wegschieben(e, &GegnerRect[e->GegnerArt], FX(4));
}

static void schleimmaul_explode(Ent *e)
{
	part_push(e->xPos, e->yPos, EXPLOSION_ALIEN);
	for (int i = 0; i < 16; i++)
		part_push(e->xPos + FX(15 + GetRandom(20)), e->yPos + FX(15 + GetRandom(20)), SCHLEIM2);
	snd(SOUND_SCHLEIM);
	pl.Score += 120;
	gegner_default_explode(e);
}

TYPEDEF(SCHLEIMMAUL) = { schleimmaul_init, schleimmaul_ki, schleimmaul_explode, schleimmaul_draw, 0 };
