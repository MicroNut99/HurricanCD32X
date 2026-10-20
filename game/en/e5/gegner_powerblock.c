/*
 * POWERBLOCK - the bonus block (enemies/Gegner_PowerBlock.cpp): invisible until hit, then
 * every time its energy runs out (or the player jumps against it from below) an extra jumps
 * out; after Value1 (always 4, hardcoded in Hurrican) extras it breaks.
 * Value2 = ExtraType (0: random, the first one always energy).
 * Own members: i[0] = ExtraType, i[1] = AlreadySpawned (a bit per extra), i[2] = WeaponSpawned.
 * The frames by the map's power block type (UsedPowerBlock).
 * Simplified: Hurrican fades a bright frame over the block after a hit (AnimPhase 2, alpha
 * DamageTaken); here that frame is drawn instead of the normal one while DamageTaken >= 128.
 * Hurrican's BlocksFullGame (a statistic) has no field here; BlocksThisLevel is counted.
 */
static const int16_t powerblock_WaffenVerteilung[14] = { 100, 200, 300, 500, 505, 530, 550, 600, 650, 675, 680, 685, 1000, 1000 };

static void powerblock_init(Ent *e)
{
	e->i[0] = e->Value2;
	e->i[2] = 1;                         /* WeaponSpawned(NUMPLAYERS) */
	e->Handlung = GEGNER_STEHEN;
	e->AnimPhase = 0;
	e->Energy = FX(100);
	e->i[1] = 0;
	e->Value1 = 4;                       /* (hardcoded in Hurrican) */
	e->Destroyable = 1;
	e->OwnDraw = 1;
}

static void powerblock_draw(Ent *e)
{
	if (e->AnimPhase == 0) return;
	if (e->AnimPhase == 2 && e->DamageTaken >= FX(128))
		draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase + UsedPowerBlock * 2, e->xPos, e->yPos, 0);
	else
		draw_sprite(&GegnerPic[e->GegnerArt], 1 + UsedPowerBlock * 2, e->xPos, e->yPos, 0);
}

static void powerblock_ki(Ent *e)
{
	E5_TEST_HOOK(e);
	const Box *r = &GegnerRect[e->GegnerArt];
	if (e->AnimPhase == 0) e->Value2 = (int16_t)FINT(e->yPos);
	if (e->AnimPhase > 0) PlattformTest(e, r);

	/* hit: it appears */
	if (e->DamageTaken != 0 && e->AnimPhase == 0)
	{
		e->AnimPhase = 1;
		pl.BlocksThisLevel++;
		e->Energy = FX(12);
	}
	if (e->AnimPhase == 2 && e->DamageTaken == 0) e->AnimPhase = 1;

	/* the player jumped against it from below */
	if (e->AnimPhase == 1 && pl.yspeed < 0 &&
	    FINT(pl.ypos) + pl.CollideRect.t <= FINT(e->yPos) + r->b &&
	    FINT(pl.ypos) + pl.CollideRect.t >= FINT(e->yPos) &&
	    FINT(pl.xpos) + pl.CollideRect.l <= FINT(e->xPos) + r->r &&
	    FINT(pl.xpos) + pl.CollideRect.r >= FINT(e->xPos))
	{
		e->Energy = 0;
		e->AnimPhase = 2;
		e->Handlung = GEGNER_SPRINGEN;
		e->yAcc = FX(4);
		if (e->ySpeed == 0) e->ySpeed = FX(-8);
		else if (e->ySpeed > 0) e->ySpeed = -e->ySpeed;
		pl.yspeed = -pl.yspeed * 2 / 3;
	}

	/* back down after jumping */
	if (e->ySpeed != 0 && e->yPos > FX(e->Value2))
	{
		e->yPos = FX(e->Value2);
		e->ySpeed = 0;
		e->yAcc = 0;
		e->Handlung = GEGNER_STEHEN;
	}

	if (e->Energy <= 0)                 /* an extra out */
	{
		e->AnimPhase = 2;
		pl.Score += 10;
		int extra;
		if (e->i[0] != 0) extra = e->i[0];
		else
		{
			extra = 5;                       /* the first one always energy */
			if (e->Value1 != 4)
			{
				while (e->i[1] & (1 << extra))
				{
					int rr;
					if (e->i[2] > 0) rr = GetRandom(675);
					else rr = GetRandom(175) + 500;
					for (int i = 0; i < 12; i++)
						if (rr < powerblock_WaffenVerteilung[i])
						{
							if (i <= 2) e->i[2]--;
							extra = i;
							break;
						}
				}
			}
			e->i[1] |= 1 << extra;
		}
		gegner_push(e->xPos + FX(10), e->yPos, EXTRAS, extra, 0);
		e->Value1--;
		e->Energy = e->Value1 > 0 ? FX(12) : 0;
	}
}

static void powerblock_explode(Ent *e)
{
	if (pl.AufPlattform == e)
	{
		pl.AufPlattform = 0;
		pl.JumpPossible = 0;
		pl.Handlung = PA_SPRINGEN;
		pl.AnimPhase = 0;
		pl.yspeed = FX(0.5);
		pl.JumpAdd = FX(12.5);               /* PLAYER_JUMPADDSPEED */
	}
	pl.Score += 100;
	part_push(e->xPos - FX(30), e->yPos - FX(30), EXPLOSION_BIG);
	snd(SOUND_EXPLOSION1);
}

TYPEDEF(POWERBLOCK) = { powerblock_init, powerblock_ki, powerblock_explode, powerblock_draw, 0 };
