/*
 * SPINNENMASCHINE - the spider machine, boss of the spider factory (bosses/
 * Boss_Spinnenmaschine.cpp): opens its pot lid and lets enemies out (climbing spiders, drones,
 * spider bombs, as its green display shows), then raises its head and fires the Pharao laser;
 * it can only be hit at the eye while the head is up. Destroyed, it explodes for a while and
 * its broken lower part stays.
 * The parts (DoDraw): upper part (spinnenmaschine_oben.png, mirrored) raised by DeckelOffset,
 * the display, the pot lid, the lower part (whole or broken).
 * Own members: the static struct below (one machine at a time).
 * Left out: the light ray when the lid opens (blitztexture.png scaled, additive: a glow),
 * TileEngine.MaxOneUps, the music fades (stage music stops on the lock, boss music once
 * locked, stage music back at the end). Hurrican only starts the fight if the boss song is not
 * already playing; here it always starts.
 * Engine note: pic() compares 16 characters, and spinnenmaschine_oben / _states / _topf /
 * _unten / _unten2.png all start with the same 16; spinnenmaschine_pic() finds them by their
 * order in the pack (the converter sorts the names).
 */
enum { SPM_ZU, SPM_OFFEN, SPM_OEFFNEN, SPM_SCHLIESSEN };
#define SPM_TIME_TILL_OPEN  FX(10)
#define SPM_TIME_TILL_CLOSE FX(40)
#define SPM_TIME_TILL_HOCH  FX(30)
#define SPM_PI              FX(3.14159265)

static struct {
	int DisplayState, OldDisplayState, DeckelPhase, AnimUnten;
	int DeckelStatus, HochStatus;
	fx DeckelCount, DeckelOffset, OpenCounter, HochCounter, ShotDelay, SpawnDelay;
	fx SmokeDelay, SmokeDelay2, LightRayCount, DeathCount;
	int AktionFertig;
} spm;
static Pic spinnenmaschine_display, spinnenmaschine_deckel, spinnenmaschine_unten[2];

/* the n-th picture whose name starts with the same 16 characters as name (stage pack first) */
static Pic spinnenmaschine_pic(const char *name, int n)
{
	(void)n;                      /* the packs keep whole names now */
	return pic(name);
}

static void spinnenmaschine_load(void)
{
	/* sorted: _oben, _states, _topf, _unten, _unten2 */
	GegnerPic[SPINNENMASCHINE] = spinnenmaschine_pic("spinnenmaschine_oben.png", 0);
	spinnenmaschine_display = spinnenmaschine_pic("spinnenmaschine_states.png", 1);
	spinnenmaschine_deckel = spinnenmaschine_pic("spinnenmaschine_topf.png", 2);
	spinnenmaschine_unten[0] = spinnenmaschine_pic("spinnenmaschine_unten.png", 3);
	spinnenmaschine_unten[1] = spinnenmaschine_pic("spinnenmaschine_unten2.png", 4);
}

static void spinnenmaschine_init(Ent *e)
{
	memset(&spm, 0, sizeof spm);
	spm.DeckelStatus = SPM_ZU;
	spm.HochStatus = SPM_ZU;
	spm.OpenCounter = SPM_TIME_TILL_OPEN;
	spm.HochCounter = SPM_TIME_TILL_HOCH;
	spm.ShotDelay = FX(5);
	spm.SpawnDelay = FX(8);
	e->Handlung = GEGNER_INIT;
	e->blick = -1;
	e->Energy = FX(4000);
	e->Destroyable = 1;
	e->TestBlock = 0;
	e->OwnDraw = 1;
	GegnerRect[SPINNENMASCHINE] = (Box){ 0, 0, 400, 300 };
}

static void spinnenmaschine_draw(Ent *e)
{
	if (e->Handlung != GEGNER_SPECIAL)
	{
		/* (the light ray when the lid is open: a glow, left out) */
		/* DeckelOffset = -(cos(DeckelCount) * 20 - 20) */
		const int deg = FINT(FMUL(spm.DeckelCount, FX(57.2957795)));
		spm.DeckelOffset = -(cos_d(deg) * 20 - FX(20));
		draw_sprite(&GegnerPic[e->GegnerArt], 0, e->xPos, e->yPos - spm.DeckelOffset, 1);
		draw_sprite(&spinnenmaschine_display, spm.DisplayState, e->xPos + FX(133), e->yPos + FX(263) - spm.DeckelOffset, 1);
		draw_sprite(&spinnenmaschine_deckel, spm.DeckelPhase, e->xPos + FX(75), e->yPos + FX(159) - spm.DeckelOffset, 1);
	}
	/* the lower part (never flashes) */
	const int fl = draw_flash;
	draw_flash = 0;
	draw_sprite(&spinnenmaschine_unten[spm.AnimUnten], 0, e->xPos + FX(45), e->yPos + FX(352), 1);
	draw_flash = fl;
}

/* the lid up and down */
static void spinnenmaschine_deckel_step(Ent *e)
{
	switch (spm.DeckelStatus)
	{
	case SPM_ZU:                            /* closed, counting down to opening */
		spm.OpenCounter -= SYNC(FX(1));
		if (spm.OpenCounter < 0)
		{
			spm.OpenCounter = 0;
			spm.DeckelStatus = SPM_OEFFNEN;
			e->AnimCount = 0;
		}
		break;
	case SPM_OEFFNEN:                       /* opening */
		spm.LightRayCount += SYNC(FX(1));
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount > FX(0.8)) { e->AnimCount = 0; spm.DeckelPhase += 1; }
		if (spm.DeckelPhase > 10)
		{
			spm.DeckelPhase = 10;
			spm.DeckelStatus = SPM_OFFEN;
			spm.OpenCounter = SPM_TIME_TILL_CLOSE;
		}
		break;
	case SPM_OFFEN:                         /* open, counting down to closing; enemies come out */
		spm.OpenCounter -= SYNC(FX(1));
		if (spm.OpenCounter < 0)
		{
			spm.OpenCounter = 0;
			spm.DeckelStatus = SPM_SCHLIESSEN;
			e->AnimCount = 0;
		}
		spm.SpawnDelay -= SYNC(FX(1));
		if (spm.SpawnDelay < 0)
		{
			switch (spm.DisplayState)
			{
			case 1:                         /* climbers */
				spm.SpawnDelay = FX(6);
				gegner_push(e->xPos + FX(100) + FX(GetRandom(60)), e->yPos + FX(190) - spm.DeckelOffset, CLIMBSPIDER, 99, 0);
				break;
			case 2:                         /* drones */
				spm.SpawnDelay = FX(20);
				gegner_push(e->xPos + FX(135), e->yPos + FX(190) - spm.DeckelOffset, DRONE, 99, 0);
				break;
			case 3:                         /* spider bombs */
				spm.SpawnDelay = FX(15);
				gegner_push(e->xPos + FX(100) + FX(GetRandom(80)), e->yPos + FX(180) - spm.DeckelOffset, SPIDERBOMB, 99, 0);
				break;
			}
		}
		break;
	case SPM_SCHLIESSEN:                    /* closing */
		spm.LightRayCount -= SYNC(FX(1));
		e->AnimCount += SYNC(FX(1));
		if (e->AnimCount > FX(0.8)) { e->AnimCount = 0; spm.DeckelPhase -= 1; }
		if (spm.DeckelPhase < 0)
		{
			spm.DeckelPhase = 0;
			spm.DeckelStatus = SPM_ZU;
			spm.OpenCounter = SPM_TIME_TILL_OPEN;
			spm.LightRayCount = 0;
			spm.AktionFertig = 1;
		}
		break;
	}
}

/* the head up and down */
static void spinnenmaschine_hoch_step(Ent *e)
{
	switch (spm.HochStatus)
	{
	case SPM_ZU:                            /* down, counting down to going up */
		spm.HochCounter -= SYNC(FX(1));
		if (spm.HochCounter < 0)
		{
			spm.HochCounter = 0;
			spm.HochStatus = SPM_OEFFNEN;
			snd(SOUND_STEAM);
		}
		break;
	case SPM_OEFFNEN:                       /* going up */
		spm.DeckelCount += SYNC(FX(0.2));
		if (spm.DeckelCount > SPM_PI)
		{
			spm.DeckelCount = SPM_PI;
			spm.HochStatus = SPM_OFFEN;
			spm.HochCounter = SPM_TIME_TILL_HOCH * 2;
			spm.ShotDelay = FX(10);
			snd(SOUND_DOORSTOP);
		}
		if (spm.DeckelCount < SPM_PI)
		{
			spm.SmokeDelay -= SYNC(FX(1));
			if (spm.SmokeDelay < 0)
			{
				spm.SmokeDelay = FX(0.4);
				part_push(e->xPos + FX(55), e->yPos + FX(375), SMOKE3_LU);
				part_push(e->xPos + FX(245), e->yPos + FX(375), SMOKE3_RU);
			}
		}
		break;
	case SPM_OFFEN:                         /* up: shoots, counting down to going down */
		spm.HochCounter -= SYNC(FX(1));
		if (spm.HochCounter < 0)
		{
			spm.HochCounter = 0;
			spm.HochStatus = SPM_SCHLIESSEN;
			snd(SOUND_STEAM);
		}
		spm.ShotDelay -= SYNC(FX(1));
		if (spm.ShotDelay <= 0)
		{
			spm.ShotDelay = FX(15);
			proj_push(e->xPos + FX(230), e->yPos + FX(310), PHARAOLASER);
			snd(SOUND_PHARAODIE);
			snd(SOUND_LASERSHOT);
		}
		break;
	case SPM_SCHLIESSEN:                    /* going down */
		spm.DeckelCount -= SYNC(FX(0.2));
		if (spm.DeckelCount <= 0)
		{
			spm.DeckelCount = 0;
			spm.HochStatus = SPM_ZU;
			spm.HochCounter = SPM_TIME_TILL_HOCH;
			for (int i = 1; i < 10; i++) part_push(e->xPos + FX(i * 25), e->yPos + FX(330), SMOKEBIG);
			snd(SOUND_DOORSTOP);
			spm.AktionFertig = 1;
		}
		break;
	}
}

static void spinnenmaschine_ki(Ent *e)
{
	/* the energy bar */
	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_SPECIAL && e->Handlung != GEGNER_EXPLODIEREN)
		boss_bar(e->Energy, FX(4000));

	/* lock the screen, music */
	if (e->Active && e->Handlung != GEGNER_SPECIAL && tile_zustand() == TS_SCROLLBAR)
	{
		if (PlayerAbstandHoriz(e) < 450)
		{
			scroll_level(FX(e->Value1), FX(e->Value2), TS_SCROLLTOLOCK);
			music_stop();                   /* FadeSong(STAGEMUSIC, -2, 0, pause) */
		}
	}

	/* a boss does not flash as long as the others */
	if (e->DamageTaken > 0) e->DamageTaken -= SYNC(FX(50));
	else e->DamageTaken = 0;

	/* no energy: it explodes */
	if (e->Energy <= FX(100) && e->Handlung != GEGNER_EXPLODIEREN && e->Handlung != GEGNER_SPECIAL)
	{
		e->Handlung = GEGNER_EXPLODIEREN;
		spm.SpawnDelay = 0;
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		spm.DeathCount = FX(40);
		gegner_push(FX(140), FX(820), ONEUP, 0, 0);
		music_stop();                       /* FadeSong(BOSS, -2, 0) */
	}

	/* steam when damaged */
	if (e->Handlung != GEGNER_SPECIAL) spm.SmokeDelay2 -= SYNC(FX(1));
	if (spm.SmokeDelay2 < 0)
	{
		const fx o = spm.DeckelOffset;
		spm.SmokeDelay2 = FX(0.3);
		if (e->Energy < FX(3800)) part_push(e->xPos + FX(45), e->yPos + FX(200) - o, SMOKE3_LO);
		if (e->Energy < FX(3600)) part_push(e->xPos + FX(247), e->yPos + FX(278) - o, SMOKE3_RO);
		if (e->Energy < FX(3400)) part_push(e->xPos + FX(228), e->yPos + FX(202) - o, SMOKE3_RO);
		if (e->Energy < FX(3100)) part_push(e->xPos + FX(40), e->yPos + FX(228) - o, SMOKE3_LO);
		if (e->Energy < FX(2600)) part_push(e->xPos + FX(270), e->yPos + FX(310) - o, SMOKE3_R);
		if (e->Energy < FX(2200)) part_push(e->xPos + FX(178), e->yPos + FX(205) - o, SMOKE3);
		if (e->Energy < FX(1800)) part_push(e->xPos + FX(178), e->yPos + FX(205) - o, SMOKE3);
		if (e->Energy < FX(1400)) part_push(e->xPos + FX(73), e->yPos + FX(280) - o, SMOKE3_L);
		if (e->Energy < FX(1000)) part_push(e->xPos + FX(108), e->yPos + FX(389), SMOKE3_LU);
	}

	switch (e->Handlung)
	{
	case GEGNER_INIT:                       /* wait until the screen is locked */
		if (tile_zustand() == TS_LOCKED)
		{
			music_boss();
			e->Handlung = GEGNER_EINFLIEGEN;
		}
		break;

	case GEGNER_EINFLIEGEN:
		e->Energy = FX(4000);
		e->DamageTaken = 0;
		spm.DisplayState = GetRandom(3) + 1;
		spm.OldDisplayState = spm.DisplayState;
		e->Handlung = GEGNER_LAUFEN;
		break;

	case GEGNER_STEHEN:                     /* the head up and down */
		spinnenmaschine_hoch_step(e);
		if (spm.AktionFertig)
		{
			spm.AktionFertig = 0;
			e->Handlung = GEGNER_LAUFEN;
			do spm.DisplayState = GetRandom(3) + 1;
			while (spm.DisplayState == spm.OldDisplayState);
			spm.OldDisplayState = spm.DisplayState;
		}
		break;

	case GEGNER_LAUFEN:                     /* the lid */
		spinnenmaschine_deckel_step(e);
		if (spm.AktionFertig)
		{
			spm.AktionFertig = 0;
			e->Handlung = GEGNER_STEHEN;
			spm.DisplayState = 0;
		}
		break;

	case GEGNER_SPECIAL:                    /* broken: smokes */
		if (PlayerAbstand(e) < 800)
		{
			spm.SmokeDelay -= SYNC(FX(1));
			if (spm.SmokeDelay < 0)
			{
				spm.SmokeDelay = FX(1);
				part_push(e->xPos + FX(GetRandom(250)), e->yPos + FX(GetRandom(100) + 300), SMOKEBIG);
			}
		}
		break;

	case GEGNER_EXPLODIEREN:
		e->Energy = FX(100);
		spm.SpawnDelay -= SYNC(FX(1));
		if (spm.SpawnDelay < 0)
		{
			spm.SpawnDelay = FX(0.4);
			const int xo = GetRandom(300), yo = GetRandom(400);
			part_push(e->xPos + FX(xo), e->yPos + FX(yo), EXPLOSION_MEDIUM2);
			if (GetRandom(2) == 0) part_push(e->xPos + FX(GetRandom(300)), e->yPos + FX(GetRandom(400)), SMOKEBIG);
			if (GetRandom(10) == 0)
				part_push(e->xPos + FX(GetRandom(100) + 100), e->yPos + FX(GetRandom(200) + 200), EXPLOSION_TRACE);
			if (GetRandom(3) == 0) snd(SOUND_EXPLOSION3 + GetRandom(2));
			if (yo > 100 && GetRandom(5) == 0)
				for (int i = 0; i < 10; i++)
					part_push(e->xPos + FX(xo - 10 + GetRandom(20)), e->yPos + FX(yo - 10 + GetRandom(20)), SPIDERSPLITTER);
		}
		spm.DeathCount -= SYNC(FX(1));
		/* done exploding: it falls apart, the lower part stays */
		if (spm.DeathCount < 0)
		{
			e->Handlung = GEGNER_SPECIAL;
			e->Energy = FX(1);
			e->Destroyable = 0;
			pl.Score += 8000;
			snd(SOUND_EXPLOSION2);
			/* ScrolltoPlayeAfterBoss */
			scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
			music_stage();
			shake_screen(5);
			for (int i = 0; i < 10; i++)
			{
				part_push(e->xPos + FX(GetRandom(300)), e->yPos + FX(GetRandom(400)), SPIDERSPLITTER);
				part_push(e->xPos + FX(50) + FX(GetRandom(200)), e->yPos + FX(100) + FX(GetRandom(300)), EXPLOSION_TRACE);
			}
			for (int i = 0; i < 50; i++)
			{
				part_push(e->xPos + FX(GetRandom(300)), e->yPos + FX(100) + FX(GetRandom(300)), EXPLOSION_MEDIUM2);
				part_push(e->xPos + FX(GetRandom(300)), e->yPos + FX(100) + FX(GetRandom(300)), SMOKEBIG);
			}
			for (int i = 0; i < 300; i++)
				part_push(e->xPos + FX(GetRandom(300)), e->yPos + FX(100) + FX(GetRandom(300)), FUNKE);
			spm.AnimUnten = 1;              /* the lower part: broken */
		}
		break;

	default:
		break;
	}

	/* lid closed: it cannot be hit; else at the eye */
	if (spm.HochStatus == SPM_ZU)
	{
		GegnerRect[e->GegnerArt] = (Box){ 0, 0, 0, 0 };
		e->Destroyable = 0;
	}
	else if (e->Handlung != GEGNER_SPECIAL)
	{
		GegnerRect[e->GegnerArt] = (Box){ 204, (int16_t)(350 - FINT(spm.DeckelOffset)), 204 + 47, 350 };
		e->Destroyable = 1;
	}

	/* the player cannot get past it */
	if (e->Handlung != GEGNER_SPECIAL && pl.xpos < e->xPos + FX(250))
	{
		if (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL)
		{
			if (pl.Blickrichtung == -1) pl.Blickrichtung = 1;
		}
		else pl.xpos = e->xPos + FX(250);
	}
}

/* it does not explode: it stays, broken */
static void spinnenmaschine_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(SPINNENMASCHINE) = { spinnenmaschine_init, spinnenmaschine_ki, spinnenmaschine_explode, spinnenmaschine_draw, spinnenmaschine_load };
