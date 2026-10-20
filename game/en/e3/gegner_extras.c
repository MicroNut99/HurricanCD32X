/*
 * EXTRAS - the power-ups (enemies/Gegner_Extras.cpp). Value1 is the kind:
 *  0 spread, 1 laser, 2 bounce, 3 lightning, 4 shield, 5 energy, 6 wheel energy, 7 power line,
 *  8 grenade, 9 smart bomb, 10 autofire, 11 big shots, 12 bonus points.
 * Value2 == 0: flies out in an arc (from a power block, an eye ...), else floats where it is.
 * Own members: none.
 * Left out: the power-up ring effect (300 KRINGELR / KRINGELHB particles and
 * ClearPowerUpEffects: those particles are dropped in game/part.c), the tutorial's faster grenade.
 * (AUGE with Value2 == 1 becomes an EXTRAS in Hurrican's PushGegner: the AUGE port calls this
 * init with GegnerArt changed - it only uses Value1 / Value2.)
 */
#define EXTRAS_PLAYER_MAXEXTRAS 9          /* Player.hpp PLAYER_MAXEXTRAS */

static void extras_init(Ent *e)
{
	e->blick = -1;
	e->Energy = FX(1);
	e->AnimPhase = e->Value1;
	e->ChangeLight = 0;
	if (e->Value2 == 0)
	{
		e->Handlung = GEGNER_SPRINGEN;
		e->ySpeed = FX(GetRandom(40)) / 10 - FX(20);
		e->yAcc = FX(5);
		if (e->Value1 == 0) e->xSpeed = FX(9);
		else if (e->Value1 == 1) e->xSpeed = FX(-9);
		else if (e->Value1 == 2) e->xSpeed = 0;
		else
		{
			e->xSpeed = FX(GetRandom(80) + 20) / 8;
			if (GetRandom(2) == 0) e->xSpeed = -e->xSpeed;
		}
		e->xSpeed += FX(GetRandom(20) - 10) / 5;
	}
	else
		e->Handlung = GEGNER_SPECIAL;
	e->Destroyable = 0;
}

static void extras_ki(Ent *e)
{
	e->blick = -1;

	switch (e->Handlung)
	{
	case GEGNER_SPRINGEN:      /* still flying out */
		if ((e->blockl & B_WAND) || (e->blockr & B_WAND)) e->xSpeed = -e->xSpeed;
		if (e->ySpeed >= FX(25))
		{
			e->ySpeed = FX(-1);
			e->xSpeed = 0;
			e->yAcc = 0;
			e->Handlung = GEGNER_STEHEN;
		}
		break;
	case GEGNER_STEHEN:        /* floats up slowly */
		if (e->blocko & B_WAND) e->ySpeed = 0;
		if (e->xSpeed > 0)
		{
			e->xSpeed -= SYNC(FX(0.5));
			if (e->xSpeed < 0) e->xSpeed = 0;
		}
		if (e->xSpeed < 0)
		{
			e->xSpeed += SYNC(FX(0.5));
			if (e->xSpeed > 0) e->xSpeed = 0;
		}
		break;
	}

	/* collected? */
	if (!SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect)) return;

	const int v = e->Value1;
	pl.Score += 10;
	part_push(e->xPos - FX(4), e->yPos - FX(4), EXTRACOLLECTED);

	/* weapon upgrade */
	if (v >= 0 && v < 3)
	{
		if (pl.CurrentWeaponLevel[v] < 8)
		{
			pl.CurrentWeaponLevel[v]++;
			snd(SOUND_UPGRADE);
			pl.BlinkCounter = FX(9.5);
			pl.BlinkColor = v + 1;
		}
		if (pl.CurrentWeaponLevel[v] > pl.CurrentWeaponLevel[pl.SelectedWeapon]) pl.SelectedWeapon = v;
	}
	/* lightning upgrade */
	if (v == 3 && pl.CurrentWeaponLevel[3] < 16)
	{
		pl.CurrentWeaponLevel[3]++;
		snd(SOUND_UPGRADE);
		pl.BlinkCounter = FX(9.5);
		pl.BlinkColor = 4;
	}
	/* shield */
	if (v == 4)
	{
#if !PLF_DRAW_SHIELD
		if (pl.Shield <= 0)
		{
			proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER);
			proj_push(pl.xpos, pl.ypos, SHIELDSPAWNER2);
		}
#endif
		pl.Shield = FX(100);
	}
	if (v == 5) pl.Energy = MAX_ENERGY;
	if (v == 6) pl.Armour = MAX_ARMOUR;
	if (v == 7 && pl.PowerLines < EXTRAS_PLAYER_MAXEXTRAS) pl.PowerLines++;
	if (v == 8 && pl.Grenades < EXTRAS_PLAYER_MAXEXTRAS) pl.Grenades++;
	if (v == 9 && pl.SmartBombs < 1) pl.SmartBombs++;
	if (v == 10) pl.AutoFireExtra += FX(120);
	if (v == 11) pl.RiesenShotExtra += FX(120);
	if (v == 12) pl.Score += 100;

	e->Energy = 0;

	if (v >= 0 && v <= 12) snd(SOUND_VOICE_SPREAD + v);
	switch (v)
	{
	case 8: case 9: case 10:
		snd(SOUND_AMMO);
		break;
	default:
		snd(SOUND_COLLECT);
		break;
	}
	snd(SOUND_COLLECT);
}

static void extras_explode(Ent *e)
{
	(void)e;
}

static void extras_load(void)
{
	e3test_load();               /* (PC check only: HB_E3_SPAWN) */
}

TYPEDEF(EXTRAS) = { extras_init, extras_ki, extras_explode, 0, extras_load };
