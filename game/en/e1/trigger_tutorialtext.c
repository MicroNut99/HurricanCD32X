/*
 * TUTORIALTEXT - a tutorial message trigger (triggers/Trigger_TutorialText.cpp): near the
 * player it shows text number Value1 and steps on to the next text when the player did what
 * the text asks.
 * Own members: none (Value1 = the text, as Hurrican).
 * Shared: tutorialtext_id / tutorialtext_visible stand in for the GUI box's text id and fade
 * state (GUI.GetTextID / GetFadeMode).
 * Left out (no tutorial, no text at run time): the message box itself and HUD.ShowArrow. Kept:
 * the message sound, the game changes (5 diamonds at text 7, a smart bomb at text 28, the
 * power lines reset at text 20) and the stepping on between texts - except text 16 -> 17
 * (the first power block shot at: needs the PowerBlock's ExtraType, which only its own port
 * knows). No map of the 9 stages has these triggers (the tutorial level is not part of it).
 */
static int tutorialtext_id = -1, tutorialtext_visible;

static void tutorialtext_init(Ent *e)
{
	e->Handlung = GEGNER_STEHEN;
	e->blick = -1;
	e->Energy = FX(100);
	e->AnimPhase = 0;
	e->Destroyable = 0;
	e->Active = 1;
	e->DontMove = 1;
}

static void tutorialtext_ki(Ent *e)
{
	/* near: show the text */
	if (PlayerAbstand(e) < 120 && e->Value1 != 9 &&
	    (!tutorialtext_visible || tutorialtext_id != e->Value1) &&
	    !(e->Value1 == 20 && pl.PowerLines == 5))
	{
		if (e->Value1 == 7 && pl.CollectedDiamonds < 5) e->Value2 = (int16_t)(pl.CollectedDiamonds = 5);
		if (e->Value1 == 28 && pl.SmartBombs == 0) pl.SmartBombs = 1;
		tutorialtext_visible = 1;
		tutorialtext_id = e->Value1;
		snd(SOUND_MESSAGE);
	}

	/* away: hide it */
	if (tutorialtext_id == e->Value1 && IsOnScreen(e) && tutorialtext_visible && PlayerAbstand(e) > 120)
		tutorialtext_visible = 0;

	/* on to the next text */
	if (PlayerAbstand(e) < 120)
		switch (e->Value1)
		{
		case 2:
			if (pl.CollectedDiamonds >= 5) e->Value1 = 3;
			break;
		case 7:
			if (pl.CollectedDiamonds >= 8) e->Value1 = 8;
			break;
		case 9:
		{
			int alive = 0;
			for (int k = 0; k < MAX_LIVE; k++)
				if (ents[k].GegnerArt == AUGE && ents[k].Value2 == 99) alive = 1;
			if (!alive) e->Value1 = 10;
			break;
		}
		case 10:
			if (pl.CurrentWeaponLevel[0] >= 2 && pl.BlinkCounter <= FX(3)) e->Value1 = 11;
			break;
		case 17:
			if (pl.PowerLines >= 5) e->Value1 = 18;
			break;
		case 20:
			if (pl.Grenades >= 2) { pl.PowerLines = 0; e->Value1 = 21; }
			break;
		case 28:
			if (pl.SmartBombs == 0) e->Value1 = 29;
			break;
		default:
			break;
		}
}

static void tutorialtext_explode(Ent *e) { gegner_default_explode(e); }

TYPEDEF(TUTORIALTEXT) = { tutorialtext_init, tutorialtext_ki, tutorialtext_explode, 0, 0 };
