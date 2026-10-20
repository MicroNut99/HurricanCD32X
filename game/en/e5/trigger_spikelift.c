/*
 * SPIKELIFT - the spike lift (triggers/Trigger_Spikelift.cpp). Its DoKI is all "#if 0" in
 * Hurrican (it does nothing); only drawn: the lower part, and the spikes when Value1 == 0.
 * Own members: none.
 * (spikelift.png is in none of the 9 stage packs: nothing is drawn there.)
 */
static void spikelift_init(Ent *e)
{
	e->Handlung = GEGNER_INIT;
	e->blick = 1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->AnimCount = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

static void spikelift_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], 1, e->xPos, e->yPos + FX(40), 0);
	if (e->Value1 == 0) draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos, 0);
}

static void spikelift_ki(Ent *e)
{
	E5_TEST_HOOK(e);
}
static void spikelift_explode(Ent *e) { (void)e; }

TYPEDEF(SPIKELIFT) = { spikelift_init, spikelift_ki, spikelift_explode, spikelift_draw, 0 };
