/*
 * LIGHTFLARE - a slowly turning light flare (triggers/Trigger_LightFlare.cpp): Value2 its colour,
 * fades to half when the player is in front of it.
 * Own members: f[0] = rot, f[1] = rotspeed, f[2] = Alpha, i[0] = r, i[1] = g, i[2] = b.
 * Not drawn: it is only an additive, tinted, rotated glow (lavaflare.png) - no glows here. Its
 * logic runs as Hurrican's.
 */
static void lightflare_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->f[0] = 0;
	e->f[1] = FX(GetRandom(10) + 1) / 3;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	int r = 0, g = 0, b = 0;
	switch (e->Value2)
	{
	case 1: r = 255; break;
	case 2: g = 255; break;
	case 3: b = 255; break;
	case 4: r = g = 255; break;
	case 5: r = b = 255; break;
	case 6: g = b = 255; break;
	default: r = g = b = 255; break;
	}
	e->i[0] = r; e->i[1] = g; e->i[2] = b;
	e->Value1 = 255;
	e->f[2] = FX(e->Value1);
}

static void lightflare_ki(Ent *e)
{
	E2DBG(e);
	e->f[0] += SYNC(e->f[1]);
	while (e->f[0] >= FX(360)) e->f[0] -= FX(360);
	/* the player in front: fade out */
	if (PlayerAbstand(e) < 20)
	{
		if (e->f[2] > FX(128)) e->f[2] -= SYNC(FX(75));
		if (e->f[2] < FX(128)) e->f[2] = FX(128);
	}
	else
	{
		if (e->f[2] < FX(e->Value1)) e->f[2] += SYNC(FX(75));
		if (e->f[2] > FX(e->Value1)) e->f[2] = FX(e->Value1);
	}
}

static void lightflare_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(LIGHTFLARE) = { lightflare_init, lightflare_ki, lightflare_explode, 0, 0 };
