/*
 * SCHNEEKOENIG - the snow king of the ice stage (bosses/Boss_Schneekoenig.cpp): drops into the
 * screen, aims its gun and fires snowballs (SCHNEEKOPPE, other group) at the player, jumps on
 * him or hops from side to side dropping blue bombs. Only its own snowballs hurt it (5000
 * each; the player can stand on it). It comes three times (Value1 0, 1, 2): when it is beaten
 * it flies off (the third time it blows up on the way).
 * Own members: f[0] = ySave, f[1] = LastEnergy, f[2] = SmokeDelay, f[3] = yOffset,
 *   f[4] = KnarreWinkel (degrees), f[5] = KnarreY, f[6] = ShotDelay, f[7] = WackelAnim;
 *   i[0] = ShotCount, i[1] = Action, i[2] = DrawNow, i[3] = GunSlide (fx).
 *   SpezialAktion is never used; WackelCount is never set above 0 in Hurrican (left out).
 * Drawing: the gun is drawn upright (Hurrican turns it by KnarreWinkel and slides it by
 * GunSlide). Hurrican counts the body's shake (xoff) and its hit animation (WackelAnim) in its
 * DoDraw; here in DoKI, so they keep the game's speed whatever the frame rate.
 * Left out: screen shaking. Hurrican fades the music: here it stops / starts.
 */
static Pic schneekoenig_knarre;
static const int8_t schneekoenig_damage_anims[4] = { 1, 2, 1, 0 };
static fx schneekoenig_xoff;                  /* DoDraw's static xoff (radians) */
static int schneekoenig_last_attack, schneekoenig_next_attack;   /* NextAction's statics */

static int schneekoenig_sin(fx rad)
{
	return sin_d(FINT(FMUL(rad, FX(57.29578))));
}

static void schneekoenig_init(Ent *e)
{
	e->i[0] = 0;
	e->i[1] = -1;
	e->f[2] = 0;
	e->f[3] = 0;
	e->f[4] = 0;
	e->f[5] = FX(60);
	e->f[6] = 0;
	e->f[7] = 0;
	e->i[3] = 0;
	e->i[2] = 0;
	e->Handlung = GEGNER_INIT;
	e->blick = -1;
	e->Energy = FX(50000);
	e->f[1] = FX(50000);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->OwnDraw = 1;
}

static void schneekoenig_draw(Ent *e)
{
	if (!e->i[2]) return;
	/* the gun */
	draw_sprite(&schneekoenig_knarre, 0, e->xPos + FX(95) + schneekoenig_sin(schneekoenig_xoff) * 3 / 2,
	            e->yPos + e->f[3] - FX(36) + e->f[5], 0);
	/* the body */
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos + 5 * schneekoenig_sin(schneekoenig_xoff),
	            e->yPos + e->f[3], 0);
}

/* the angle the gun aims at the player */
static fx schneekoenig_winkel_to_player(Ent *e)
{
	return (e->xPos + FX(100) - (pl.xpos + FX(35))) / 10;
}

/* the next attack */
static void schneekoenig_next_action(Ent *e, int NewAction)
{
	e->Handlung = GEGNER_EINFLIEGEN;
	if (NewAction != -1) schneekoenig_next_attack = NewAction;
	else
	{
		schneekoenig_next_attack = schneekoenig_last_attack + 1;
		if (schneekoenig_next_attack > 3 || (schneekoenig_next_attack == 2 && e->Value1 == 1)) schneekoenig_next_attack = 0;
	}
	switch (schneekoenig_next_attack)
	{
	case 0: e->i[1] = GEGNER_INIT2; break;     /* shoot snowballs */
	case 1: e->i[1] = GEGNER_INIT3; break;     /* gun in and jump */
	case 2: e->i[1] = GEGNER_INIT2; break;
	case 3: e->i[1] = GEGNER_INIT4; break;     /* hop from side to side, dropping bombs */
	}
	schneekoenig_last_attack = schneekoenig_next_attack;
	e->f[4] = 0;
}

static void schneekoenig_ki(Ent *e)
{
	fx *ySave = &e->f[0], *LastEnergy = &e->f[1], *SmokeDelay = &e->f[2], *yOffset = &e->f[3];
	fx *KnarreWinkel = &e->f[4], *KnarreY = &e->f[5], *ShotDelay = &e->f[6], *WackelAnim = &e->f[7];
	int32_t *ShotCount = &e->i[0], *Action = &e->i[1], *GunSlide = &e->i[3];
	int i;

	if (e->Handlung != GEGNER_INIT && e->Handlung != GEGNER_EXPLODIEREN)
	{
		if (e->Handlung != GEGNER_AUSFAHREN) b2test_hurt(e);
		boss_bar(e->Energy, FX(50000));
	}

	/* start the boss and its music */
	if (e->Active && tile_zustand() == TS_SCROLLBAR)
	{
		music_boss();
		*ySave = e->yPos;
		/* comes down in the middle of the screen */
		e->xPos = cam_xoff() + FX(RENDERWIDTH - 140) / 2;
		e->yPos = cam_yoff() - FX(300);
		e->ySpeed = FX(50);
		e->i[2] = 1;
		scroll_level(cam_xoff(), cam_yoff(), TS_LOCKED);
	}

	/* the gun slides back */
	if (*GunSlide > 0) *GunSlide -= SYNC(FX(1));
	else *GunSlide = 0;

	switch (e->Handlung)
	{
	case GEGNER_INIT:              /* falling in */
		if (e->yPos > *ySave)
		{
			e->yPos = *ySave;
			shake_screen(3);
			e->Handlung = GEGNER_INIT2;
			e->ySpeed = 0;
			e->AnimCount = FX(20);
			for (i = 0; i < 30; i++)
				part_push(e->xPos + FX(GetRandom(180) - 30), e->yPos + FX(GetRandom(40) + 70), SNOWFLUSH);
			for (i = 0; i < 10; i++)
				part_push(e->xPos + FX(GetRandom(130) - 10), e->yPos + FX(GetRandom(40) + 60), SMOKEBIG);
			scroll_level(cam_xoff(), e->yPos - FX(320), TS_SCROLLTOLOCK);
			snd(SOUND_DOORSTOP);
		}
		break;

	case GEGNER_INIT2:             /* a short wait while the screen scrolls up */
		e->AnimCount -= SYNC(FX(1));
		if (e->AnimCount < 0)
		{
			e->Handlung = GEGNER_EINFLIEGEN;
			*Action = GEGNER_EINFLIEGEN;
			e->AnimEnde = 12;
			e->AnimSpeed = FX(1);
			snd(SOUND_STEAM);
		}
		break;

	case GEGNER_EINFLIEGEN:        /* the leg out */
		switch (*Action)
		{
		case GEGNER_EINFLIEGEN:
			SimpleAnimation(e, 0);
			*yOffset = -FX(e->AnimPhase) * 8;
			if (*yOffset < FX(-40)) *yOffset = FX(-40);
			if (e->AnimPhase >= e->AnimEnde - 1)
			{
				*yOffset = 0;
				e->yPos -= FX(40);
				e->AnimPhase = 10;
				GegnerRect[e->GegnerArt].b = 150;
				schneekoenig_next_action(e, 0);
			}
			break;
		case GEGNER_INIT2:
			*KnarreY -= SYNC(FX(5));
			if (*KnarreY < 0)
			{
				*KnarreY = 0;
				*Action = -1;
				e->Handlung = GEGNER_VERFOLGEN;
				*ShotDelay = FX(1);
				*ShotCount = 10 + GetRandom(6);
			}
			break;
		case GEGNER_INIT3:
		case GEGNER_INIT4:
			*KnarreY -= SYNC(FX(5));
			if (*KnarreY < 0)
			{
				*KnarreY = 0;
				*Action = *Action == GEGNER_INIT3 ? GEGNER_STEHEN : GEGNER_BOMBARDIEREN;
				e->Handlung = GEGNER_SPRINGEN;
				e->AnimPhase = 10;
				e->AnimCount = FX(1);
				*ShotCount = 6;
				if (*Action == GEGNER_BOMBARDIEREN) *ShotDelay = FX(1.5);
				snd(SOUND_STEAM);
				if (pl.AufPlattform == e) pl.AufPlattform = 0;
			}
			break;
		}
		break;

	case GEGNER_SPRINGEN:
		/* the gun back in */
		if (*KnarreY < FX(60)) *KnarreY += SYNC(FX(5));
		else
		{
			*KnarreY = FX(60);
			switch (*Action)
			{
			case GEGNER_STEHEN:        /* take off */
			case GEGNER_BOMBARDIEREN:
				e->AnimCount -= SYNC(FX(4));
				if (e->AnimCount <= 0)
				{
					e->AnimCount = FX(1);
					e->AnimPhase--;
					if (e->AnimPhase < 5)
					{
						if (*Action == GEGNER_STEHEN)        /* onto the player */
						{
							*Action = GEGNER_SPRINGEN;
							e->ySpeed = FX(-80);
							e->yAcc = FX(18);
							e->xSpeed = FX(PlayerAbstandHoriz(e)) / 8;
							if (e->xPos > pl.xpos) e->xSpeed = -e->xSpeed;
						}
						else                                  /* to the side */
						{
							fx target;
							if (e->xPos + FX(100) > cam_xoff() + FX(320)) target = cam_xoff() + FX(100);
							else target = cam_xoff() + FX(540);
							if (*ShotCount == 1) target = cam_xoff() + FX(320);   /* back to the middle */
							*Action = GEGNER_AUSSPUCKEN;
							e->ySpeed = FX(-110);
							e->yAcc = FX(16);
							e->xSpeed = (target - (e->xPos + FX(100))) / 12;
						}
					}
				}
				break;

			case GEGNER_SCHLIESSEN:    /* landing */
			case GEGNER_AUSSPUCKENZWEI:
				e->AnimCount -= SYNC(FX(4));
				if (e->AnimCount <= 0)
				{
					e->AnimCount = FX(1);
					e->AnimPhase++;
					if (e->AnimPhase > 10)
					{
						e->AnimPhase = 10;
						*Action = *Action == GEGNER_SCHLIESSEN ? GEGNER_STEHEN : GEGNER_BOMBARDIEREN;
						(*ShotCount)--;
						if (*ShotCount <= 0)
						{
							e->Handlung = GEGNER_EINFLIEGEN;
							schneekoenig_next_action(e, -1);
						}
					}
				}
				break;

			case GEGNER_SPRINGEN:
			case GEGNER_AUSSPUCKEN:
				if (*Action == GEGNER_AUSSPUCKEN && *ShotCount > 1)
				{
					*ShotDelay -= SYNC(FX(1));
					if (*ShotDelay <= 0)
					{
						*ShotDelay = FX(2);
						proj_push(e->xPos + FX(100), e->yPos + FX(140), BLUEBOMB);
					}
				}
				if (e->ySpeed > 0 && e->yPos > *ySave - FX(40))   /* down on the ground */
				{
					for (i = 0; i < 30; i++)
						part_push(e->xPos + FX(GetRandom(140) + 10), e->yPos + FX(GetRandom(40) + 110), SNOWFLUSH);
					e->yPos = *ySave - FX(40);
					shake_screen(3);
					e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
					*Action = *Action == GEGNER_SPRINGEN ? GEGNER_SCHLIESSEN : GEGNER_AUSSPUCKENZWEI;
					/* the player crushed? */
					TestDamagePlayers(e, FX(500), 0);
					snd(SOUND_DOORSTOP);
				}
				break;
			}
		}
		break;

	case GEGNER_AUSFAHREN:         /* beaten: flies off */
		if (*KnarreWinkel < 0) *KnarreWinkel += SYNC(FX(5));
		if (*KnarreWinkel > 0) *KnarreWinkel -= SYNC(FX(5));
		if (*KnarreY < FX(60)) *KnarreY += SYNC(FX(5));
		else
		{
			*KnarreY = FX(60);
			e->AnimCount -= SYNC(FX(1));
			if (e->AnimCount < 0 && e->yAcc == 0)
			{
				e->ySpeed = FX(-5);
				e->yAcc = FX(-0.6);
			}
			if (e->yPos < cam_yoff() - FX(300)) e->Energy = 0;
			/* the third time it blows up half way */
			if (e->Value1 >= 2 && e->yPos < cam_yoff() + FX(150))
			{
				snd(SOUND_EXPLOSION2);
				shake_screen(5);
				for (i = 0; i < 50; i++)
					part_push(e->xPos + FX(GetRandom(150) - 20), e->yPos + FX(GetRandom(70)), SMOKEBIG);
				for (i = 0; i < 20; i++)
					part_push(e->xPos + FX(GetRandom(200) - 40), e->yPos + FX(GetRandom(100)), EXPLOSION_MEDIUM2);
				for (i = 0; i < 20; i++)
					part_push(e->xPos + FX(GetRandom(150)), e->yPos + FX(GetRandom(70)), SCHROTT1);
				for (i = 0; i < 5; i++)
					part_push(e->xPos + FX(GetRandom(180) - 40), e->yPos + FX(GetRandom(100)), EXPLOSION_BIG);
				for (i = 0; i < 5; i++)
					part_push(e->xPos + FX(GetRandom(200)), e->yPos + FX(GetRandom(20) + 70), SPLITTER);
				e->Energy = 0;
			}
			/* let off steam =) */
			*SmokeDelay -= SYNC(FX(1));
			if (*SmokeDelay < 0)
			{
				*SmokeDelay = FX(0.5);
				part_push(e->xPos + FX(60) + FX(GetRandom(20)), e->yPos + FX(120) + *yOffset, SMOKEBIG);
				part_push(e->xPos + FX(95) + FX(GetRandom(20)), e->yPos + FX(140) + *yOffset, FUNKE);
				proj_push(e->xPos + FX(75) + FX(GetRandom(4)), e->yPos + FX(80) + *yOffset, FEUERFALLE3);
				if (e->Value1 >= 2 && GetRandom(2) == 0)
				{
					snd(SOUND_EXPLOSION1);
					part_push(e->xPos + FX(GetRandom(150)), e->yPos + FX(GetRandom(100)), EXPLOSION_MEDIUM2);
				}
			}
			int a = FINT((e->yPos - cam_yoff() - FX(200)) / 10);
			if (a < 0) a = 0;
			if (a > 10) a = 10;
			e->AnimPhase = (int16_t)a;
		}
		break;

	case GEGNER_VERFOLGEN:         /* shoot at the player */
	{
		const fx aim = schneekoenig_winkel_to_player(e);
		if (*KnarreWinkel < aim)
		{
			*KnarreWinkel += SYNC(FX(8));
			if (*KnarreWinkel > aim) *KnarreWinkel = aim;
		}
		if (*KnarreWinkel > aim)
		{
			*KnarreWinkel -= SYNC(FX(8));
			if (*KnarreWinkel < aim) *KnarreWinkel = aim;
		}
		if (PlayerAbstandHoriz(e) < 100 && e->Value1 > 0) (*ShotCount)--;
		else if (abs(FINT(aim - *KnarreWinkel)) < 10) *ShotDelay -= SYNC(FX(1));
		if (*ShotDelay <= 0)
		{
			*ShotDelay = FX(8);
			const fx w = *KnarreWinkel <= 0 ? FX(180) - *KnarreWinkel : *KnarreWinkel - FX(180);
			const int d = FINT(*KnarreWinkel + FX(180));
			for (int p = 0; p < 2; p++)
				part_push(e->xPos + 70 * sin_d(d) + FX(80), e->yPos + 60 * cos_d(d) + *KnarreY + *yOffset, SMOKEBIG);
			gegner_push(e->xPos + 70 * sin_d(d) + FX(100), e->yPos + 60 * cos_d(d) + *KnarreY + FX(5) + *yOffset,
			            SCHNEEKOPPE, -FINT(w) - 3 + GetRandom(7), 40);
			snd(SOUND_GRANATE);
			*GunSlide = FX(5);
			if (e->Value1 > 0) (*ShotCount)--;   /* not the first time: count the shots */
		}
		if (*ShotCount <= 0) schneekoenig_next_action(e, -1);
	}
	break;
	}

	/* hurt? */
	if (e->DamageTaken > 0)
	{
		if (!(*LastEnergy - e->Energy > FX(5000))) e->DamageTaken = 0;
		*LastEnergy = e->Energy;
	}

	GegnerRect[e->GegnerArt].t = (int16_t)(60 + FINT(*yOffset));

	if (e->Handlung != GEGNER_SPRINGEN && e->Handlung != GEGNER_AUSFAHREN)
	{
		Wegschieben(e, &GegnerRect[e->GegnerArt], 0);
		PlattformTest(e, &GegnerRect[e->GegnerArt]);
	}

	/* hit by a snowball of its own? =) */
	if (e->Handlung != GEGNER_AUSFAHREN)
		for (int k = 0; k < MAX_LIVE; k++)
		{
			Ent *o = &ents[k];
			if (o->GegnerArt != SCHNEEKOPPE) continue;
			if (SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], o->xPos, o->yPos, &GegnerRect[SCHNEEKOPPE]))
			{
				o->Energy = 0;
				e->Energy -= FX(5000);
				if (e->Handlung != GEGNER_SPRINGEN) *WackelAnim = FX(8);
			}
		}

	/* beaten? then off it goes */
	if (e->Energy < FX(100) && e->Handlung != GEGNER_AUSFAHREN)
	{
		if (pl.AufPlattform == e) pl.AufPlattform = 0;
		e->Energy = FX(100);
		e->Destroyable = 0;
		e->Handlung = GEGNER_AUSFAHREN;
		e->AnimCount = FX(10);
		e->xSpeed = e->ySpeed = e->xAcc = e->yAcc = 0;
		snd(SOUND_TAKEOFF);
		music_stop();
	}

	/* (Hurrican's DoDraw) the shake and the hit animation */
	if (e->i[2])
	{
		e->blick = -1;
		if (e->Handlung == GEGNER_AUSFAHREN)
		{
			schneekoenig_xoff += SYNC(FX(15));
			while (schneekoenig_xoff > FX(6.2831853)) schneekoenig_xoff -= FX(6.2831853);
		}
		if (*WackelAnim > 0)
		{
			e->AnimPhase = (int16_t)(10 - schneekoenig_damage_anims[FINT(*WackelAnim) % 4]);
			*WackelAnim -= SYNC(FX(2));
			if (*WackelAnim < 0) e->AnimPhase = 10;
		}
	}
}

static void schneekoenig_explode(Ent *e)
{
	music_stop();
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	pl.Score += 2000 * e->Value1;
	/* ScrolltoPlayeAfterBoss */
	scroll_level(cam_xoff(), cam_yoff(), TS_SCROLLTOPLAYER);
	music_stage();
	gegner_default_explode(e);
}

static void schneekoenig_load(void)
{
	schneekoenig_knarre = pic("schneekoenig_knarre.png");
}

TYPEDEF(SCHNEEKOENIG) = { schneekoenig_init, schneekoenig_ki, schneekoenig_explode, schneekoenig_draw, schneekoenig_load };
