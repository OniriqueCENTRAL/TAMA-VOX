// TAMA-VOX : scène de l'interface (habillage Onirique + Tamagotchi + menus). Indépendante de VSTGUI : elle ne dessine
// qu'avec Painter et ne parle au plugin que par IUiHost, donc testable hors DAW (rendu PNG via Cairo).
#pragma once
#include "../StdCommon.h"
#include "Lcd.h"
#include "Painter.h"
#include "UiHost.h"
#include <functional>
#include <string>
#include <vector>

namespace tama::ui
{
enum Mods { kShift = 1, kCtrl = 2, kAlt = 4 };
enum class CursorKind { Arrow, Hand, ResizeV, ResizeH };

class TamaScene
{
public:
	static constexpr int kW = 720, kH = 800;

	explicit TamaScene (IUiHost& h);

	void draw (Painter& p, double nowMs);

	void mouseDown (double x, double y, int button, int mods, int clicks);
	void mouseMove (double x, double y, int mods);
	void mouseUp (double x, double y, int button);
	void mouseLeave ();
	void wheel (double x, double y, double deltaY, int mods);
	bool keyDown (int vkey); // Echap / Entrée ; renvoie true si consommé
	CursorKind cursor () const { return cur; }

	// Pour les tests : états forcés
	void debugOpenMenu (int index) { openMenu = index; }
	void debugDialog (const std::string& title, const std::string& text) { showDialog (title, text); }
	void debugFlip (bool back) { flipTarget = back ? 1.0 : 0.0; flip = flipTarget; }
	void debugHoverEgg (bool on) { hoverEgg = on; swing = 0; }

private:
	// ---------- menus ----------
	struct Item
	{
		enum Kind { Action, Check, Radio, Slider, Sep } kind = Action;
		std::string label;
		bool on = false;
		std::function<void ()> run;
		uint32_t pid = 0;   // Slider
		double step = 0;
		std::string unitFmt;
	};
	std::vector<Item> buildMenu (int index) const;
	struct MenuGeom { double x, y, w, h; std::vector<double> itemY, itemH; };
	MenuGeom menuGeom (int index, const std::vector<Item>& items) const;
	int menuBarHit (double x, double y) const;
	double menuBarX (int i) const;
	void drawMenus (Painter& p);
	void setSliderFromX (const Item& it, const MenuGeom& g, double x);
	std::string sliderText (const Item& it) const;

	// ---------- dialogue ----------
	void showDialog (const std::string& title, const std::string& text);
	void drawDialog (Painter& p);
	struct DialogGeom { double x, y, w, h, okX, okY, okW, okH; };
	DialogGeom dialogGeom () const;

	// ---------- dessin ----------
	void update (double nowMs);
	void drawWindow (Painter& p);
	void drawTitleBar (Painter& p);
	void drawMenuBar (Painter& p);
	void drawStage (Painter& p);
	void drawEgg (Painter& p, double nowMs);
	void drawEggFront (Painter& p);
	void drawEggBack (Painter& p);
	void drawCrown (Painter& p);
	void drawChain (Painter& p);
	void drawReadout (Painter& p);
	void drawStatus (Painter& p);
	std::vector<Pt> eggShape () const;
	double swingAngle () const;
	void presetEdit (int i);
	bool presetMatches (int i) const;

	void raised (Painter& p, double x, double y, double w, double h, bool pressed = false) const;
	void sunken (Painter& p, double x, double y, double w, double h, Color fill) const;

	// ---------- géométrie de l'oeuf ----------
	Pt toEggSpace (double x, double y) const;          // coordonnées écran -> repère de l'oeuf (centre = 0,0), sans retournement
	bool hitKnob (Pt e) const;
	int hitButton (Pt e) const; // 0..2 ou -1
	bool hitEgg (Pt e) const;
	bool hitLcd (Pt e) const;

	void updateCursor (double x, double y);
	void pressButton (int i);

	IUiHost& host;
	// animation
	double lastMs = -1, swing = 0, swingPhase = 0, flip = 0, flipTarget = 0;
	bool hoverEgg = false;
	double mx = 0, my = 0;
	Color shell[4];                 // base, clair, sombre, liseré
	double lcdBright = 0.3;
	double sLamp = 0, sSquash = 0, sGr = 0, sDuck = 0, sTau = 0;
	double lastSignalMs = -1e9;
	int glitchFrames = 0;
	uint32_t frame = 0;
	// interaction
	int openMenu = -1, hoverItem = -1, hoverBar = -1;
	int dragSlider = -1;           // index d'item en cours de glissement
	int dragMenu = -1;
	bool dragKnob = false;
	double knobLastY = 0;
	int pressedButton = -1;
	int hoverButton = -1;
	bool hoverKnob = false;
	bool dialogOpen = false;
	std::string dialogTitle, dialogText;
	CursorKind cur = CursorKind::Arrow;
	int pressedTitleBtn = -1;
	double drivePreview = 0; // pour l'affichage de la valeur pendant le glissement
	double knobTipUntil = 0;
	double nowCached = 0;
};
} // namespace tama::ui
