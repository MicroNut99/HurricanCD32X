/*
 * B2 test hooks (PC only, not part of Hurrican): used by the B2 boss files (boss_*.c here).
 *  HB_B2_SPAWN=id,x,y[,v1,v2]  adds an object to the stage's map objects (for the bosses that
 *                              are in no map: RIESENPIRANHA, RIESENRAUPE, WUXESPINNEN)
 *  HB_B2_HURT=n                the B2 bosses lose n energy every step while they fight (to
 *                              check their death without hours of shooting, or the
 *                              Schneekoenig that only its own snowballs hurt)
 *  HB_B2_PIN=x,y               holds the player at x,y while a B2 boss fights (where the
 *                              platforms of other groups are missing)
 * On the 32X the hooks are empty.
 */
#ifndef OJ32X_MARS
static uint8_t *b2test_objs;

/* (from a B2 type's load: OBJECTS.BIN is mounted, objs / nobjs are set) */
static void b2test_load(void)
{
	const char *s = getenv("HB_B2_SPAWN");
	int id = 0, x = 0, y = 0, v1 = 0, v2 = 0;
	if (!s || sscanf(s, "%d,%d,%d,%d,%d", &id, &x, &y, &v1, &v2) < 3 || nobjs >= MAX_OBJS) return;
	uint8_t *n = malloc(12 * (size_t)(nobjs + 1));
	if (!n) return;
	int at = 0;
	while (at < nobjs && OBJ_X(at) < x) at++;
	memcpy(n, objs, 12 * (size_t)at);
	uint8_t *o = n + 12 * at;
	const int v[6] = { id, x, y, v1, v2, 0 };
	for (int k = 0; k < 6; k++) { o[2 * k] = (uint8_t)(v[k] >> 8); o[2 * k + 1] = (uint8_t)v[k]; }
	memcpy(n + 12 * (at + 1), objs + 12 * at, 12 * (size_t)(nobjs - at));
	free(b2test_objs);
	b2test_objs = n;
	objs = n;
	nobjs++;
}

static void b2test_hurt(Ent *e)
{
	static int n = -1;
	if (n < 0) n = getenv("HB_B2_HURT") ? atoi(getenv("HB_B2_HURT")) : 0;
	if (n > 0 && e->Energy > 0) e->Energy -= FX(n);
	static int px = -1, py;
	if (px == -1) { px = -2; if (getenv("HB_B2_PIN")) sscanf(getenv("HB_B2_PIN"), "%d,%d", &px, &py); }
	if (px >= 0) { pl.xpos = pl.xposold = FX(px); pl.ypos = pl.yposold = FX(py); pl.yspeed = 0; }
}
#else
#define b2test_load() ((void)0)
#define b2test_hurt(e) ((void)0)
#endif
