/*
 * SURFBRETT - the surf board (triggers/Trigger_SurfBrett.cpp). Hurrican's whole DoKI is commented
 * out (surfing was never finished), so the board only does what the base class does with its
 * constructor's values: it sinks slowly (yAcc 0.25) until it lands.
 * Own members: f[0] = SmokeCount (unused, as in Hurrican). Hurrican's PushGegner passes the map's
 * y as Value1 (unused).
 * (Its picture surfbrett.png is in no stage pack: the type is in no map either.)
 */
static void surfbrett_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->f[0] = 0;
	e->yAcc = FX(0.25);
}

static void surfbrett_ki(Ent *e)
{
	e3test_push(e);              /* (PC check only: HB_E3_PUSH, HB_E3_DUMP) */
	e3test_dump(e);
}

static void surfbrett_explode(Ent *e)
{
	(void)e;
}

TYPEDEF(SURFBRETT) = { surfbrett_init, surfbrett_ki, surfbrett_explode, 0, 0 };
