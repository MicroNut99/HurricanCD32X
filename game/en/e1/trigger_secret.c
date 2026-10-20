/*
 * SECRET - a secret area (triggers/Trigger_Secret.cpp): touched, it counts the level's secrets
 * (the summary screen) and shows its sparkle.
 * Own members: none (Value1 / Value2 = the area's width / height).
 * The sparkle's particles (KRINGELSECRET, TEXTSECRET, GRENADEFLARE, EXPLOSION_KRINGEL) are
 * pushed as Hurrican does; the particle system drops these types (glows / text).
 *
 * PC check (not on the 32X): E1_SPAWN="art,dx,dy[,v1,v2[,every]]" makes an object of type art
 * at the player + (dx, dy) from the step of a SECRET / SOUNDTRIGGER object (every stage has a
 * secret: warp near one), once, or again every `every` of those steps - for the types that no
 * map has (FETTERAKETE, MADE, COLUMN2, SWITCH, TUTORIALTEXT, WARNING).
 */
#ifndef OJ32X_MARS
static int secret_test_n = -1, secret_test_art, secret_test_dx, secret_test_dy, secret_test_v1,
           secret_test_v2, secret_test_every;
static void secret_test_hook(void)
{
	if (secret_test_n == -1)
	{
		secret_test_n = -2;
		const char *s = getenv("E1_SPAWN");
		if (s && sscanf(s, "%d,%d,%d,%d,%d,%d", &secret_test_art, &secret_test_dx, &secret_test_dy,
		                &secret_test_v1, &secret_test_v2, &secret_test_every) >= 3)
			secret_test_n = 0;
	}
	if (secret_test_n < 0) return;
	if (secret_test_n == 0 || (secret_test_every > 0 && secret_test_n % secret_test_every == 0))
	{
		Ent *o = gegner_push(pl.xpos + FX(secret_test_dx), pl.ypos + FX(secret_test_dy), secret_test_art,
		                     secret_test_v1, secret_test_v2);
		fprintf(stderr, "E1_SPAWN: art %d at %d,%d -> slot %d\n", secret_test_art, FINT(o->xPos), FINT(o->yPos),
		        o->GegnerArt < 0 ? -1 : (int)(o - ents));
	}
	secret_test_n++;
	if (secret_test_every <= 0 && secret_test_n > 0) secret_test_n = -3;
}
#else
static void secret_test_hook(void) {}
#endif

static void secret_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->Energy = FX(100);
	e->Destroyable = 0;
	e->TestBlock = 0;
	e->DontMove = 1;
}

static void secret_ki(Ent *e)
{
	secret_test_hook();

	/* not on the screen? */
	if (e->xPos + FX(10) < cam_xoff() || e->xPos > cam_xoff() + FX(RENDERWIDTH) ||
	    e->yPos + FX(10) < cam_yoff() || e->yPos > cam_yoff() + FX(RENDERHEIGHT))
		return;

	switch (e->Handlung)
	{
	case GEGNER_STEHEN:
	{
		Box *r = &GegnerRect[e->GegnerArt];
		r->l = (int16_t)(-(e->Value1 / 2) + 20);
		r->r = (int16_t)((e->Value1 / 2) + 20);
		r->t = (int16_t)(-(e->Value2 / 2) + 20);
		r->b = (int16_t)((e->Value2 / 2) + 20);
		if (SpriteCollision(e->xPos, e->yPos, r, pl.xpos, pl.ypos, &pl.CollideRect))
		{
			pl.SecretThisLevel++;
			e->Handlung = GEGNER_FALLEN;
			snd(SOUND_SECRET);
			for (int i = 0; i < 200; i++) part_push(e->xPos + FX(20), e->yPos + FX(20), KRINGELSECRET);
			proj_winkel = FX(-1);
			part_push(e->xPos + FX(20), e->yPos + FX(12), TEXTSECRET);
			part_push(e->xPos - FX(100), e->yPos - FX(100), GRENADEFLARE);
			part_push(e->xPos + FX(5), e->yPos + FX(5), EXPLOSION_KRINGEL);
		}
		break;
	}
	}
}

static void secret_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(SECRET) = { secret_init, secret_ki, secret_explode, 0, 0 };
