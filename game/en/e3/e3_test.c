/*
 * E3 test hooks (PC only, not part of Hurrican), used by the E3 files of this folder:
 *  HB_E3_SPAWN=id,x,y[,v1,v2][;id,x,y,v1,v2 ...]
 *      adds objects to the stage's map objects (for the types that are in no map or only
 *      spawned by other objects: KRABBLEROBEN, QUALLE, FIESERWALKER, SCHLEIMALIEN, KETTENGLIED,
 *      COLUMN, SCHLEUSEH, SURFBRETT, EXTRAS ...). Called from extras_load (EXTRAS is type 0, so
 *      its load runs first, after OBJECTS.BIN is mounted).
 *  HB_E3_KETTE=n       a KETTENGLIED with no Rollmops flies off (GEGNER_SPECIAL) after n steps
 *  HB_E3_SHIELD=1     the player keeps a shield (called from stampfstein_ki)
 *  HB_E3_LIFTSPEED=n  the lift level's AutoScrollspeed starts at n (fahrstuhl_init)
 *  HB_E3_PUSH=id,dx,dy,v1,v2[;...]  objects pushed (gegner_push, as a spawning object does)
 *                     once, relative to the first SURFBRETT that runs (spawn one with
 *                     HB_E3_SPAWN): for EXTRAS (object 0 in a map is the player's start)
 *  HB_E3_OPEN=n        a SCHLEUSEH starts opening (GEGNER_OEFFNEN, as a switch does) after n steps
 * On the 32X the hooks are empty.
 */
#ifndef OJ32X_MARS
static uint8_t *e3test_objs;

static void e3test_load(void)
{
	const char *s = getenv("HB_E3_SPAWN");
	if (!s) return;
	while (*s)
	{
		int id = 0, x = 0, y = 0, v1 = 0, v2 = 0;
		const int n = sscanf(s, "%d,%d,%d,%d,%d", &id, &x, &y, &v1, &v2);
		if (n < 3 || nobjs >= MAX_OBJS) return;
		uint8_t *nw = malloc(12 * (size_t)(nobjs + 1));
		if (!nw) return;
		int at = 0;
		while (at < nobjs && OBJ_X(at) < x) at++;
		memcpy(nw, objs, 12 * (size_t)at);
		uint8_t *o = nw + 12 * at;
		const int v[6] = { id, x, y, v1, v2, 0 };
		for (int k = 0; k < 6; k++) { o[2 * k] = (uint8_t)(v[k] >> 8); o[2 * k + 1] = (uint8_t)v[k]; }
		memcpy(nw + 12 * (at + 1), objs + 12 * at, 12 * (size_t)(nobjs - at));
		free(e3test_objs);
		e3test_objs = nw;
		objs = nw;
		nobjs++;
		while (*s && *s != ';') s++;
		if (*s == ';') s++;
	}
}

static int e3test_env(const char *name)
{
	const char *s = getenv(name);
	return s ? atoi(s) : -1;
}

/* HB_E3_SHIELD=1: the player keeps a shield (to ride what would crush him) */
static void e3test_step(void)
{
	static int sh = -2;
	if (sh == -2) sh = e3test_env("HB_E3_SHIELD");
	if (sh > 0) pl.Shield = FX(100);
}

/* HB_E3_PUSH=id,dx,dy,v1,v2[;...]: pushed once (gegner_push) at the first SURFBRETT's place */
static void e3test_push(Ent *e)
{
	static int done;
	const char *s = getenv("HB_E3_PUSH");
	if (done || !s) return;
	done = 1;
	while (*s)
	{
		int id = 0, x = 0, y = 0, v1 = 0, v2 = 0;
		if (sscanf(s, "%d,%d,%d,%d,%d", &id, &x, &y, &v1, &v2) < 3) return;
		gegner_push(e->xPos + FX(x), e->yPos + FX(y), id, v1, v2);
		while (*s && *s != ';') s++;
		if (*s == ';') s++;
	}
}

/* HB_E3_DUMP=n: every n steps (from the first SURFBRETT's DoKI) the objects other than
 * BRUECKE / PARTIKELSPAWN / DIAMANT active near the screen: type, position, Handlung, Energy */
static void e3test_dump(Ent *e)
{
	static int t;
	const int n = e3test_env("HB_E3_DUMP");
	if (n <= 0 || ++t % n) return;
	fprintf(stderr, "step %d:", t);
	for (int k = 0; k < MAX_LIVE; k++)
	{
		const Ent *o = &ents[k];
		if (o->GegnerArt < 0 || o->GegnerArt == BRUECKE || o->GegnerArt == PARTIKELSPAWN || o->GegnerArt == DIAMANT || !o->Active) continue;
		if (abs(FINT(o->xPos - e->xPos)) > 700 || abs(FINT(o->yPos - e->yPos)) > 500) continue;
		fprintf(stderr, " [%d %d,%d h%d e%d a%d]", o->GegnerArt, FINT(o->xPos), FINT(o->yPos), o->Handlung, FINT(o->Energy), o->Active);
	}
	fprintf(stderr, "\n");
}
#else
static void e3test_dump(Ent *e) { (void)e; }
static void e3test_step(void) {}
static void e3test_push(Ent *e) { (void)e; }
static void e3test_load(void) {}
static int e3test_env(const char *name) { (void)name; return -1; }
#endif
