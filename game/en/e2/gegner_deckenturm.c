/*
 * DECKENTURM - the gun turret on the ceiling (enemies/Gegner_Deckenturm.cpp): hangs closed and
 * invulnerable, opens when the player comes within Value2, then turns to the player and shoots
 * SUCHSCHUSS every Value1 (its AnimSpeed); closes again when the player goes away.
 * Own members: f[0] = Winkel (fx degrees).
 */
/* PC test log for the E2 objects: HB_E2LOG=1 prints each one every 30 of its steps;
 * HB_E2KILL=n sets each one's Energy to 0 after its n-th step (tests GegnerExplode);
 * HB_E2FAHR=1 puts g_Fahrstuhl_yPos under the player (LAVAKRABBE without the elevator) */
#ifndef OJ32X_MARS
static void deckenturm_e2dbg(const Ent *e)
{
	static int on = -1, killat;
	static uint8_t cnt[MAX_LIVE];
	static uint16_t kc[MAX_LIVE];
	if (on < 0) { on = getenv("HB_E2LOG") != 0; killat = getenv("HB_E2KILL") ? atoi(getenv("HB_E2KILL")) : 0; }
	if (e < ents || e >= ents + MAX_LIVE) return;
	if (killat && ++kc[e - ents] == killat) ((Ent *)e)->Energy = 0;   /* HB_E2KILL=n: each one killed after n steps */
	if (getenv("HB_E2FAHR")) g_Fahrstuhl_yPos = pl.ypos + FX(80);   /* a stand-in elevator under the player */
	if (!on) return;
	if (cnt[e - ents]++ % 30) return;
	fprintf(stderr, "e2 %3d art %3d pos %d,%d spd %d,%d hdl %d anim %d ac %d en %d v %d,%d\n", (int)(e - ents), e->GegnerArt,
	        FINT(e->xPos), FINT(e->yPos), FINT(e->xSpeed), FINT(e->ySpeed), e->Handlung, e->AnimPhase, e->AnimCount, FINT(e->Energy), e->Value1, e->Value2);
}
#define E2DBG(e) deckenturm_e2dbg(e)
#else
#define E2DBG(e) ((void)0)
#endif

static void deckenturm_init(Ent *e)
{
	e->Handlung = GEGNER_UNVERWUNDBAR;
	e->blick = -1;
	e->Energy = FX(50);
	e->AnimSpeed = FX(1);
	e->Destroyable = 1;
	e->TestBlock = 0;
}

static void deckenturm_ki(Ent *e)
{
	E2DBG(e);
	e->blick = -1;
	SimpleAnimation(e, 0);

	switch (e->Handlung)
	{
	case GEGNER_UNVERWUNDBAR:          /* waits closed until the player comes near */
		e->DamageTaken = 0;
		e->Destroyable = 0;
		if (PlayerAbstand(e) <= e->Value2 && pl.ypos + FX(70) >= e->yPos)
		{
			e->Destroyable = 1;
			e->Handlung = GEGNER_OEFFNEN;
			e->AnimPhase = 1;
			e->AnimStart = 0;
			e->AnimEnde = 10;
			e->AnimCount = 0;
		}
		break;

	case GEGNER_OEFFNEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->Handlung = GEGNER_VERFOLGEN;
			e->AnimPhase = 10;
			e->AnimEnde = e->AnimPhase;
			e->AnimStart = e->AnimPhase;
			e->AnimCount = 0;
			e->AnimSpeed = FX(e->Value1);
		}
		break;

	case GEGNER_SCHLIESSEN:
		if (e->AnimPhase == e->AnimStart)
		{
			e->Handlung = GEGNER_UNVERWUNDBAR;
			e->AnimEnde = 0;
			e->AnimStart = 0;
		}
		break;

	case GEGNER_VERFOLGEN:             /* aim at the player and shoot, or close again */
	{
		int d1 = PlayerAbstand(e);    /* hypotenuse */
		if (d1 == 0) d1 = 1;
		/* adjacent: the player's middle minus ours */
		const fx d2 = pl.xpos + FX(pl.CollideRect.l + (pl.CollideRect.r - pl.CollideRect.l) / 2) -
		              (e->xPos + FX(GegnerRect[e->GegnerArt].r / 2));
		/* Winkel = RadToDeg(d2 / d1) (as Hurrican: the ratio taken as radians) */
		e->f[0] = FMUL(d2 / d1, FX(57.29578));
		int ap = FINT(FX(16) - e->f[0] / 12);
		if (ap < 10) ap = 10;
		if (ap > 21) ap = 21;
		e->AnimPhase = (int16_t)ap;
		e->AnimEnde = e->AnimPhase;
		e->AnimStart = e->AnimPhase;

		if (e->AnimCount == 0)
		{
			const int w = FINT(e->f[0]);
			proj_push(e->xPos + FX(10) + sin_d(w) * 20, e->yPos + FX(5) + cos_d(w) * 30, SUCHSCHUSS);
		}

		if (PlayerAbstand(e) > e->Value2 || pl.ypos + FX(70) < e->yPos)
		{
			e->Handlung = GEGNER_SCHLIESSEN;
			e->AnimStart = 0;
			e->AnimPhase = 22;
			e->AnimEnde = 30;
			e->AnimSpeed = FX(1);
			e->AnimCount = 0;
		}
	}
	break;

	default:
		break;
	}
}

static void deckenturm_explode(Ent *e)
{
	part_push(e->xPos - FX(10), e->yPos - FX(10), EXPLOSION_MEDIUM2);
	snd(SOUND_EXPLOSION1);
	pl.Score += 200;
	gegner_default_explode(e);
}

/* PC test hook (all E2 files): HB_E2SPAWN="art,x,y[,v1,v2]" adds one object to the stage's
 * map objects (for the types that no map has; they are made when the screen comes near) */
#ifndef OJ32X_MARS
static void deckenturm_load(void)
{
	const char *s = getenv("HB_E2SPAWN");
	int a = 0, x = 0, y = 0, v1 = 0, v2 = 0;
	if (!s || sscanf(s, "%d,%d,%d,%d,%d", &a, &x, &y, &v1, &v2) < 3 || nobjs + 1 > MAX_OBJS) return;
	static uint8_t *copy;
	free(copy);
	copy = malloc(12 * (size_t)(nobjs + 1));
	int k = 0;
	while (k < nobjs && OBJ_X(k) < x) k++;
	memcpy(copy, objs, 12 * (size_t)k);
	uint8_t *n = copy + 12 * k;
	const int vals[6] = { a, x, y, v1, v2, 0 };
	for (int i = 0; i < 6; i++) { n[2 * i] = (uint8_t)(vals[i] >> 8); n[2 * i + 1] = (uint8_t)vals[i]; }
	memcpy(copy + 12 * (k + 1), objs + 12 * k, 12 * (size_t)(nobjs - k));
	objs = copy;
	nobjs++;
}
#else
#define deckenturm_load 0
#endif

TYPEDEF(DECKENTURM) = { deckenturm_init, deckenturm_ki, deckenturm_explode, 0, deckenturm_load };
