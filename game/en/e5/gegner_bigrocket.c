/*
 * BIGROCKET - the fat rocket (enemies/Gegner_BigRocket.cpp): flies up from below the screen
 * in the flying-sack level.
 * Own members: f[0] = smokedelay.
 * Left out: the LavaFlare glow of the exhaust (a glow), the SMOKE3_U / ROCKETSMOKE trail and
 * the MINIFLARE / LONGFUNKE of the explosion (dropped particle types), ShakeScreen.
 * (bigrocket.png is in none of the 9 stage packs: nothing is drawn there.)
 */
static void bigrocket_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(1000);
	e->Destroyable = 1;
	e->AnimSpeed = FX(1.5);
	e->AnimEnde = 15;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	e->f[0] = 0;
}

static void bigrocket_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, 0);
}

static void bigrocket_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	SimpleAnimation(e, 0);
	switch (e->Handlung)
	{
	case GEGNER_STEHEN:            /* put below the player, to fly up */
		e->Handlung = GEGNER_FALLEN;
		e->yPos += FX(RENDERHEIGHT + 300);
		e->ySpeed = -FX(e->Value1 + 7);
		e->Value2 = (int16_t)FINT(e->xPos);
		break;
	case GEGNER_FALLEN:
		e->xPos = FX(e->Value2) + FX((FINT(e->yPos) % 14) / 7);
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0) e->f[0] += FX(0.2);    /* (SMOKE3_U / ROCKETSMOKE: dropped types) */
		e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, &GegnerRect[e->GegnerArt], 0);
		if (e->yPos + FX(350) < cam_yoff() || (e->blocko & B_WAND)) e->Energy = 0;
		break;
	}
	TestDamagePlayers(e, FX(30), 1);
}

static void bigrocket_explode(Ent *e)
{
	if (!(e->yPos + FX(350) < cam_yoff()))
	{
		shake_screen(5);
		snd(SOUND_EXPLOSION1);
		snd(SOUND_EXPLOSION3);
		for (int i = 0; i < 30; i++)
		{
			part_push(e->xPos + FX(GetRandom(60)), e->yPos + FX(30) + FX(GetRandom(250)), EXPLOSION_MEDIUM2);
			part_push(e->xPos + FX(10) + FX(GetRandom(50)), e->yPos + FX(30) + FX(GetRandom(250)), SPIDERSPLITTER);
		}
		pl.Score += 2000;
	}
}

TYPEDEF(BIGROCKET) = { bigrocket_init, bigrocket_ki, bigrocket_explode, bigrocket_draw, 0 };
