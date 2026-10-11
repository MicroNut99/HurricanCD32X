/*
 * The air vents (Trigger_PartikelSpawner.cpp, Value1 5 / 6 with Value2 0, and 19): rising smoke
 * that lifts the player while he jumps through it - the way up to higher platforms.
 * Hurrican's DoKI: while the player is in the air (yspeed != 0) and his box meets the column
 * (x .. x + 40, from 300 above to 40 below the vent), yspeed -= Timer.sync(6) (type 19:
 * Timer.sync(Value2)). Type 6 pauses: its Aussetzer counts 0..10 (0.2 a unit), the smoke shows
 * above 4 and lifts above 6.
 * Drawn: the smoke column as four puffs of smokebig.png rising 300 pixels from the vent (Hurrican
 * pushes SMOKE3 particles every 0.25 units and a SMOKEBIG now and then) - so it is seen where to
 * jump. Other spawner kinds (map object 140 with other values) are left out (effects only).
 *   t1: Aussetzer x 60 (type 6), t2: the smoke's phase
 */
static void VENT_load(void) { }
static void VENT_init(Ent *e)
{
	const int lifts = ((e->v1 == 5 || e->v1 == 6) && e->v2 == 0) || e->v1 == 19;
	if (!lifts) { e->type = E_NONE; return; }
	e->t1 = 0;
	e->t2 = (int16_t)rnd(300);
}
static int VENT_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	(void)idx; (void)bl; (void)br; (void)bo; (void)bu; (void)touching;
	int smoke = 1, lift = 1;
	if (e->v1 == 6)
	{
		e->t1 = (int16_t)(e->t1 + 2);                /* 0.2 a unit = 2 per step in 1/60s of units x 60 */
		if (e->t1 > 600) e->t1 = 0;
		smoke = e->t1 > 240;
		lift = e->t1 > 360;
	}
	e->state = (uint8_t)smoke;
	if (smoke) e->t2 = (int16_t)((e->t2 + 3) % 300);
	if (lift && pl.state != PS_DEAD && pl.ys != 0)
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		if (px + player_box.l < x + 40 && x < px + player_box.r && py + player_box.t < y + 40 && y - 300 < py + player_box.b)
			pl.ys -= SYNC(FX(e->v1 == 19 ? e->v2 : 6));
	}
	return 1;                                         /* it stays put */
}
static int VENT_explode(Ent *e) { (void)e; return 1; }
static int VENT_draw(const Ent *e, uint32_t t)
{
	(void)t;
	if (!e->state || !P_VENT.count) return 1;
	const int x = e->x >> 8, y = e->y >> 8;
	for (int k = 0; k < 4; k++)
	{
		const int h = (e->t2 + k * 75) % 300;            /* how far this puff has risen */
		const int f = h * P_VENT.count / 300;            /* it grows as it rises */
		draw_pic(&P_VENT, f, FX(x - 10 + ((k * 7) & 7)), FX(y - 20 - h), 0);
	}
	return 1;
}
