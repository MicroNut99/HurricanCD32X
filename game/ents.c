/*
 * Hurrican CD32X - the objects of a stage (Hurrican's GegnerClass / GegnerListClass, with the
 * map's objects): the base every enemy, trigger and boss shares. Included by game/p1.c.
 *
 * Hurrican creates every object of the map when the level loads and runs each one once it has
 * come onto the screen (Active). Here the map's objects (OBJECTS.BIN in the stage pack) wait in
 * the cart and are made (their constructor runs) when the screen comes near them; they then
 * behave as Hurrican's: inactive until on the screen, then running every step. Objects far
 * away (two screens and more) go back to waiting - those from the map come again, as new, when
 * the screen comes back; the others are gone. Killed ones stay killed.
 */

/* the table of object types (game/types.c) */
static const TypeDef *const gegner_types[200];

/* ---- the map's objects */
#define MAX_OBJS 1300
static const uint8_t *objs;          /* OBJECTS.BIN entries: id, x, y, v1, v2, skill (12 bytes) */
static int nobjs;
static uint8_t obj_state[MAX_OBJS];  /* 0 waiting, 1 in the game, 2 gone */
#define OBJ_ID(i)   rd16(objs + 12 * (i))
#define OBJ_X(i)    ((int16_t)rd16(objs + 12 * (i) + 2))
#define OBJ_Y(i)    ((int16_t)rd16(objs + 12 * (i) + 4))
#define OBJ_V1(i)   ((int16_t)rd16(objs + 12 * (i) + 6))
#define OBJ_V2(i)   ((int16_t)rd16(objs + 12 * (i) + 8))
#define OBJ_SKILL(i) rd16(objs + 12 * (i) + 10)
#define SKILL 1                      /* medium */

static Ent ent_scratch;              /* gegner_push when the list is full: a harmless dummy */

static int gegner_known(int art)
{
	return art >= 0 && art < 200 && gegner_types[art] != 0;
}

/* a new object (PushGegner): the constructor, then the position (as Hurrican: the
 * constructor sees xPos = yPos = 0) */
static Ent *gegner_make(fx x, fx y, int art, int v1, int v2, int obj)
{
	if (!gegner_known(art)) { memset(&ent_scratch, 0, sizeof ent_scratch); ent_scratch.GegnerArt = -1; return &ent_scratch; }
	Ent *e = 0;
	for (int i = 0; i < MAX_LIVE; i++) if (ents[i].GegnerArt < 0) { e = &ents[i]; break; }
	if (!e) { memset(&ent_scratch, 0, sizeof ent_scratch); ent_scratch.GegnerArt = -1; return &ent_scratch; }
	memset(e, 0, sizeof *e);
	e->GegnerArt = (int16_t)art;
	e->obj = (int16_t)obj;
	e->Value1 = (int16_t)v1;
	e->Value2 = (int16_t)v2;
	e->blick = -1;
	e->Destroyable = 1;
	e->TestBlock = 1;
	e->LastAction = -1;
	e->TimeToChange = FX(50);
	if (art >= STAHLFAUST && art < PARTIKELSPAWN) e->NoDespawn = 1;   /* bosses */
	if (gegner_types[art]->init) gegner_types[art]->init(e);
	e->xPos = e->xPosOld = x;
	e->yPos = e->yPosOld = y;
	if (art == SHOOTPLATTFORM) gegner_push(x + FX(42), y - FX(9), SHOOTBUTTON, 0, 0);
	return e;
}

static Ent *gegner_push(fx x, fx y, int art, int value1, int value2)
{
	return gegner_make(x, y, art, value1, value2, -1);
}

static void gegner_free(Ent *e)
{
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
	e->GegnerArt = -1;
}

/* the map's objects near the screen into the game; far ones out again */
static void objects_update(void)
{
	const int x0 = FINT(hur_xoff) - 640, x1 = FINT(hur_xoff) + RENDERWIDTH + 640;
	const int y0 = FINT(hur_yoff) - 480, y1 = FINT(hur_yoff) + RENDERHEIGHT + 480;
	/* the objects are sorted by x: find the first at x0 */
	int lo = 0, hi = nobjs;
	while (lo < hi) { const int m = (lo + hi) / 2; if (OBJ_X(m) < x0) lo = m + 1; else hi = m; }
	for (int i = lo; i < nobjs && OBJ_X(i) <= x1; i++)
	{
		if (obj_state[i] != 0) continue;
		const int y = OBJ_Y(i);
		if (y < y0 || y > y1) continue;
		const int art = OBJ_ID(i);
		/* object 0 on the map is the player's start, not an extra */
		if (art == 0 || !gegner_known(art) || OBJ_SKILL(i) > SKILL) { obj_state[i] = 2; continue; }
		Ent *e = gegner_make(FX(OBJ_X(i)), FX(y), art, OBJ_V1(i), OBJ_V2(i), i);
		if (e->GegnerArt < 0) break;                /* the list is full */
		obj_state[i] = 1;
	}
	/* far ones out */
	const int fx0 = FINT(hur_xoff) - 1400, fx1 = FINT(hur_xoff) + RENDERWIDTH + 1400;
	const int fy0 = FINT(hur_yoff) - 1100, fy1 = FINT(hur_yoff) + RENDERHEIGHT + 1100;
	for (int k = 0; k < MAX_LIVE; k++)
	{
		Ent *e = &ents[k];
		if (e->GegnerArt < 0 || e->NoDespawn || pl.AufPlattform == e) continue;
		const int x = FINT(e->xPos), y = FINT(e->yPos);
		if (x >= fx0 && x <= fx1 && y >= fy0 && y <= fy1) continue;
		if (e->obj >= 0 && obj_state[e->obj] == 1) obj_state[e->obj] = 0;
		gegner_free(e);
	}
}

/* ---- GegnerClass's own functions */
static int IsOnScreen(const Ent *e)
{
	const int art = e->GegnerArt;
	int off = GegnerRect[art].l < 0 ? GegnerRect[art].l : 0;
	int xs = GegnerPic[art].fw * 2, ys = GegnerPic[art].fh * 2;
	if (art == DRACHE || art == THEWALL) { xs = 400; off = -400; }
	const fx x = e->xPos, y = e->yPos;
	return !(x + FX(xs) < hur_xoff || x + FX(off) > hur_xoff + FX(RENDERWIDTH) ||
	         y + FX(ys) < hur_yoff || y > hur_yoff + FX(RENDERHEIGHT));
}

static int PlayerAbstand(const Ent *e)
{
	const Box *r = &GegnerRect[e->GegnerArt];
	const int dx = FINT(pl.xpos) + 35 - (FINT(e->xPos) + r->r / 2);
	const int dy = FINT(pl.ypos) + 40 - (FINT(e->yPos) + r->b / 2);
	return isqrt(dx * dx + dy * dy);
}
static int PlayerAbstandHoriz(const Ent *e)
{
	const Box *r = &GegnerRect[e->GegnerArt];
	const int d = (FINT(pl.xpos) + pl.CollideRect.l + (pl.CollideRect.r - pl.CollideRect.l) / 2) -
	              (FINT(e->xPos) + r->l + (r->r - r->l) / 2);
	return abs(d);
}
static int PlayerAbstandVert(const Ent *e)
{
	const Box *r = &GegnerRect[e->GegnerArt];
	const int d = (FINT(pl.ypos) + pl.CollideRect.t + (pl.CollideRect.b - pl.CollideRect.t) / 2) -
	              (FINT(e->yPos) + r->b / 2);
	return abs(d);
}

static void TestDamagePlayers(Ent *e, fx dam, int destroy)
{
	if (pl.Handlung == PA_TOT || pl.Handlung == PA_EXPLODIEREN) return;
	if (SpriteCollision(e->xPos, e->yPos, &GegnerRect[e->GegnerArt], pl.xpos, pl.ypos, &pl.CollideRect))
	{
		DamagePlayer(dam, 0);
		if (destroy) e->Energy = 0;
	}
}

static void PlattformTest(Ent *e, const Box *rect)
{
	if (pl.Handlung == PA_TOT || pl.Handlung == PA_EXPLODIEREN) return;
	if (pl.AufPlattform == e)
	{
		/* moved as the platform moves, if no wall is in the way */
		fx x = pl.xpos, y = pl.ypos;
		const int br = BlockRechts(&x, y, &x, y, &pl.CollideRect, 0);
		x = pl.xpos;
		const int bl = BlockLinks(&x, y, &x, y, &pl.CollideRect, 0);
		if ((e->xSpeed > 0 && !(br & B_WAND)) || (e->xSpeed < 0 && !(bl & B_WAND))) pl.xpos += SYNC(e->xSpeed);
		pl.ypos = e->yPos - FX(pl.CollideRect.b) + FX(GegnerRect[e->GegnerArt].t) + SYNC(e->ySpeed);
		if (FINT(pl.xpos) + pl.CollideRect.l > FINT(e->xPos) + rect->r ||
		    FINT(pl.xpos) + pl.CollideRect.r < FINT(e->xPos) + rect->l)
			pl.AufPlattform = 0;
	}
	else if (pl.AufPlattform == 0 && pl.Handlung != PA_SACKREITEN && pl.Handlung != PA_DREHEN)
	{
		if (pl.yspeed >= 0)
		{
			const int laenge = abs(FINT(pl.ypos - pl.yposold)) + 2;
			const int yo = FINT(pl.yposold) + pl.CollideRect.b + 1, top = FINT(e->yPos) + rect->t;
			const int px = FINT(pl.xpos), ex = FINT(e->xPos);
			for (int i = 0; i < laenge; i++)
				if (yo + i >= top && yo + i <= top + 10 &&
				    px + pl.CollideRect.l <= ex + rect->r && px + pl.CollideRect.r >= ex + rect->l)
				{
					pl.AufPlattform = e;
					break;
				}
		}
	}
}

static void Wegschieben(Ent *e, const Box *rect, fx dam)
{
	if (pl.Handlung == PA_TOT || pl.Handlung == PA_EXPLODIEREN) return;
	if (!SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect) || pl.AufPlattform == e) return;
	if (pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL)
	{
		if (pl.xpos < e->xPos) pl.Blickrichtung = -1;
		if (pl.xpos > e->xPos) pl.Blickrichtung = 1;
		return;
	}
	if (dam > 0) DamagePlayer(SYNC(dam), 0);
	const fx mid = e->xPos + FX(rect->l + (rect->r - rect->l) / 2);
	fx x = pl.xpos;
	if (pl.xpos + FX(35) < mid && !(BlockLinks(&x, pl.ypos, &x, pl.ypos, &pl.CollideRect, 0) & B_WAND))
		pl.xpos -= SYNC(FX(20) + FX(1));
	x = pl.xpos;
	if (pl.xpos + FX(35) >= mid && !(BlockRechts(&x, pl.ypos, &x, pl.ypos, &pl.CollideRect, 0) & B_WAND))
		pl.xpos += SYNC(FX(20) + FX(1));
}

static void SimpleAnimation(Ent *e, int backward)
{
	if (e->AnimEnde <= 0) return;
	e->AnimCount += SPEEDFACTOR;
	if (e->AnimCount > e->AnimSpeed)
	{
		e->AnimCount = 0;
		if (backward) { if (--e->AnimPhase <= e->AnimStart) e->AnimPhase = e->AnimEnde; }
		else if (++e->AnimPhase >= e->AnimEnde) e->AnimPhase = e->AnimStart;
	}
}

static void TurnonWall(Ent *e)
{
	if ((((e->blockl & B_WAND) || (e->blockl & B_GEGNERWAND)) && e->xSpeed < 0) ||
	    (((e->blockr & B_WAND) || (e->blockr & B_GEGNERWAND)) && e->xSpeed > 0))
	{
		e->xSpeed = -e->xSpeed;
		e->blick = (int8_t)-e->blick;
	}
}

static int TurnonShot(Ent *e)
{
	if (e->TurnCount > 0) e->TurnCount -= SYNC(FX(1));
	if (e->DamageTaken > 0 && e->TurnCount <= 0)
	{
		e->TurnCount = FX(20);
		if ((pl.xpos < e->xPos && e->blick == 1) || (pl.xpos > e->xPos && e->blick == -1))
		{
			e->blick = (int8_t)-e->blick;
			e->xSpeed = -e->xSpeed;
		}
		return 1;
	}
	return 0;
}

static void gegner_default_explode(Ent *e)
{
	if (pl.AufPlattform == e) pl.AufPlattform = 0;
}

static void gegner_default_draw(Ent *e)
{
	draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, e->xPos, e->yPos, e->blick == 1);
}

static void DamageEnemiesonScreen(fx x, fx y, int maxdamage)
{
	for (int k = 0; k < MAX_LIVE; k++)
	{
		Ent *e = &ents[k];
		if (e->GegnerArt < 0 || !e->Active) continue;
		const Box *r = &GegnerRect[e->GegnerArt];
		const int ax = FINT(x - e->xPos), ay = FINT(y - e->yPos);
		const int dx = isqrt(ax * ax + ay * ay);
		const int on = e->xPos + FX(r->r) > hur_xoff && e->xPos + FX(r->l) < hur_xoff + FX(RENDERWIDTH) &&
		               e->yPos + FX(r->b) > hur_yoff && e->yPos + FX(r->t) < hur_yoff + FX(RENDERHEIGHT);
		if (e->GegnerArt == STAMPFSTEIN && e->Handlung == GEGNER_STEHEN && dx < 300 && on)
		{
			e->Handlung = GEGNER_FALLEN;
			e->ySpeed = FX(20);
			e->yAcc = FX(15);
			snd(SOUND_STONEFALL);
		}
		if (dx < 300 && e->Destroyable && e->GegnerArt != POWERBLOCK && e->GegnerArt < RIESENPIRANHA && on)
		{
			int amount = maxdamage - dx;
			if (amount < 0) amount = 0;
			e->Energy -= FX(amount);
		}
	}
}

/* ---- GegnerClass::Run: one 60 Hz step */
static void gegner_run(Ent *e)
{
	const int art = e->GegnerArt;
	const Box *rect = &GegnerRect[art];
	if (e->Active)
	{
		if (gegner_types[art]->ki) gegner_types[art]->ki(e);
		if (e->GegnerArt < 0) return;                 /* removed itself */
		if (e->DontMove) return;
		if (e->AnimStart < 0 || e->AnimPhase < 0) e->AnimStart = 0;
		if (e->TimeToChange > 0) e->TimeToChange -= SYNC(FX(1));
		if (e->TestBlock)
		{
			e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
			e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
			e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
			e->blocku = (uint16_t)BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect);
		}
		else e->blockl = e->blockr = e->blocko = e->blocku = 0;
		e->xSpeed += SYNC(e->xAcc);
		e->ySpeed += SYNC(e->yAcc);
		if ((e->xSpeed < 0 && !(e->blockl & B_WAND)) || (e->xSpeed > 0 && !(e->blockr & B_WAND))) e->xPos += SYNC(e->xSpeed);
		if ((e->ySpeed < 0 && !(e->blocko & B_WAND)) || (e->ySpeed > 0 && !(e->blocku & B_WAND))) e->yPos += SYNC(e->ySpeed);
		if (art != FAHRSTUHL &&
		    (e->xPos + FX(rect->r) < 0 || e->yPos + FX(rect->b) < 0 ||
		     e->xPos > FX(map_w * TILESIZE) || e->yPos > FX(map_h * TILESIZE)))
			e->Energy = 0;
		/* the player as a wheel grinds enemies */
		if ((pl.Handlung == PA_RADELN || pl.Handlung == PA_RADELN_FALL) && e->Destroyable &&
		    SpriteCollision(e->xPos, e->yPos, rect, pl.xpos, pl.ypos, &pl.CollideRect))
		{
			e->Energy -= SYNC(FX(10));
			if (e->DamageTaken <= 0) e->DamageTaken = FX(255);
			if (!pl.WheelMode) pl.Armour -= SYNC(FX(3));
			snd(SOUND_HIT + e->HitSound);
		}
	}
	else if (e->yPos - hur_yoff < FX(RENDERHEIGHT) && e->yPos - hur_yoff > FX(-rect->b) &&
	         e->xPos - hur_xoff < FX(RENDERWIDTH) && e->xPos - hur_xoff > FX(-rect->r))
	{
		e->Active = 1;
		const fx mid = e->xPos + FX(rect->l + (rect->r - rect->l) / 2);
		e->blick = (int8_t)(pl.xpos + FX(35) < mid ? -1 : 1);
		if (art != EXTRAS) { e->xSpeed *= e->blick; e->xAcc *= e->blick; }
		if (e->TestBlock)
		{
			e->blockl = (uint16_t)BlockLinks(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
			e->blockr = (uint16_t)BlockRechts(&e->xPos, e->yPos, &e->xPosOld, e->yPosOld, rect, 0);
			e->blocko = (uint16_t)BlockOben(e->xPos, &e->yPos, e->xPosOld, &e->yPosOld, rect, 0);
			e->blocku = (uint16_t)BlockUntenNormal(e->xPos, e->yPos, e->xPosOld, e->yPosOld, rect);
		}
	}
	e->xPosOld = e->xPos;
	e->yPosOld = e->yPos;
	/* the hit flash fades (Render: 250 a time unit, the power block 75) */
	if (e->DamageTaken > 0)
	{
		e->DamageTaken -= SYNC(art == POWERBLOCK ? FX(75) : FX(250));
		if (e->DamageTaken < 0) e->DamageTaken = 0;
	}
}

/* RunAll: every object one step; the dead explode and go */
static void gegner_step_all(void)
{
	for (int k = 0; k < MAX_LIVE; k++)
	{
		Ent *e = &ents[k];
		if (e->GegnerArt < 0) continue;
		gegner_run(e);
		if (e->GegnerArt >= 0 && e->Energy <= 0)
		{
			const int art = e->GegnerArt;
			if (gegner_types[art]->explode) gegner_types[art]->explode(e);
			else gegner_default_explode(e);
			if (e->obj >= 0) obj_state[e->obj] = 2;
			gegner_free(e);
		}
	}
}

/* RenderAll: the background ones first */
static void gegner_draw_all(void)
{
	for (int pass = 0; pass < 2; pass++)
		for (int k = 0; k < MAX_LIVE; k++)
		{
			Ent *e = &ents[k];
			if (e->GegnerArt < 0 || !e->Active || (e->BackGround != 0) != (pass == 0)) continue;
			if (e->Handlung == GEGNER_NOTVISIBLE || !IsOnScreen(e)) continue;
			draw_flash = e->DamageTaken > 0;
			if (e->OwnDraw) { if (gegner_types[e->GegnerArt]->draw) gegner_types[e->GegnerArt]->draw(e); }
			else if (!e->DontMove) gegner_default_draw(e);
			draw_flash = 0;
		}
}

static void gegner_clear(void)
{
	for (int k = 0; k < MAX_LIVE; k++) ents[k].GegnerArt = -1;
	memset(obj_state, 0, sizeof obj_state);
}

/* after a stage pack is mounted: the objects, their pictures and rectangles */
static void gegner_load(void)
{
	const uint8_t *o;
	uint32_t size;
	if (!pack_file("OBJECTS.BIN", &o, &size)) plat_fatal("STAGE PACK: NO OBJECTS.BIN");
	nobjs = (int)rd32(o + 4);
	if (nobjs > MAX_OBJS) nobjs = MAX_OBJS;
	objs = o + 8;
	for (int i = 0; i < 200; i++)
	{
		GegnerRect[i] = (Box){ gegner_rect_init[i][0], gegner_rect_init[i][1], gegner_rect_init[i][2], gegner_rect_init[i][3] };
		GegnerPic[i] = gegner_pic_name[i] ? pic(gegner_pic_name[i]) : (Pic){ 0, 0, 0, 0, 0 };
	}
	DroneFlame = pic("droneflame.png");
	DroneGun = pic("Zitronestiel.png");
	for (int i = 0; i < 200; i++)
		if (gegner_types[i] && gegner_types[i]->load) gegner_types[i]->load();
}

/* the player's start (object 0 with value2 0) */
static int gegner_player_start(fx *x, fx *y, int *blick)
{
	for (int i = 0; i < nobjs; i++)
		if (OBJ_ID(i) == 0 && OBJ_V2(i) == 0)
		{
			*x = FX(OBJ_X(i)); *y = FX(OBJ_Y(i));
			*blick = OBJ_V1(i) == 0 ? 1 : -1;
			return 1;
		}
	return 0;
}
