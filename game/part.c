/*
 * Hurrican CD32X - the particles (Partikelsystem.cpp / .hpp: PartikelClass::CreatePartikel, Run,
 * Render; PartikelsystemClass::LoadSprites, PushPartikel, ClearAll). Included by game/p1.c in
 * place of the first version game/part.c (PART_C).
 *
 * FEWER AND SMALLER than the PC game (the 32X has little drawing time):
 *  - at most PART_MAX (40) particles at once. No particle affects gameplay in Hurrican (none
 *    hurts the player, none is counted by an object), so all are decorative; they come in two
 *    classes: "major" ones (explosions, the explosion spawners, the player's / bosses' flying
 *    parts) and "minor" ones (smoke, debris, splashes, blood). When the pool is full a new
 *    particle takes the slot of the oldest minor one; when there is none, a new major one takes
 *    the oldest major one and a new minor one is not made.
 *  - every particle is drawn as one plain keyed sprite: no rotation (the frame Hurrican would
 *    rotate is drawn upright), no scaling (scaled sprites - water splashes, the diamond flash -
 *    are drawn at their natural size, centred where Hurrican centres them), no colour tints, no
 *    alpha: a particle that fades out in Hurrican simply disappears when its life runs out.
 *    (A particle whose alpha would be 0 stops moving, as in Hurrican's Run.)
 *  - water / lava splashes (WASSER_SPRITZER, WASSER_SPRITZER2, LAVA_SPRITZER, LAVA_SPRITZER2):
 *    only every third one pushed is made (the player's jump into water pushes 12).
 *  - EXPLOSION_TRACE (the flying explosion spawner of the Rollmops / Spinnenmaschine bosses,
 *    the water mine): its own sprite is a glow and is not drawn; it spawns its
 *    EXPLOSION_REGULARs half as often (every 0.8 instead of 0.4).
 *  - EXPLOSION_BIG's ten SMOKEBIG2 come only at Hurrican's maximum detail: left out (detail
 *    "high"). The glows the explosions push (EXPLOSIONFLARE, EXPLOSION_KRINGEL) and
 *    EXPLOSION_GIGA's 20 FUNKE + 20 LONGFUNKE are dropped types (below).
 *  - Hurrican's Render draws a frame only while AnimPhase <= AnimEnde (a DKS guard); for the
 *    particles that pick a still frame at random (SMOKEBIG, GLASSPLITTER, PIRANHATEILE, the
 *    HURRITEILE ...) that hid every frame but 0 - here the frame is drawn while it exists in
 *    the picture.
 *  - EXPLOSION_MEDIUM2_ADD / EXPLOSION_MEDIUM3_ADD pushed directly (nobody does) start as
 *    EXPLOSION_MEDIUM2 / 3 (in Hurrican they would stand still on frame 0).
 *  - SPIDERPARTS: the engine's map has no conveyor-belt flags (BLOCKWERT_FLIESSBANDL / R), so
 *    the parts only fall, bounce off walls and lie still.
 *  - BlockUntenNormal for non-bouncing particles is only computed where it is used
 *    (SPIDERGRENADE).
 *  - Not available: the FahrstuhlBoss's moving of all particles (it reaches into
 *    PartikelSystem.particles; no such function in hur.h), SetTarget (TURBINESMOKE, dropped),
 *    ClearPowerUpEffects (KRINGEL, dropped), the thunder flash (SetThunderColor / DoThunder).
 *
 * KEPT (pushed with part_push, moved and drawn):
 *   explosions: EXPLOSION_MEDIUM, EXPLOSION_MEDIUM2, EXPLOSION_MEDIUM3, EXPLOSION_MEDIUM2_ADD,
 *     EXPLOSION_MEDIUM3_ADD, EXPLOSION_GREEN, EXPLOSION_BIG, EXPLOSION_GIANT, EXPLOSION_ALIEN,
 *     BLUE_EXPLOSION, EXPLOSION_GIGA, EXPLOSION_REGULAR, EXPLOSION_TRACE (spawner, not drawn),
 *     EXPLOSION_TRACE_END
 *   smoke: SMOKE, SMOKEBIG, SMOKEBIG2, SMOKEBIG_OUTTRO
 *   debris: GLASSPLITTER, PIRANHATEILE, PIRANHATEILE2, PIRANHABLUT, MADEBLUT, NESTLUFT,
 *     ROCKSPLITTER, ROCKSPLITTERSMALL, ROCKSPLITTERSMALLBLUE, BOULDER_SMALL, SPIDERSPLITTER,
 *     SPIDERSPLITTER2, SPIDERGRENADE, STELZE, STELZHEAD (spawns explosions), HALSWIRBEL,
 *     KAPUTTETURBINE, LAVAKRABBE_KOPF, LAVAKRABBE_BEIN, SPIDERPARTS, KETTENTEILE,
 *     KETTENTEILE2, KETTENTEILE3, KETTENTEILE4, SCHROTT1, SCHROTT2, HURRITEILE,
 *     HURRITEILE_ARM1 .. HURRITEILE_TORSO, HURRITEILE_P2, HURRITEILE_P2_ARM1 .. _P2_TORSO
 *   splashes: WASSER_SPRITZER, WASSER_SPRITZER2, LAVA_SPRITZER, LAVA_SPRITZER2,
 *     WATERFLUSH_HIGH, WATERFLUSH_HIGH2 (water mine / Rollmops splash spray)
 *   DIAMANTCOLLECTED (the diamond's flash, drawn at its natural size)
 *
 * DROPPED (part_push does nothing):
 *   tiny (a few pixels on the 32X): BULLET, BULLET_SKELETOR (shell casings), SPLITTER (and its
 *     smoke trail), FUNKE, FUNKE2, LASERFUNKE, LASERFUNKE2, LONGFUNKE, WATERFUNKE, EVILFUNKE,
 *     WASSERTROPFEN, WASSERTROPFEN2, SPAWNDROP, SCHLEIM, SCHLEIM2, DUST, LAVADUST,
 *     PHARAOSMOKE, ROCKETSMOKE, ROCKETSMOKEBLUE, ROCKETSMOKEGREEN, FLOATSMOKE, KRINGEL,
 *     KRINGELR, KRINGELB, KRINGELG, KRINGELHB, KRINGELSECRET, TURBINESMOKE, BEAMSMOKE2,
 *     BEAMSMOKE3
 *   weather / ambience: REGENTROPFEN, SCHNEEFLOCKE, SCHNEEFLOCKE_END, FOG, BUBBLE, BLATT,
 *     BLATT2, THUNDERBOLT, SNOWFLUSH, WATERFLUSH, WATERFLUSH2
 *   faint smoke and trails: SMOKE2, SMOKE3, SMOKE3_RO .. SMOKE3_LO, EVILSMOKE, EVILSMOKE2,
 *     EVILROUNDSMOKE, FLUGSACKSMOKE, FLUGSACKSMOKE2, ROBOMANSMOKE, DRACHE_SMOKE,
 *     FIREBALL_SMOKE, LILA, LILA2, LILA3, BEAMSMOKE, BEAMSMOKE4
 *   glows / flares / scaled rings (additive): SHOTFLARE, SHOTFLARE2, LASERFLARE, GRENADEFLARE,
 *     EXPLOSIONFLARE, EXPLOSIONFLARE2, MINIFLARE, UFOLASERFLARE, STELZFLARE, LASERFLAME,
 *     LASERFLAMEPHARAO, SHIELD, EXTRACOLLECTED, EXPLOSION_KRINGEL, SHOCKEXPLOSION, BEAMSMOKE5
 *   text: TEXTSECRET (no text at run time)
 *
 * Units as Hurrican's (pixels of the 640 x 480 screen, speeds per 1/10 s) in fx; one call of
 * part_step_all is one 60 Hz step (Hurrican's Run with SpeedFaktor 1/6).
 */

#define PART_MAX 40

/* the pictures (LoadSprites), looked up by name in part_load */
enum {
	PP_NONE, PP_EXPL_MED, PP_EXPL_MED2, PP_EXPL_GREEN, PP_EXPL_ALIEN, PP_EXPL_BIG, PP_EXPL_GIANT,
	PP_EXPL_BLUE, PP_EXPL_GIGA, PP_EXPL_REG, PP_GLAS, PP_PIRANHA, PP_PIRANHA2, PP_PIRBLUT,
	PP_MADEBLUT, PP_NEST, PP_ROCK, PP_ROCKS, PP_ROCKSB, PP_SPIDS, PP_SPIDS2, PP_SPIDGREN,
	PP_STELZE, PP_STELZHEAD, PP_SMOKE, PP_SMOKEBIG, PP_HALS, PP_TURB, PP_HURRI, PP_HURRI2,
	PP_BOULDER, PP_SPRITZER, PP_KRABBE, PP_SPIDERPARTS, PP_KETTE, PP_KETTE2, PP_SCHROTT1,
	PP_SCHROTT2, PP_SNOWFLUSH, PP_DIAMANT, PP_COUNT
};
/* file name; the second number: which of the pictures whose first 16 letters are the same
 * (the packs keep 16 letters of a name, sorted: "explosion-medium.png" before
 * "explosion-medium2.png", "rocksplittersmall.png" before "rocksplittersmallblue.png") */
static const struct { const char *name; uint8_t nth; } part_pic_names[PP_COUNT] = {
	[PP_NONE] = { 0, 0 },
	[PP_EXPL_MED] = { "explosion-medium.png", 0 },
	[PP_EXPL_MED2] = { "explosion-medium2.png", 1 },
	[PP_EXPL_GREEN] = { "explosion-green.png", 0 },
	[PP_EXPL_ALIEN] = { "alienexplosion.png", 0 },
	[PP_EXPL_BIG] = { "explosion-big.png", 0 },
	[PP_EXPL_GIANT] = { "explosion-big2.png", 0 },
	[PP_EXPL_BLUE] = { "blue-explosion.png", 0 },
	[PP_EXPL_GIGA] = { "explosion-giga.png", 0 },
	[PP_EXPL_REG] = { "explosion-regular.png", 0 },
	[PP_GLAS] = { "glassplitter.png", 0 },
	[PP_PIRANHA] = { "piranhateile.png", 0 },
	[PP_PIRANHA2] = { "piranhateile2.png", 0 },
	[PP_PIRBLUT] = { "piranhablut.png", 0 },
	[PP_MADEBLUT] = { "madeblut.png", 0 },
	[PP_NEST] = { "nest.png", 0 },
	[PP_ROCK] = { "rocksplitter.png", 0 },
	[PP_ROCKS] = { "rocksplittersmall.png", 0 },
	[PP_ROCKSB] = { "rocksplittersmallblue.png", 1 },
	[PP_SPIDS] = { "spidersplitter.png", 0 },
	[PP_SPIDS2] = { "spidersplitter2.png", 0 },
	[PP_SPIDGREN] = { "spidergrenade.png", 0 },
	[PP_STELZE] = { "stelze.png", 0 },
	[PP_STELZHEAD] = { "stelzhead.png", 0 },
	[PP_SMOKE] = { "smoke.png", 0 },
	[PP_SMOKEBIG] = { "smokebig.png", 0 },
	[PP_HALS] = { "metalhead_halsteil.png", 0 },
	[PP_TURB] = { "metalhead_turbine.png", 0 },
	[PP_HURRI] = { "p1_hurri-teile.png", 0 },
	[PP_HURRI2] = { "p2_hurri-teile.png", 0 },
	[PP_BOULDER] = { "boulder_small.png", 0 },
	[PP_SPRITZER] = { "wasserspritzer.png", 0 },
	[PP_KRABBE] = { "lavakrabbe_teile.png", 0 },
	[PP_SPIDERPARTS] = { "spiderparts.png", 0 },
	[PP_KETTE] = { "kettenteile.png", 0 },
	[PP_KETTE2] = { "kettenteil2.png", 0 },
	[PP_SCHROTT1] = { "schrott1.png", 0 },
	[PP_SCHROTT2] = { "schrott2.png", 0 },
	[PP_SNOWFLUSH] = { "snowflush.png", 0 },
	[PP_DIAMANT] = { "diamantcollected.png", 0 },
};
static Pic part_pics[PP_COUNT];

/* PartikelRect (LoadSprites), {left, top, right, bottom}, for the kept types */
static const Box part_rect[MAX_PARTIKELGFX] = {
	[EXPLOSION_MEDIUM] = { 0, 0, 60, 60 },  [EXPLOSION_MEDIUM2] = { 0, 0, 60, 60 },
	[EXPLOSION_MEDIUM3] = { 0, 0, 60, 60 }, [EXPLOSION_MEDIUM2_ADD] = { 0, 0, 60, 60 },
	[EXPLOSION_MEDIUM3_ADD] = { 0, 0, 60, 60 },
	[EXPLOSION_GREEN] = { 0, 0, 60, 60 },   [EXPLOSION_ALIEN] = { 0, 0, 60, 60 },
	[EXPLOSION_BIG] = { 0, 0, 100, 100 },   [EXPLOSION_GIANT] = { 0, 0, 120, 120 },
	[EXPLOSION_TRACE] = { 0, 0, 120, 120 }, [BLUE_EXPLOSION] = { 0, 0, 24, 24 },
	[EXPLOSION_GIGA] = { 0, 0, 200, 200 },  [EXPLOSION_REGULAR] = { 0, 0, 76, 76 },
	[EXPLOSION_TRACE_END] = { 0, 0, 76, 76 },
	[GLASSPLITTER] = { 8, 8, 12, 12 },
	[PIRANHATEILE] = { 0, 0, 20, 16 },      [PIRANHATEILE2] = { 0, 0, 120, 90 },
	[PIRANHABLUT] = { 0, 0, 40, 40 },       [MADEBLUT] = { 0, 0, 12, 12 },
	[NESTLUFT] = { 0, 0, 43, 60 },
	[ROCKSPLITTER] = { 0, 0, 24, 24 },      [ROCKSPLITTERSMALL] = { 0, 0, 12, 12 },
	[ROCKSPLITTERSMALLBLUE] = { 0, 0, 12, 12 },
	[SPIDERSPLITTER] = { 2, 2, 14, 14 },    [SPIDERSPLITTER2] = { 2, 2, 14, 14 },
	[SPIDERGRENADE] = { 0, 0, 16, 16 },
	[STELZE] = { 30, 30, 70, 70 },          [STELZHEAD] = { 0, 0, 64, 60 },
	[SMOKE] = { 0, 0, 30, 30 },
	[SMOKEBIG] = { 0, 0, 60, 60 },          [SMOKEBIG2] = { 0, 0, 60, 60 },
	[SMOKEBIG_OUTTRO] = { 0, 0, 60, 60 },
	[HALSWIRBEL] = { 0, 0, 64, 64 },        [KAPUTTETURBINE] = { 0, 0, 98, 49 },
	[HURRITEILE] = { 10, 10, 22, 22 },      [HURRITEILE_P2] = { 10, 10, 22, 22 },
	[BOULDER_SMALL] = { 0, 0, 12, 11 },
	[WASSER_SPRITZER] = { 0, 0, 40, 128 },  [WASSER_SPRITZER2] = { 0, 0, 64, 128 },
	[LAVAKRABBE_KOPF] = { 12, 12, 28, 28 },
	[SPIDERPARTS] = { 0, 0, 48, 36 },
	[KETTENTEILE] = { 10, 10, 40, 32 },     [KETTENTEILE4] = { 0, 0, 11, 21 },
	[SCHROTT1] = { 10, 10, 30, 30 },        [SCHROTT2] = { 5, 5, 25, 25 },
	[SNOWFLUSH] = { 0, 0, 32, 32 },
	[DIAMANTCOLLECTED] = { 0, 0, 29, 29 },
};

/* one particle (PartikelClass) */
enum { PF_BOUNCE = 1, PF_OFFRM = 2, PF_FADE = 4, PF_MAJOR = 8, PF_NEW = 16 };
typedef struct {
	fx xPos, yPos, xPosOld, yPosOld;
	fx xSpeed, ySpeed, xAcc, yAcc;
	fx Lebensdauer, AnimCount, AnimSpeed;
	int16_t PartikelArt, AnimPhase, AnimEnde;
	uint16_t seq;                 /* push counter: the oldest has the largest age */
	uint8_t live, pic, flags, alpha;
} Part;
static Part parts[PART_MAX];
static uint16_t part_seq;
static int part_cur = -1;        /* the slot being run (never taken by a push from its Run) */
static uint8_t part_running;     /* in part_step_all: new particles wait for the next step */
static uint8_t part_splash_n;    /* every third splash */

#define PRD(n, d) ((fx)(((n) * 256) / (d)))   /* (float)n / d as fx (d a constant) */

/* the nth picture with this name (the packs keep 16 letters of a name) */
static Pic part_pic_nth(const char *name, int nth)
{
	(void)nth;                    /* the packs keep whole names now */
	return pic(name);
}

static void part_load(void)
{
	for (int i = 1; i < PP_COUNT; i++) part_pics[i] = part_pic_nth(part_pic_names[i].name, part_pic_names[i].nth);
}

static void part_clear(void)
{
	memset(parts, 0, sizeof parts);
	part_cur = -1;
	part_running = 0;
}

/* the picture of a (kept, final) type */
static int part_pic_of(int art)
{
	switch (art)
	{
	case EXPLOSION_MEDIUM: case EXPLOSION_MEDIUM3: case EXPLOSION_MEDIUM3_ADD: return PP_EXPL_MED;
	case EXPLOSION_MEDIUM2: case EXPLOSION_MEDIUM2_ADD: return PP_EXPL_MED2;
	case EXPLOSION_GREEN: return PP_EXPL_GREEN;
	case EXPLOSION_ALIEN: return PP_EXPL_ALIEN;
	case EXPLOSION_BIG: return PP_EXPL_BIG;
	case EXPLOSION_GIANT: return PP_EXPL_GIANT;
	case BLUE_EXPLOSION: return PP_EXPL_BLUE;
	case EXPLOSION_GIGA: return PP_EXPL_GIGA;
	case EXPLOSION_REGULAR: case EXPLOSION_TRACE_END: return PP_EXPL_REG;
	case GLASSPLITTER: return PP_GLAS;
	case PIRANHATEILE: return PP_PIRANHA;
	case PIRANHATEILE2: return PP_PIRANHA2;
	case PIRANHABLUT: return PP_PIRBLUT;
	case MADEBLUT: return PP_MADEBLUT;
	case NESTLUFT: return PP_NEST;
	case ROCKSPLITTER: return PP_ROCK;
	case ROCKSPLITTERSMALL: return PP_ROCKS;
	case ROCKSPLITTERSMALLBLUE: return PP_ROCKSB;
	case SPIDERSPLITTER: return PP_SPIDS;
	case SPIDERSPLITTER2: return PP_SPIDS2;
	case SPIDERGRENADE: return PP_SPIDGREN;
	case STELZE: return PP_STELZE;
	case STELZHEAD: return PP_STELZHEAD;
	case SMOKE: return PP_SMOKE;
	case SMOKEBIG: case SMOKEBIG2: case SMOKEBIG_OUTTRO: return PP_SMOKEBIG;
	case HALSWIRBEL: return PP_HALS;
	case KAPUTTETURBINE: return PP_TURB;
	case HURRITEILE: return PP_HURRI;
	case HURRITEILE_P2: return PP_HURRI2;
	case BOULDER_SMALL: return PP_BOULDER;
	case WASSER_SPRITZER: case WASSER_SPRITZER2: return PP_SPRITZER;
	case LAVAKRABBE_KOPF: return PP_KRABBE;
	case SPIDERPARTS: return PP_SPIDERPARTS;
	case KETTENTEILE: return PP_KETTE;
	case KETTENTEILE4: return PP_KETTE2;
	case SCHROTT1: return PP_SCHROTT1;
	case SCHROTT2: return PP_SCHROTT2;
	case SNOWFLUSH: return PP_SNOWFLUSH;
	case DIAMANTCOLLECTED: return PP_DIAMANT;
	default: return PP_NONE;          /* EXPLOSION_TRACE: not drawn */
	}
}

/* 1 if the type is kept; *major: an explosion or a big part (see the header) */
static int part_kept(int art, int *major)
{
	*major = 0;
	switch (art)
	{
	case EXPLOSION_MEDIUM: case EXPLOSION_MEDIUM2: case EXPLOSION_MEDIUM3:
	case EXPLOSION_MEDIUM2_ADD: case EXPLOSION_MEDIUM3_ADD: case EXPLOSION_GREEN:
	case EXPLOSION_BIG: case EXPLOSION_GIANT: case EXPLOSION_ALIEN: case BLUE_EXPLOSION:
	case EXPLOSION_GIGA: case EXPLOSION_REGULAR: case EXPLOSION_TRACE: case EXPLOSION_TRACE_END:
	case STELZHEAD: case STELZE: case HALSWIRBEL: case KAPUTTETURBINE: case PIRANHATEILE2:
	case NESTLUFT:
	case HURRITEILE: case HURRITEILE_ARM1: case HURRITEILE_ARM2: case HURRITEILE_BEIN1:
	case HURRITEILE_BEIN2: case HURRITEILE_KOPF: case HURRITEILE_WAFFE: case HURRITEILE_TORSO:
	case HURRITEILE_P2: case HURRITEILE_P2_ARM1: case HURRITEILE_P2_ARM2: case HURRITEILE_P2_BEIN1:
	case HURRITEILE_P2_BEIN2: case HURRITEILE_P2_KOPF: case HURRITEILE_P2_WAFFE:
	case HURRITEILE_P2_TORSO:
		*major = 1;
		return 1;
	case SMOKE: case SMOKEBIG: case SMOKEBIG2: case SMOKEBIG_OUTTRO:
	case GLASSPLITTER: case PIRANHATEILE: case PIRANHABLUT: case MADEBLUT:
	case ROCKSPLITTER: case ROCKSPLITTERSMALL: case ROCKSPLITTERSMALLBLUE: case BOULDER_SMALL:
	case SPIDERSPLITTER: case SPIDERSPLITTER2: case SPIDERGRENADE:
	case LAVAKRABBE_KOPF: case LAVAKRABBE_BEIN: case SPIDERPARTS:
	case KETTENTEILE: case KETTENTEILE2: case KETTENTEILE3: case KETTENTEILE4:
	case SCHROTT1: case SCHROTT2:
	case WASSER_SPRITZER: case WASSER_SPRITZER2: case LAVA_SPRITZER: case LAVA_SPRITZER2:
	case WATERFLUSH_HIGH: case WATERFLUSH_HIGH2: case DIAMANTCOLLECTED:
		return 1;
	default:
		return 0;
	}
}

/* a slot for a new particle: a free one, else the oldest minor one, else (for a major one)
 * the oldest major one; never the one being run. -1: none */
static int part_slot(int major)
{
	int best = -1, best_age = -1, best_major = -1, best_major_age = -1;
	for (int i = 0; i < PART_MAX; i++)
	{
		const Part *q = &parts[i];
		if (!q->live) return i;
		if (i == part_cur) continue;
		const int age = (uint16_t)(part_seq - q->seq);
		if (q->flags & PF_MAJOR) { if (age > best_major_age) { best_major_age = age; best_major = i; } }
		else if (age > best_age) { best_age = age; best = i; }
	}
	if (best >= 0) return best;
	return major ? best_major : -1;
}

/* PushPartikel + CreatePartikel */
static void part_push(fx x, fx y, int art)
{
	int major;
	if (art < 0 || art >= MAX_PARTIKELGFX || !part_kept(art, &major)) return;
	if (art == WASSER_SPRITZER || art == WASSER_SPRITZER2 || art == LAVA_SPRITZER || art == LAVA_SPRITZER2)
	{
		const int n = part_splash_n;
		part_splash_n = (uint8_t)(n >= 2 ? 0 : n + 1);
		if (n) return;
	}
	const int k = part_slot(major);
	if (k < 0) return;
	Part *q = &parts[k];
	memset(q, 0, sizeof *q);
	q->live = 1;
	q->seq = part_seq++;
	q->flags = (uint8_t)(PF_OFFRM | PF_FADE | (major ? PF_MAJOR : 0) | (part_running ? PF_NEW : 0));
	q->alpha = 255;
	q->Lebensdauer = FX(255);
	q->xPos = q->xPosOld = x;
	q->yPos = q->yPosOld = y;
	q->PartikelArt = (int16_t)art;

	switch (art)
	{
	case GLASSPLITTER:
		q->xSpeed = PRD(GetRandom(40) - 20, 2);
		q->ySpeed = -PRD(GetRandom(10) + 20, 2);
		q->yAcc = FX(5);
		q->flags |= PF_BOUNCE;
		q->AnimPhase = (int16_t)GetRandom(20);
		break;
	case EXPLOSION_MEDIUM:
	case EXPLOSION_MEDIUM2:
	case EXPLOSION_MEDIUM3:
	case EXPLOSION_MEDIUM2_ADD:
	case EXPLOSION_MEDIUM3_ADD:
		q->Lebensdauer = FX(192);
		q->AnimEnde = 19;
		q->AnimSpeed = PRD(GetRandom(6) + 3, 20);
		/* (EXPLOSIONFLARE: a glow, left out) */
		if (art == EXPLOSION_MEDIUM2_ADD) q->PartikelArt = EXPLOSION_MEDIUM2;
		else if (art == EXPLOSION_MEDIUM3_ADD) q->PartikelArt = EXPLOSION_MEDIUM3;
		else if (GetRandom(2) == 0)
			q->PartikelArt = art == EXPLOSION_MEDIUM3 ? EXPLOSION_MEDIUM3_ADD : EXPLOSION_MEDIUM2_ADD;
		break;
	case EXPLOSION_GIGA:
		q->Lebensdauer = FX(192);
		q->AnimEnde = 19;
		q->AnimSpeed = PRD(GetRandom(2) + 5, 10);
		/* (EXPLOSIONFLARE, 20 FUNKE, 20 LONGFUNKE: left out) */
		break;
	case EXPLOSION_GREEN:
		q->Lebensdauer = FX(192);
		q->AnimEnde = 24;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 15);
		break;
	case EXPLOSION_ALIEN:
		q->Lebensdauer = FX(224);
		q->AnimEnde = 24;
		q->AnimSpeed = PRD(GetRandom(5) + 10, 80);
		break;
	case EXPLOSION_BIG:
		q->Lebensdauer = FX(224);
		q->AnimEnde = 19;
		q->AnimSpeed = PRD(GetRandom(5) + 10, 30);
		/* (EXPLOSION_KRINGEL, EXPLOSIONFLARE; SMOKEBIG2 only at maximum detail) */
		break;
	case EXPLOSION_GIANT:
		q->Lebensdauer = FX(192);
		q->AnimEnde = 15;
		q->AnimSpeed = PRD(GetRandom(5) + 10, 15);
		break;
	case BLUE_EXPLOSION:
		q->Lebensdauer = FX(224);
		q->AnimEnde = 11;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 15);
		break;
	case PIRANHATEILE:
	case PIRANHATEILE2:
		q->AnimPhase = (int16_t)GetRandom(5);
		if (GetRandom(2) == 0) { q->xSpeed = -PRD(GetRandom(20) + 4, 5); q->xAcc = FX(0.1); }
		else { q->xSpeed = PRD(GetRandom(20) + 4, 5); q->xAcc = -FX(0.1); }
		q->yAcc = -PRD(GetRandom(10) + 20, 100);
		break;
	case PIRANHABLUT:
		q->alpha = 192;
		q->Lebensdauer = FX(192);
		q->AnimEnde = 3;
		q->AnimSpeed = FX(2);
		q->ySpeed = -PRD(GetRandom(10) + 20, 20);
		break;
	case MADEBLUT:
		q->alpha = 192;
		q->Lebensdauer = FX(128);
		q->AnimEnde = 3;
		q->AnimSpeed = FX(1);
		break;
	case NESTLUFT:
		q->yAcc = FX(5);
		q->xSpeed = PRD(GetRandom(40) - 20, 2);
		q->ySpeed = -FX(GetRandom(10) + 20);
		q->AnimSpeed = FX(1.5);
		q->flags |= PF_BOUNCE;
		break;
	case HALSWIRBEL:
		q->alpha = 0;
		q->xSpeed = PRD(GetRandom(20) + 20, 2);
		q->ySpeed = -FX(GetRandom(20) + 30);
		q->yAcc = FX(5);
		break;
	case KAPUTTETURBINE:
		q->alpha = 0;
		q->xSpeed = FX(18);
		q->ySpeed = -FX(30);
		q->yAcc = FX(5);
		break;
	case ROCKSPLITTER:
		q->xSpeed = PRD(GetRandom(80) - 40, 4);
		q->ySpeed = -PRD(GetRandom(30) + 15, 4);
		q->yAcc = FX(2.8);
		q->flags |= PF_BOUNCE;
		q->AnimEnde = 7;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 12);
		break;
	case ROCKSPLITTERSMALL:
	case ROCKSPLITTERSMALLBLUE:
		q->xSpeed = PRD(GetRandom(80) - 40, 4);
		q->ySpeed = -PRD(GetRandom(40) + 20, 3);
		q->yAcc = FX(2.8);
		q->flags |= PF_BOUNCE;
		q->AnimEnde = 7;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 8);
		break;
	case SPIDERSPLITTER:
	case SPIDERSPLITTER2:
		if (art == SPIDERSPLITTER) q->PartikelArt = (int16_t)(SPIDERSPLITTER + GetRandom(2));
		/* (grey tint: left out) */
		q->xSpeed = PRD(GetRandom(80) - 40, 3);
		q->ySpeed = -PRD(GetRandom(30) + 15, 2);
		q->yAcc = FX(3);
		q->flags |= PF_BOUNCE;
		q->AnimEnde = 15;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 12);
		break;
	case SPIDERGRENADE:
		q->xSpeed = -FX(14);
		q->ySpeed = -FX(15);
		q->yAcc = FX(5);
		q->AnimEnde = 3;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 12);
		q->flags &= (uint8_t)~PF_FADE;
		break;
	case STELZE:
		q->xSpeed = PRD(GetRandom(40) - 20, 3);
		q->ySpeed = -PRD(GetRandom(80) + 40, 3);
		q->yAcc = FX(5);
		q->flags &= (uint8_t)~PF_FADE;
		break;
	case STELZHEAD:
		q->xSpeed = PRD(GetRandom(40) - 20, 5);
		q->ySpeed = -PRD(GetRandom(80) + 40, 4);
		q->yAcc = FX(4);
		q->AnimCount = FX(1);
		q->flags &= (uint8_t)~PF_FADE;
		break;
	case SMOKE:
		q->ySpeed = -PRD(GetRandom(10), 10);
		q->yAcc = -FX(0.1);
		break;
	case SMOKEBIG_OUTTRO:
		q->Lebensdauer = FX(90);
		q->xSpeed = PRD(GetRandom(40) - 20, 6);
		q->ySpeed = -PRD(GetRandom(10) + 10, 5);
		q->AnimPhase = (int16_t)GetRandom(4);
		break;
	case SMOKEBIG:
		q->alpha = 140;
		q->Lebensdauer = FX(140);
		q->xSpeed = PRD(GetRandom(40) - 20, 10);
		q->ySpeed = -PRD(GetRandom(20) + 10, 10);
		q->AnimPhase = (int16_t)GetRandom(4);
		break;
	case SMOKEBIG2:
		q->alpha = 200;
		q->Lebensdauer = FX(200);
		q->xSpeed = FX(GetRandom(6) - 3) * 6;
		q->ySpeed = FX(GetRandom(10) - 5) * 2;
		q->yAcc = -FX(1);
		q->AnimPhase = (int16_t)GetRandom(4);
		break;
	case SCHROTT1:
		q->AnimEnde = 19;
		q->AnimSpeed = PRD(GetRandom(10) + 10, 20);
		q->xSpeed = PRD(GetRandom(80) - 40, 4);
		q->ySpeed = -FX(GetRandom(10) + 8);
		q->yAcc = FX(3);
		q->flags |= PF_BOUNCE;
		if (GetRandom(2) == 0) q->PartikelArt = SCHROTT2;
		break;
	case BOULDER_SMALL:
		q->xSpeed = PRD(GetRandom(80) - 40, 3);
		q->ySpeed = -PRD(GetRandom(80) - 20, 3);
		q->yAcc = FX(5);
		q->flags |= PF_BOUNCE;
		q->AnimEnde = 14;
		q->AnimSpeed = PRD(GetRandom(10) + 5, 20);
		break;
	case LAVAKRABBE_KOPF:
	case LAVAKRABBE_BEIN:
		q->xSpeed = PRD(GetRandom(80) - 40, 3);
		q->ySpeed = -PRD(GetRandom(60) + 20, 2);
		q->yAcc = FX(5);
		q->flags |= PF_BOUNCE;
		if (art == LAVAKRABBE_BEIN) { q->AnimPhase = 1; q->PartikelArt = LAVAKRABBE_KOPF; }
		break;
	case SPIDERPARTS:
		q->AnimPhase = (int16_t)GetRandom(4);
		q->flags &= (uint8_t)~(PF_OFFRM | PF_FADE);
		break;
	case KETTENTEILE:
	case KETTENTEILE2:
	case KETTENTEILE3:
	case KETTENTEILE4:
		q->xSpeed = PRD(GetRandom(80) - 40, 3);
		q->ySpeed = -PRD(GetRandom(30) + 10, 2);
		q->yAcc = FX(5);
		q->flags |= PF_BOUNCE;
		if (art == KETTENTEILE2 || art == KETTENTEILE3)
		{
			q->AnimPhase = (int16_t)(art - KETTENTEILE);
			q->PartikelArt = KETTENTEILE;
		}
		break;
	case WASSER_SPRITZER:
	case WASSER_SPRITZER2:
		/* (white / water colour tint: left out) */
		if (GetRandom(2) == 0) q->Lebensdauer = FX(GetRandom(128) + 128);
		else q->Lebensdauer = FX(GetRandom(32) + 224);
		if (art == WASSER_SPRITZER)
		{
			q->AnimSpeed = FX(GetRandom(20) + 30);
			q->AnimCount = PRD(GetRandom(25) + 5, 10);
		}
		else
		{
			q->AnimSpeed = FX(GetRandom(15) + 25);
			q->AnimCount = PRD(GetRandom(25) + 5, 8);
		}
		break;
	case LAVA_SPRITZER:
		q->AnimSpeed = FX(GetRandom(20) + 30);
		q->AnimCount = PRD(GetRandom(25) + 5, 10);
		q->PartikelArt = WASSER_SPRITZER;
		break;
	case LAVA_SPRITZER2:
		q->AnimSpeed = FX(GetRandom(15) + 25);
		q->AnimCount = PRD(GetRandom(25) + 5, 8);
		q->PartikelArt = WASSER_SPRITZER2;
		break;
	case HURRITEILE_ARM1: case HURRITEILE_ARM2: case HURRITEILE_BEIN1: case HURRITEILE_BEIN2:
	case HURRITEILE_KOPF: case HURRITEILE_WAFFE: case HURRITEILE_TORSO:
	case HURRITEILE_P2_ARM1: case HURRITEILE_P2_ARM2: case HURRITEILE_P2_BEIN1:
	case HURRITEILE_P2_BEIN2: case HURRITEILE_P2_KOPF: case HURRITEILE_P2_WAFFE:
	case HURRITEILE_P2_TORSO:
	{
		const int p2 = art >= HURRITEILE_P2_ARM1;
		q->AnimPhase = (int16_t)(art - (p2 ? HURRITEILE_P2_ARM1 : HURRITEILE_ARM1));
		q->xSpeed = PRD(GetRandom(80) - 40, 4);
		q->ySpeed = -FX(2.5) * (q->AnimPhase + 2);          /* -15 * (AnimPhase + 2) / 6 */
		q->yAcc = FX(2);
		q->flags |= PF_BOUNCE;
		q->PartikelArt = p2 ? HURRITEILE_P2 : HURRITEILE;
		break;
	}
	case WATERFLUSH_HIGH:
		q->xSpeed = PRD(GetRandom(80) - 40, 2);
		q->ySpeed = -PRD(GetRandom(100) + 100, 5);
		q->yAcc = FX(4);
		q->Lebensdauer = FX(128);
		q->PartikelArt = SNOWFLUSH;
		break;
	case WATERFLUSH_HIGH2:
		q->xSpeed = PRD(GetRandom(40) - 20, 4);
		q->ySpeed = -PRD(GetRandom(50) + 50, 3);
		q->yAcc = FX(6);
		q->Lebensdauer = FX(128);
		q->PartikelArt = SNOWFLUSH;
		break;
	case DIAMANTCOLLECTED:
		q->AnimCount = FX(16);
		break;
	case EXPLOSION_TRACE_END:
		q->Lebensdauer = FX(150);
		q->AnimEnde = 17;
		q->AnimPhase = 5;
		q->AnimSpeed = FX(3);
		/* (its size from WinkelUebergabe: drawn at the natural size) */
		break;
	case EXPLOSION_TRACE:
		q->xPos -= FX(60);
		q->yPos -= FX(60);
		q->xSpeed = FX(GetRandom(80) - 40);
		q->ySpeed = -FX(GetRandom(10) + 50);
		q->yAcc = FX(8);
		q->flags &= (uint8_t)~PF_FADE;
		break;
	case EXPLOSION_REGULAR:
		q->Lebensdauer = FX(128);
		q->AnimEnde = 17;
		q->AnimSpeed = FX(0.75);
		break;
	default:                          /* HURRITEILE / HURRITEILE_P2 pushed as such: as Hurrican */
		break;
	}
	q->pic = (uint8_t)part_pic_of(q->PartikelArt);
}

/* PartikelClass::Run, one 60 Hz step */
static void part_run(Part *q, fx xoff, fx yoff)
{
	const Box *r = &part_rect[q->PartikelArt];

	if (q->flags & PF_OFFRM)
	{
		const fx sx = q->xPos - xoff, sy = q->yPos - yoff;
		if (sy > FX(RENDERHEIGHT + 20) || sy + FX(r->b) < FX(20) ||
		    sx > FX(RENDERWIDTH + 20) || sx + FX(r->r) < FX(20))
			q->Lebensdauer = 0;
	}
	if (q->Lebensdauer <= 0) { q->live = 0; return; }

	/* move (only while visible) */
	if (q->alpha > 0)
	{
		q->xSpeed += SYNC(q->xAcc);
		q->ySpeed += SYNC(q->yAcc);
		q->xPos += SYNC(q->xSpeed);
		q->yPos += SYNC(q->ySpeed);
	}

	int bo = 0, bl = 0, br = 0, bu = 0;
	if (q->flags & PF_BOUNCE)
	{
		bo = BlockOben(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0);
		bl = BlockLinks(&q->xPos, q->yPos, &q->xPosOld, q->yPosOld, r, 0);
		br = BlockRechts(&q->xPos, q->yPos, &q->xPosOld, q->yPosOld, r, 0);
		bu = BlockUnten(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0);

		/* stuck in a wall: gone */
		if ((bo & B_WAND) && (bu & B_WAND) && ((bl & B_WAND) || (br & B_WAND)))
			q->Lebensdauer = 0;
		/* off the ceiling */
		if (q->ySpeed < 0 && (bo & B_WAND))
			q->ySpeed = -q->ySpeed;
		/* off the floor */
		if (q->ySpeed > 0 && (bu & (B_WAND | B_PLATTFORM)))
		{
			q->ySpeed = -(q->ySpeed >> 1);
			if (q->ySpeed > -FX(0.5))
			{
				q->ySpeed = 0;
				q->xSpeed -= q->xSpeed >> 7;             /* *= 0.99 */
			}
		}
		if ((q->xSpeed > 0 && (br & B_WAND)) || (q->xSpeed < 0 && (bl & B_WAND)))
			q->xSpeed = -q->xSpeed;
	}

	/* animate */
	if (q->AnimEnde > 0)
	{
		q->AnimCount += SPEEDFACTOR;
		if (q->AnimCount > q->AnimSpeed)
		{
			q->AnimCount = 0;
			q->AnimPhase++;
		}
	}

	switch (q->PartikelArt)
	{
	case GLASSPLITTER:
		q->Lebensdauer -= SYNC(FX(20));
		break;
	case EXPLOSION_GREEN: case EXPLOSION_ALIEN: case EXPLOSION_MEDIUM: case EXPLOSION_MEDIUM2:
	case EXPLOSION_MEDIUM3: case EXPLOSION_GIGA: case EXPLOSION_BIG: case EXPLOSION_GIANT:
	case EXPLOSION_MEDIUM2_ADD: case EXPLOSION_MEDIUM3_ADD: case EXPLOSION_REGULAR:
		q->Lebensdauer -= SYNC(FX(5));
		if (q->AnimPhase > q->AnimEnde) q->Lebensdauer = 0;
		break;
	case BLUE_EXPLOSION:
		q->Lebensdauer -= SYNC(FX(12));
		if (q->AnimPhase > q->AnimEnde) q->Lebensdauer = 0;
		break;
	case EXPLOSION_TRACE_END:
		q->Lebensdauer -= SYNC(FX(4));
		if (q->AnimPhase > q->AnimEnde) q->Lebensdauer = 0;
		break;
	case HURRITEILE:
	case HURRITEILE_P2:
		q->Lebensdauer -= SYNC(FX(5));
		break;
	case LAVAKRABBE_KOPF:
	case KETTENTEILE:
	case KETTENTEILE4:
		q->Lebensdauer -= SYNC(FX(10));
		break;
	case SPIDERPARTS:
		bl = BlockLinks(&q->xPos, q->yPos, &q->xPosOld, q->yPosOld, r, 0);
		br = BlockRechts(&q->xPos, q->yPos, &q->xPosOld, q->yPosOld, r, 0);
		bu = BlockUntenNormal(q->xPos, q->yPos, q->xPosOld, q->yPosOld, r);
		if ((bl | br) & B_WAND)
		{
			if (q->ySpeed > 0) q->xSpeed = (bl & B_WAND) ? FX(11) : -FX(11);
			else q->Lebensdauer = 0;
		}
		/* (the conveyor belt: no such flag in the engine's map) */
		if (!(bu & B_WAND)) q->yAcc = FX(5);
		else { q->yAcc = 0; q->ySpeed = 0; }
		if (q->yPos > yoff + FX(500)) q->Lebensdauer = 0;
		break;
	case PIRANHATEILE:
	case PIRANHATEILE2:
		bo = BlockOben(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0);
		if (q->ySpeed < -FX(2)) q->ySpeed = -FX(2);
		if (q->ySpeed == 0) q->Lebensdauer -= SYNC(FX(7));      /* at the surface */
		if ((q->xAcc > 0 && q->xSpeed > 0) || (q->xAcc < 0 && q->xSpeed < 0))
		{
			q->xAcc = 0;
			q->xSpeed = 0;
		}
		if ((bl | br) & B_WAND) { q->xSpeed = 0; q->xAcc = 0; }
		if (!(bo & B_LIQUID)) { q->xSpeed = 0; q->ySpeed = 0; q->yAcc = 0; }
		break;
	case PIRANHABLUT:
		bo = BlockOben(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0);
		q->Lebensdauer -= SYNC(FX(5));
		if (!(bo & B_LIQUID)) q->Lebensdauer = 0;
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		break;
	case MADEBLUT:
		q->Lebensdauer -= SYNC(FX(15));
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		break;
	case NESTLUFT:
	{
		q->Lebensdauer -= SYNC(FX(5));
		const int bu2 = BlockUnten(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0);
		if (q->yAcc != 0 && (bu2 & (B_WAND | B_PLATTFORM)))
		{
			q->ySpeed = -FMUL(q->ySpeed, 171);                  /* -ySpeed / 1.5 */
			if (q->ySpeed > -FX(1)) { q->xSpeed = 0; q->ySpeed = 0; q->yAcc = 0; }
		}
		break;
	}
	case ROCKSPLITTER: case ROCKSPLITTERSMALL: case ROCKSPLITTERSMALLBLUE: case BOULDER_SMALL:
	case SPIDERSPLITTER: case SPIDERSPLITTER2:
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		q->Lebensdauer -= SYNC(FX(6));
		break;
	case WASSER_SPRITZER:
	case WASSER_SPRITZER2:
		q->Lebensdauer -= SYNC(q->AnimSpeed);
		break;
	case SPIDERGRENADE:
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		bu = BlockUntenNormal(q->xPos, q->yPos, q->xPosOld, q->yPosOld, r);
		if (bu & B_WAND) q->Lebensdauer = 0;
		break;
	case STELZE:
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		break;
	case STELZHEAD:
		q->AnimCount -= SYNC(FX(1));
		if (q->AnimCount < 0)
		{
			q->AnimCount = FX(0.5);
			part_push(q->xPos - FX(20) + FX(GetRandom(40)), q->yPos - FX(20) + FX(GetRandom(40)), EXPLOSION_MEDIUM2);
		}
		break;
	case SMOKEBIG:
		q->Lebensdauer -= SYNC(FX(4.5));
		break;
	case SMOKEBIG2:
		q->Lebensdauer -= SYNC(FX(4.5));
		if (q->ySpeed < -FX(5)) { q->yAcc = 0; q->ySpeed = -FX(5); }
		if (q->xSpeed > 0) q->xSpeed -= SYNC(FX(3));
		if (q->xSpeed < 0) q->xSpeed += SYNC(FX(3));
		break;
	case SMOKEBIG_OUTTRO:
		q->Lebensdauer -= SYNC(FX(1));
		break;
	case SMOKE:
		q->Lebensdauer -= SYNC(FX(30));
		break;
	case HALSWIRBEL:
	case KAPUTTETURBINE:
		q->Lebensdauer -= SYNC(FX(5));
		break;
	case SCHROTT1:
	case SCHROTT2:
		q->Lebensdauer -= SYNC(FX(5));
		if (q->AnimPhase > q->AnimEnde) q->AnimPhase = 0;
		break;
	case SNOWFLUSH:                   /* WATERFLUSH_HIGH / _HIGH2 */
		q->Lebensdauer -= SYNC(FX(8));
		if (BlockUnten(q->xPos, &q->yPos, q->xPosOld, &q->yPosOld, r, 0) & B_LIQUID)
			q->Lebensdauer -= SYNC(FX(20));
		break;
	case DIAMANTCOLLECTED:
		q->Lebensdauer -= SYNC(FX(120));
		q->AnimCount += SYNC(FX(40));
		break;
	case EXPLOSION_TRACE:
		q->Lebensdauer -= SYNC(FX(5));
		q->AnimCount -= SYNC(FX(1));
		if (q->AnimCount < 0)
		{
			q->AnimCount = FX(0.8);   /* Hurrican: 0.4 */
			part_push(q->xPos + FX(30), q->yPos + FX(30), EXPLOSION_REGULAR);
		}
		break;
	default:
		break;
	}

	q->xPosOld = q->xPos;
	q->yPosOld = q->yPos;
	if (q->flags & PF_FADE)
	{
		const int a = FINT(q->Lebensdauer);
		q->alpha = (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a);
	}
	if (q->Lebensdauer <= 0) q->live = 0;
}

#ifndef OJ32X_MARS
/* PC test: HB_PART="art,art,...[@steps]" pushes those particle types 150 pixels ahead of the
 * player every <steps> (default 120) steps */
static void part_test_hook(void)
{
	static int init, n, every = 120, t;
	static int arts[16];
	if (!init)
	{
		init = 1;
		const char *s = getenv("HB_PART");
		while (s && *s && n < 16)
		{
			char *e;
			const long v = strtol(s, &e, 10);
			if (e == s) break;
			arts[n++] = (int)v;
			s = e;
			if (*s == '@') { every = (int)strtol(s + 1, 0, 10); if (every < 1) every = 1; break; }
			if (*s == ',') s++;
		}
	}
	if (!n || ++t < every) return;
	t = 0;
	for (int i = 0; i < n; i++)
		part_push(pl.xpos + FX(150) * pl.Blickrichtung, pl.ypos + FX(10), arts[i]);
}
#endif

static void part_step_all(void)
{
#ifndef OJ32X_MARS
	part_test_hook();
#endif
	const fx xoff = cam_xoff(), yoff = cam_yoff();
	part_running = 1;
	for (int i = 0; i < PART_MAX; i++)
	{
		Part *q = &parts[i];
		if (!q->live || (q->flags & PF_NEW)) continue;
		part_cur = i;
		part_run(q, xoff, yoff);
	}
	part_cur = -1;
	part_running = 0;
	for (int i = 0; i < PART_MAX; i++) parts[i].flags &= (uint8_t)~PF_NEW;
}

/* PartikelClass::Render, as plain sprites */
static void part_draw_all(void)
{
	for (int i = 0; i < PART_MAX; i++)
	{
		const Part *q = &parts[i];
		if (!q->live || !q->pic) continue;
		const Pic *p = &part_pics[q->pic];
		if (!p->count || q->AnimPhase < 0 || q->AnimPhase >= p->count) continue;
		fx x = q->xPos, y = q->yPos;
		switch (q->PartikelArt)
		{
		case WASSER_SPRITZER:         /* scaled b x 100 at (x - b/4, y - 50): centred */
		case WASSER_SPRITZER2:
		{
			const int b = q->PartikelArt == WASSER_SPRITZER ? 200 - (q->alpha >> 1) : FINT(q->AnimCount * 30);
			x += FX((b >> 2) - p->fw);
			y -= FX(p->fh);
			break;
		}
		case DIAMANTCOLLECTED:        /* scaled, centred on (x + 15, y + 15) */
			x += FX(15 - p->fw);
			y += FX(15 - p->fh);
			break;
		default:
			break;
		}
		draw_sprite(p, q->AnimPhase, x, y, 0);
	}
}
