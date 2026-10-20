/*
 * The falling rock (Gegner_FallingRock.cpp): waits in the ceiling until the player is below it
 * (within its 80 pixels, under it, nearer than 480), then falls (yAcc 12, at most 50); touching
 * the player costs 40 at once; it bursts when it lands. Not shootable (Destroyable = false).
 * Left out: the smoke and splinter particles (one smoke effect instead).
 */
static void FALLINGROCK_load(void) { }
static void FALLINGROCK_init(Ent *e) { e->state = 0; }
static int FALLINGROCK_step(Ent *e, int idx, int x, int y, int bl, int br, int bo, int bu, int touching)
{
	if (e->state == 0)                       /* waiting */
	{
		const int px = pl.x >> 8, py = pl.y >> 8;
		if (px + player_box.r > x && px < x + 80 && py > y && player_distance(e) < 480)
		{
			e->state = 1;
			e->ys = 0;
			e->ya = FX(12);
			fx_add(&P_smoke, x + 40, y, 4);
			sfx(SFX_LAND);
		}
		return 1;                            /* stays put */
	}
	if (e->ys > FX(50)) e->ya = 0;
	if (touching) { player_damage(DMG(40)); e->energy = 0; return 1; }
	if (e->ys > FX(10) && (bu & (B_WALL | B_PLATFORM))) { e->energy = 0; return 1; }
	return 0;
}
static int FALLINGROCK_explode(Ent *e)
{
	fx_add(&P_smoke, ecx(e), ecy(e), 3);
	sfx(SFX_EXPLODE);
	return 1;
}
static int FALLINGROCK_draw(const Ent *e, uint32_t t) { return 0; }
