/*
 * SHOOTBUTTON - the button on the ShootPlattform (triggers/Trigger_ShootButton.cpp): while it
 * is shot the platform accelerates upwards (with a jet flame); otherwise it sinks back.
 * Own members: i[0] = the platform's slot + 1 (0: not found yet), f[0] = SmokeCount.
 * Hurrican gives the constructor the platform (the object made just before); here the button
 * finds it on its first step: the ShootPlattform 42 pixels to its left nearest below it.
 */
static void shootbutton_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = 1;
	e->Energy = FX(100);
	e->Value1 = 0;
	e->Value2 = 0;
	e->Destroyable = 1;
	e->AnimEnde = 2;
	e->AnimSpeed = FX(5);
	e->i[0] = 0;
	e->f[0] = FX(0.1);
	e->OwnDraw = 1;
}

/* the platform, or 0 */
static Ent *shootbutton_platform(Ent *e)
{
	if (!e->i[0])
	{
		int k, best = -1;
		fx bd = FX(10000);
		for (k = 0; k < MAX_LIVE; k++)
		{
			const Ent *o = &ents[k];
			fx d;
			if (o->GegnerArt != SHOOTPLATTFORM || o->xPos != e->xPos - FX(42)) continue;
			d = FABS(o->yPos - (e->yPos + FX(9)));
			if (d < bd) { bd = d; best = k; }
		}
		if (best < 0) return 0;
		e->i[0] = best + 1;
	}
	if (ents[e->i[0] - 1].GegnerArt != SHOOTPLATTFORM) return 0;
	return &ents[e->i[0] - 1];
}

static void shootbutton_draw(Ent *e)
{
	const Ent *p = shootbutton_platform(e);
	if (p) draw_sprite(&GegnerPic[e->GegnerArt], e->AnimPhase, p->xPos + FX(41), p->yPos - FX(8), 0);
}

static void shootbutton_ki(Ent *e)
{
	Ent *p = shootbutton_platform(e);
	SimpleAnimation(e, 0);
	if (!p)
	{
		/* its platform is gone (Hurrican's never is) */
		if (e->i[0]) { e->Energy = 0; }
		return;
	}

	e->yPos = p->yPos - FX(9) + SYNC(p->ySpeed);

	/* shot? */
	if (e->Energy < FX(100))
	{
		e->Energy = FX(100);
		p->yAcc = FX(-3);
		p->Value2 = 1;
		/* the flame */
		e->f[0] -= SYNC(FX(1));
		if (e->f[0] <= 0)
		{
			e->f[0] = FX(0.2);
			proj_push(p->xPos + FX(23) + FX(GetRandom(4)), p->yPos - FX(15), FEUERFALLE3);
			part_push(p->xPos + FX(33) + FX(GetRandom(2)), p->yPos + FX(40), FLUGSACKSMOKE);
			part_push(p->xPos + FX(51) + FX(GetRandom(2)), p->yPos + FX(40), FLUGSACKSMOKE2);
			part_push(p->xPos + FX(48) + FX(GetRandom(2)), p->yPos + FX(40), FUNKE);
		}
	}
	else if (FINT(p->yPos) < p->Value1)
		p->yAcc = FX(3);
}

static void shootbutton_explode(Ent *e)
{
	gegner_default_explode(e);
}

TYPEDEF(SHOOTBUTTON) = { shootbutton_init, shootbutton_ki, shootbutton_explode, shootbutton_draw, 0 };
