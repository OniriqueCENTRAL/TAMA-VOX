#include "../StdCommon.h"
#include "TamaScene.h"
#include "PixelFont.h"
#include "SigilData.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tama::ui
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTitleH = 28, kMenuY = 28, kMenuH = 26, kStageY = 58, kStageH = 710, kStatusY = 772, kStatusH = 28;
constexpr double kEggCx = 360, kEggCy = 384, kEggA = 200, kEggB = 250;
constexpr double kPivotY = kStageY + 4;
constexpr double kKnobX = 180, kKnobY = -95;
constexpr double kBtnX[3] = {-84, 0, 84}, kBtnY[3] = {172, 190, 172}, kBtnR = 21;

// Direction artistique Onirique : dégradé sang -> noir, rouge braise, blanc os ; l'oeuf, l'écran LCD et la fille-chat gardent leur âme.
constexpr Color kNight = Color::rgb (0x0D0809), kDeep = Color::rgb (0x1A080A), kDeep2 = Color::rgb (0x2A0A0E), kBlood = Color::rgb (0x5C0008);
constexpr Color kEmber = Color::rgb (0xE23444), kBone = Color::rgb (0xFFECEE), kHaze = Color::rgb (0xA88F92), kLine = Color::rgb (0xFFD2D6, 40);
constexpr Color kFace = kDeep2, kHi = kBone, kSoft = kHaze, kLo = kLine, kInk = kBone;
constexpr Color kNavy = Color::rgb (0xA3202F), kSky = kBlood;
constexpr Color kLcdDark = Color::rgb (0x262c0c);

struct ShellSet { Color base, light, dark, rim; };
const ShellSet kShells[3] = {
	{Color::rgb (0xff6fae), Color::rgb (0xffc2dc), Color::rgb (0xc23c78), Color::rgb (0x8c1f52)}, // pink
	{Color::rgb (0x3a8dff), Color::rgb (0xa6cfff), Color::rgb (0x1d55b8), Color::rgb (0x0f3370)}, // blue
	{Color::rgb (0x3a3a46), Color::rgb (0x7c7c92), Color::rgb (0x17171e), Color::rgb (0x07070b)}, // black
};

const char* const kMenuNames[] = {"File", "Preset", "Flavor", "Squeeze", "Compresseur", "Mod\xC3\xA8le", "Engine", "Help"};
constexpr int kNumMenus = 8;

constexpr int kScaleUi = 2;               // échelle de la police pour les menus et les textes
constexpr double kItemH = 22, kSepH = 9, kSliderH = 40;

double clampd (double v, double a, double b) { return std::min (std::max (v, a), b); }
std::string fmt (const char* f, double a) { char b[64]; std::snprintf (b, sizeof (b), f, a); return b; }
std::string fmt2 (const char* f, double a, double c) { char b[96]; std::snprintf (b, sizeof (b), f, a, c); return b; }


// ---- éléments de la direction artistique ----
void drawSigil (Painter& p, bool big, double x, double y, double w, Color c)
{
	const int mw = big ? sigil::kBigW : sigil::kSmallW, mh = big ? sigil::kBigH : sigil::kSmallH;
	const char* const* rows = big ? sigil::kBig : sigil::kSmall;
	const double k = w / mw;
	static const double lv[4] = {0.0, 0.28, 0.62, 1.0};
	for (int j = 0; j < mh; ++j)
	{
		int i = 0;
		while (i < mw)
		{
			const char v = rows[j][i];
			if (v == '0') { ++i; continue; }
			int e = i + 1;
			while (e < mw && rows[j][e] == v) ++e;
			p.rect (x + i * k, y + j * k, (e - i) * k + 0.4, k + 0.4, c.alpha (lv[v - '0']));
			i = e;
		}
	}
}

void drawStar (Painter& p, double cx, double cy, double r, Color c)
{
	std::vector<Pt> v;
	for (int i = 0; i < 8; ++i)
	{
		const double a = kPi * i / 4.0 - kPi / 2.0, rr = (i % 2 == 0) ? r : r * 0.16;
		v.push_back ({cx + rr * std::cos (a), cy + rr * std::sin (a)});
	}
	p.fill (v, c);
}

void spacedText (Painter& p, const std::string& t, double x, double y, int scale, Color c, double gap, bool centre = false)
{
	double wtot = 0;
	for (char ch : t) wtot += font::textWidth (std::string (1, ch), scale) + gap;
	wtot -= gap;
	if (centre) x -= wtot / 2;
	for (char ch : t) { font::drawText (p, std::string (1, ch), x, y, scale, c); x += font::textWidth (std::string (1, ch), scale) + gap; }
}

} // namespace

TamaScene::TamaScene (IUiHost& h) : host (h)
{
	const ShellSet& s = kShells[0];
	shell[0] = s.base; shell[1] = s.light; shell[2] = s.dark; shell[3] = s.rim;
}

double TamaScene::swingAngle () const { return swing * 0.04 * std::sin (swingPhase); }

// ---------------------------------------------------------------------------------------------
// bevels Windows 98
// ---------------------------------------------------------------------------------------------
void TamaScene::raised (Painter& p, double x, double y, double w, double h, bool pressed) const
{
	const double r = 6;
	p.fillLinear (Painter::roundRectShape (x, y, w, h, r), {x, y}, {x, y + h},
	              {{0, pressed ? kDeep : kDeep2.lighten (0.05)}, {1, pressed ? kDeep2 : kDeep}});
	p.stroke (Painter::roundRectShape (x + 0.5, y + 0.5, w - 1, h - 1, r), true, 1, pressed ? kEmber.alpha (0.8) : kLine);
}

void TamaScene::sunken (Painter& p, double x, double y, double w, double h, Color fill) const
{
	p.roundRect (x, y, w, h, 4, fill);
	p.stroke (Painter::roundRectShape (x + 0.5, y + 0.5, w - 1, h - 1, 4), true, 1, kLine);
}

// ---------------------------------------------------------------------------------------------
// mise à jour de l'animation à partir des compteurs du moteur
// ---------------------------------------------------------------------------------------------
void TamaScene::update (double now)
{
	// Licence : relue une fois par seconde. Une activation faite dans ONIRIQUE CENTRAL se voit ici sans rouvrir le plugin.
	if (now - licenseReadMs > 1000.0)
	{
		const auto previous = licenseStatus.state;
		licenseReadMs = now;
		licenseStatus = license.status ();
		if (!licenseShownOnce)
		{
			licenseShownOnce = true;
			if (licenseStatus.state == onirique::license::State::unlicensed) showLicense ();
		}
		else if (dialogOpen && licenseDialog && previous != licenseStatus.state)
		{
			// Le plugin vient d'être activé : la page d'activation n'a plus lieu d'être.
			if (licenseStatus.state == onirique::license::State::licensed) dialogOpen = false; else showLicense ();
		}
	}

	nowCached = now;
	const double dt = lastMs < 0 ? 0.033 : clampd ((now - lastMs) / 1000.0, 0.0, 0.25);
	lastMs = now;
	++frame;
	auto follow = [dt] (double& v, double target, double tauUp, double tauDown) { const double tau = target > v ? tauUp : tauDown; v += (target - v) * (1.0 - std::exp (-dt / tau)); };
	follow (sLamp, host.getMeter (id::kMLamp), 0.05, 0.25);
	follow (sSquash, host.getMeter (id::kMSquash), 0.08, 0.35);
	follow (sGr, host.getMeter (id::kMGr), 0.06, 0.25);
	follow (sDuck, host.getMeter (id::kMDuck), 0.04, 0.2);
	follow (sTau, host.getMeter (id::kMTau), 0.15, 0.15);
	const double lufs = host.getMeter (id::kMLufsIn), peak = host.getMeter (id::kMPeak);
	if (lufs > -69.0 || peak > -59.0) lastSignalMs = now;
	if (host.getMeter (id::kMGlitch) > 0.5 && glitchFrames <= 0) glitchFrames = 2;

	// balancement du Tamagotchi au bout de sa chaîne, sauf sous la souris
	const double target = hoverEgg || dragKnob ? 0.0 : 1.0;
	swing += (target - swing) * (1.0 - std::exp (-dt / 0.25));
	swingPhase += dt * 1.1;
	flip += (flipTarget - flip) * (1.0 - std::exp (-dt / 0.09));
	if (std::fabs (flip - flipTarget) < 0.004) flip = flipTarget;

	const ShellSet& s = kShells[std::clamp (static_cast<int> (host.getPlain (id::kFlavor) + 0.5), 0, 2)];
	const double k = 1.0 - std::exp (-dt / 0.12);
	shell[0] = shell[0].mix (s.base, k); shell[1] = shell[1].mix (s.light, k); shell[2] = shell[2].mix (s.dark, k); shell[3] = shell[3].mix (s.rim, k);

	const bool playing = (now - lastSignalMs) < 1500.0;
	const double bt = clampd ((playing ? 0.4 : 0.15) + 0.6 * sLamp, 0.0, 1.0);
	lcdBright += (bt - lcdBright) * (1.0 - std::exp (-dt / 0.15));
}

// ---------------------------------------------------------------------------------------------
// géométrie de l'oeuf
// ---------------------------------------------------------------------------------------------
std::vector<Pt> TamaScene::eggShape () const
{
	std::vector<Pt> v;
	const int n = 128;
	for (int i = 0; i < n; ++i)
	{
		const double phi = 2 * kPi * i / n, ty = -std::cos (phi);
		v.push_back ({kEggA * std::sin (phi) * (1.0 + 0.18 * ty), kEggB * ty});
	}
	return v;
}

Pt TamaScene::toEggSpace (double x, double y) const
{
	const double a = -swingAngle (), cs = std::cos (a), sn = std::sin (a);
	const double dx = x - 360.0, dy = y - kPivotY;
	const double rx = dx * cs - dy * sn, ry = dx * sn + dy * cs;
	return {rx, ry + kPivotY - kEggCy};
}

bool TamaScene::hitEgg (Pt e) const
{
	const double ty = e.y / kEggB;
	if (std::fabs (ty) >= 1.0) return false;
	return std::fabs (e.x) <= kEggA * std::sqrt (1 - ty * ty) * (1.0 + 0.18 * ty);
}
bool TamaScene::hitKnob (Pt e) const { const double dx = e.x - kKnobX, dy = e.y - kKnobY; return dx * dx + dy * dy <= 40.0 * 40.0; }
bool TamaScene::hitLcd (Pt e) const { return e.x >= -142 && e.x <= 142 && e.y >= -134 && e.y <= 134; }
int TamaScene::hitButton (Pt e) const
{
	for (int i = 0; i < 3; ++i) { const double dx = e.x - kBtnX[i], dy = e.y - kBtnY[i]; if (dx * dx + dy * dy <= (kBtnR + 4) * (kBtnR + 4)) return i; }
	return -1;
}

void TamaScene::pressButton (int i)
{
	const uint32_t pid = i == 0 ? id::kAuto : (i == 1 ? id::kRaw : id::kBypass);
	host.editPlain (pid, host.getPlain (pid) > 0.5 ? 0.0 : 1.0);
}

// ---------------------------------------------------------------------------------------------
// dessin général
// ---------------------------------------------------------------------------------------------
void TamaScene::draw (Painter& p, double now)
{
	update (now);
	drawWindow (p);
	drawTitleBar (p);
	drawMenuBar (p);
	drawStage (p);
	drawEgg (p, now);
	drawReadout (p);
	drawStatus (p);
	drawMenus (p);
	if (dialogOpen) drawDialog (p);
}

void TamaScene::drawWindow (Painter& p)
{
	p.fillLinear (Painter::rectShape (0, 0, kW, kH), {0, 0}, {0, kH}, {{0, Color::rgb (0x3a0509)}, {0.3, Color::rgb (0x1a080a)}, {1, kNight}});
	p.frame (0, 0, kW, kH, kLine);
}

void TamaScene::drawTitleBar (Painter& p)
{
	drawSigil (p, false, 10, 1, 30, kBone);
	spacedText (p, "TAMA-VOX", 50, 7, 2, kBone, 3);
	font::drawText (p, "VOCAL PROCESSOR", 50 + 8 * 20 + 22, 11, 1, kHaze);
	font::drawText (p, "BY OMYLL \xC2\xB7 ABRASION X ONIRIQUE", kW - 12, 11, 1, kHaze, font::Align::Right);
	drawLicensePill (p);
	p.fillLinear (Painter::rectShape (0, kTitleH - 1, kW, 1), {0, 0}, {kW, 0}, {{0, kEmber}, {0.28, kEmber.alpha (0.5)}, {1, kLine}});
}

double TamaScene::menuBarX (int i) const
{
	double x = 6;
	for (int k = 0; k < i; ++k) x += font::textWidth (kMenuNames[k], kScaleUi) + 14;
	return x;
}

int TamaScene::menuBarHit (double x, double y) const
{
	if (y < kMenuY || y >= kMenuY + kMenuH) return -1;
	for (int i = 0; i < kNumMenus; ++i)
	{
		const double x0 = menuBarX (i), w = font::textWidth (kMenuNames[i], kScaleUi) + 14;
		if (x >= x0 && x < x0 + w) return i;
	}
	return -1;
}

void TamaScene::drawMenuBar (Painter& p)
{
	p.rect (0, kMenuY, kW, kMenuH, kNight.alpha (0.55));
	p.rect (0, kMenuY + kMenuH - 1, kW, 1, kLine);
	for (int i = 0; i < kNumMenus; ++i)
	{
		const double x = menuBarX (i), w = font::textWidth (kMenuNames[i], kScaleUi) + 14;
		const bool active = openMenu == i;
		if (active) p.roundRect (x, kMenuY + 3, w, kMenuH - 6, 5, kEmber);
		font::drawText (p, kMenuNames[i], x + 7, kMenuY + 6, kScaleUi, active ? kBone : kHaze);
	}
}

void TamaScene::drawStage (Painter& p)
{
	const double x = 6, y = kStageY, w = kW - 12, h = kStageH;
	const double t = nowCached / 1000.0;
	p.roundRect (x, y, w, h, 10, kNight);
	p.fillRadial (Painter::roundRectShape (x, y, w, h, 10), {360, 300}, 540, {{0, Color::rgb (0x520710)}, {0.5, Color::rgb (0x230a0d)}, {1, kNight}});
	p.stroke (Painter::roundRectShape (x + 0.5, y + 0.5, w - 1, h - 1, 10), true, 1, kLine);
	// rubans rouges ondulants (comme sur le site)
	for (int r = 0; r < 3; ++r)
	{
		std::vector<Pt> v;
		const double base = y + 180 + r * 150, amp = 18 + r * 7, f = 0.011 + r * 0.004, ph = t * (0.35 + r * 0.12) + r * 2.0;
		for (double xx = x; xx <= x + w; xx += 9) v.push_back ({xx, base + amp * std::sin (xx * f + ph) + 0.4 * amp * std::sin (xx * f * 2.3 - ph)});
		p.stroke (v, false, 1.2, kEmber.alpha (0.20 - r * 0.04));
	}
	// réticule autour de l'oeuf
	const double cx = kEggCx, cy = kEggCy, rr = 318;
	for (int i = 0; i < 96; ++i) { const double a = 2 * kPi * i / 96; p.ellipse (cx + rr * std::cos (a), cy + rr * std::sin (a), 1.1, 1.1, kBone.alpha (0.28)); }
	p.ellipseStroke (cx, cy, rr + 14, rr + 14, 1, kBone.alpha (0.10));
	for (int k = 0; k < 4; ++k)
	{
		const double a = kPi * k / 2.0 + kPi / 4.0;
		p.line (cx + (rr - 8) * std::cos (a), cy + (rr - 8) * std::sin (a), cx + (rr + 22) * std::cos (a), cy + (rr + 22) * std::sin (a), 1, kBone.alpha (0.22));
	}
	// étoiles à 4 branches
	const struct { double x, y, r, ph; } st[] = {{40, 96, 6, 0}, {672, 120, 9, 1.3}, {96, 300, 4, 2.1}, {640, 330, 5, 0.6}, {52, 520, 7, 3.0}, {668, 520, 4, 4.1}, {150, 140, 3, 5.2}, {590, 200, 3, 2.7}, {120, 640, 4, 1.9}, {610, 640, 5, 3.6}};
	for (const auto& s : st) drawStar (p, s.x, s.y, s.r * (0.8 + 0.2 * std::sin (t * 1.4 + s.ph)), kBone.alpha (0.55 + 0.25 * std::sin (t * 1.4 + s.ph)));
	// halo de la couleur de la coque
	p.fillRadial (Painter::ellipseShape (cx, cy, 340, 340), {cx, cy}, 340, {{0.3, shell[0].alpha (0.26 + 0.2 * sLamp)}, {1, shell[0].alpha (0.0)}});
	// ombre douce sous l'oeuf (suit le balancement)
	const double sx = 360 + std::sin (swingAngle ()) * 280;
	p.fillRadial (Painter::ellipseShape (sx, 652, 150, 18), {sx, 652}, 150, {{0, Color::rgb (0x000000, 140)}, {1, Color::rgb (0x000000, 0)}});
}

void TamaScene::drawChain (Painter& p)
{
	// repère : origine = point d'accroche (haut de l'établi) ; l'anneau est à y = kEggCy - kEggB - 14 - kPivotY
	const double ringY = kEggCy - kEggB - 18 - kPivotY;
	for (double y = 0; y < ringY - 6; y += 10.5)
	{
		const double c = 4.6;
		p.fillRadial (Painter::ellipseShape (0, y, c, c, 14), {-1.5, y - 1.5}, c * 1.5, {{0, Color::rgb (0xffffff)}, {0.45, Color::rgb (0xc9ced2)}, {1, Color::rgb (0x5b6166)}});
	}
	p.ellipseStroke (0, ringY + 4, 15, 18, 6, Color::rgb (0x4f555a));
	p.ellipseStroke (0, ringY + 4, 15, 18, 3.5, Color::rgb (0xd9dde0));
	p.ellipseStroke (0, ringY + 2, 13, 15, 1.2, Color::rgb (0xffffff, 190));
}

void TamaScene::drawEgg (Painter& p, double now)
{
	(void) now;
	p.save ();
	p.translate (360, kPivotY);
	p.rotate (swingAngle ());
	drawChain (p);
	p.translate (0, kEggCy - kPivotY);
	const double sx = std::cos (kPi * flip);
	const bool back = flip > 0.5;
	p.scale (std::max (0.02, std::fabs (sx)), 1.0);
	if (back) drawEggBack (p); else drawEggFront (p);
	// ombrage pendant la rotation
	const double dark = 1.0 - std::fabs (sx);
	if (dark > 0.01) p.fill (eggShape (), Color::rgb (0x000000, static_cast<uint8_t> (dark * 130)));
	p.restore ();
}

void TamaScene::drawEggFront (Painter& p)
{
	const auto shape = eggShape ();
	// coque : dégradé, reflet, assombrissement des bords
	p.fillLinear (shape, {-160, -230}, {180, 240}, {{0, shell[1]}, {0.42, shell[0]}, {1, shell[2]}});
	p.fillRadial (shape, {-90, -150}, 250, {{0, Color::rgb (0xffffff, 120)}, {0.45, Color::rgb (0xffffff, 26)}, {1, Color::rgb (0xffffff, 0)}});
	p.fillRadial (shape, {0, 10}, 290, {{0, Color::rgb (0x000000, 0)}, {0.72, Color::rgb (0x000000, 0)}, {1, Color::rgb (0x000000, 95)}});
	p.stroke (shape, true, 7, shell[3]);
	{
		std::vector<Pt> inner; for (const Pt& q : shape) inner.push_back ({q.x * 0.965, q.y * 0.97});
		p.stroke (inner, true, 2, Color::rgb (0xffffff, 55));
	}
	drawCrown (p);

	// cadre de l'écran
	p.roundRect (-150, -142, 300, 284, 26, Color::rgb (0x000000, 70));
	p.fillLinear (Painter::roundRectShape (-144, -136, 288, 272, 20), {-140, -130}, {140, 130}, {{0, Color::rgb (0x3a0d12)}, {1, Color::rgb (0x0d0809)}});
	p.stroke (Painter::roundRectShape (-144, -136, 288, 272, 20), true, 2, kEmber.alpha (0.55));

	// LCD 64x60 à l'échelle 4
	const double lx = -128, ly = -120, S = 4;
	LcdState ls;
	const double now = nowCached;
	const bool playing = (now - lastSignalMs) < 1500.0;
	ls.lamp = sLamp; ls.squash = sSquash; ls.grDb = sGr; ls.duckDb = sDuck;
	ls.playing = playing;
	ls.raw = host.getPlain (id::kRaw) > 0.5 || host.getPlain (id::kBypass) > 0.5;
	ls.autoGain = host.getPlain (id::kAuto) > 0.5;
	ls.pure = host.getPlain (id::kMode) > 0.5;
	ls.tooLoud = host.getMeter (id::kMPeak) > -1.5;
	if (glitchFrames > 0) { ls.glitch = 1; --glitchFrames; }
	uint8_t buf[kLcdW * kLcdH];
	renderLcd (buf, ls, now, frame * 2654435761u + 12345u);
	const Color bg = Color::rgb (0x76802f).mix (Color::rgb (0xc9d661), lcdBright).mix (Color::rgb (0xe8c860), sLamp * 0.25);
	const Color pal[4] = {bg, kLcdDark, kLcdDark.alpha (0.14), kLcdDark.alpha (0.40)};
	p.rect (lx, ly, kLcdW * S, kLcdH * S, bg);
	for (int y = 0; y < kLcdH; ++y)
	{
		int x = 0;
		while (x < kLcdW)
		{
			const uint8_t v = buf[y * kLcdW + x];
			if (v == kLcdBg) { ++x; continue; }
			int k = x + 1;
			while (k < kLcdW && buf[y * kLcdW + k] == v) ++k;
			p.rect (lx + x * S, ly + y * S, (k - x) * S, S, pal[v]);
			x = k;
		}
	}
	// halo orange de la lampe et reflet de la vitre
	if (sLamp > 0.02) p.fillRadial (Painter::rectShape (lx, ly, kLcdW * S, kLcdH * S), {lx + 40, ly + 70}, 220, {{0, Color::rgb (0xff8a1f, static_cast<uint8_t> (sLamp * 90))}, {1, Color::rgb (0xff8a1f, 0)}});
	p.fill ({{lx, ly}, {lx + 150, ly}, {lx, ly + 150}}, Color::rgb (0xffffff, 22));
	p.rect (lx, ly, kLcdW * S, 2, Color::rgb (0x000000, 60));
	p.rect (lx, ly, 2, kLcdH * S, Color::rgb (0x000000, 40));

	// boutons A / B / C
	const char* letters[3] = {"A", "B", "C"};
	const char* tags[3] = {"AUTO", "RAW", "BYPASS"};
	const bool on[3] = {host.getPlain (id::kAuto) > 0.5, host.getPlain (id::kRaw) > 0.5, host.getPlain (id::kBypass) > 0.5};
	for (int i = 0; i < 3; ++i)
	{
		const double bx = kBtnX[i], by = kBtnY[i];
		const bool down = on[i] || pressedButton == i;
		p.ellipse (bx, by + 2, kBtnR + 5, kBtnR + 5, Color::rgb (0x000000, 70));
		p.ellipse (bx, by, kBtnR + 4, kBtnR + 4, Color::rgb (0x1a080a));
		if (on[i]) p.fillRadial (Painter::ellipseShape (bx, by, kBtnR + 16, kBtnR + 16), {bx, by}, kBtnR + 16, {{0.4, kEmber.alpha (0.55)}, {1, kEmber.alpha (0.0)}});
		const double off = down ? 2.0 : 0.0;
		Color c0 = Color::rgb (0xff7a86), c1 = Color::rgb (0xE23444), c2 = Color::rgb (0x70101a);
		if (!on[i]) { c0 = Color::rgb (0x6a3036); c1 = Color::rgb (0x3a1519); c2 = Color::rgb (0x1a080a); }
		if (hoverButton == i) { c0 = c0.lighten (0.2); c1 = c1.lighten (0.15); }
		if (down) { c0 = c0.darken (0.15); c1 = c1.darken (0.2); c2 = c2.darken (0.2); }
		p.fillRadial (Painter::ellipseShape (bx, by + off, kBtnR, kBtnR), {bx - 6, by + off - 7}, kBtnR * 1.7, {{0, c0}, {0.5, c1}, {1, c2}});
		p.ellipseStroke (bx, by + off, kBtnR - 1, kBtnR - 1, 1.2, Color::rgb (0xffffff, 60));
		font::drawText (p, letters[i], bx, by + off - 10, 3, on[i] ? kBone : kHaze, font::Align::Centre);
		font::drawText (p, tags[i], bx, by + kBtnR + 10, 1, shell[3].alpha (0.9), font::Align::Centre);
	}
	// reflet de la coque
	p.save (); p.translate (-62, -184); p.rotate (-0.55); p.fillRadial (Painter::ellipseShape (0, 0, 46, 15), {0, 0}, 46, {{0, Color::rgb (0xffffff, 170)}, {1, Color::rgb (0xffffff, 0)}}); p.restore ();
}

void TamaScene::drawCrown (Painter& p)
{
	const double norm = host.getNorm (id::kDrive);
	p.save ();
	p.translate (kKnobX, kKnobY);
	p.rotate (0.38);
	p.scale (1.18, 1.18);
	p.ellipse (4, 7, 33, 37, Color::rgb (0x000000, 80));
	// collier sombre
	p.fillLinear (Painter::ellipseShape (0, 0, 31, 35), {-30, -35}, {30, 35}, {{0, Color::rgb (0x3a3340)}, {1, Color::rgb (0x0b0910)}});
	// corps cranté
	std::vector<Pt> kn;
	for (int i = 0; i < 104; ++i)
	{
		const double a = 2 * kPi * i / 104, f = 1.0 - 0.045 * (0.5 + 0.5 * std::cos (26 * a));
		kn.push_back ({27 * std::cos (a) * f, 32 * std::sin (a) * f});
	}
	Color k0 = Color::rgb (0x4a2a2e), k1 = Color::rgb (0x1d0d10);
	if (hoverKnob || dragKnob) { k0 = k0.lighten (0.15); k1 = k1.lighten (0.1); }
	p.fillLinear (kn, {-26, -30}, {26, 30}, {{0, k0}, {1, k1}});
	p.stroke (kn, true, 1.5, Color::rgb (0x0d0805));
	// face graduée
	p.fillLinear (Painter::ellipseShape (0, 0, 22, 26), {-20, -24}, {20, 24}, {{0, Color::rgb (0x2a1418)}, {1, Color::rgb (0x120708)}});
	for (int i = 0; i < 21; ++i)
	{
		const double a = (-135.0 + i * 13.5) * kPi / 180.0, big = (i % 2 == 0) ? 1 : 0;
		const double r0 = big ? 15.5 : 18.0, r1 = 21.0;
		p.line (std::sin (a) * r0 * 0.84, -std::cos (a) * r0, std::sin (a) * r1 * 0.84, -std::cos (a) * r1, big ? 2.6 : 1.4, (-135.0 + i * 13.5) <= (-135.0 + 270.0 * norm) + 0.01 ? kEmber : kBone.alpha (0.35));
	}
	const double a0 = -135.0 * kPi / 180.0;
	p.ellipse (std::sin (a0) * 12.5 * 0.84, -std::cos (a0) * 12.5, 2.3, 2.6, kEmber);
	// pointeur
	const double a = (-135.0 + 270.0 * norm) * kPi / 180.0;
	p.line (0, 0, std::sin (a) * 14 * 0.84, -std::cos (a) * 14, 3.4, kBone);
	p.ellipse (0, 0, 4.2, 4.8, Color::rgb (0xc9ced2));
	p.restore ();
}

void TamaScene::drawEggBack (Painter& p)
{
	const auto shape = eggShape ();
	p.fillLinear (shape, {160, -230}, {-180, 240}, {{0, shell[1]}, {0.42, shell[0]}, {1, shell[2]}});
	p.fillRadial (shape, {90, -150}, 250, {{0, Color::rgb (0xffffff, 90)}, {1, Color::rgb (0xffffff, 0)}});
	p.fillRadial (shape, {0, 10}, 290, {{0, Color::rgb (0x000000, 0)}, {0.72, Color::rgb (0x000000, 0)}, {1, Color::rgb (0x000000, 95)}});
	p.stroke (shape, true, 7, shell[3]);
	// plaque gravée
	p.roundRect (-132, -150, 264, 300, 24, shell[3].alpha (0.28));
	p.fillLinear (Painter::roundRectShape (-128, -156, 256, 300, 22), {-120, -150}, {120, 140}, {{0, shell[2].darken (0.15)}, {1, shell[2].darken (0.4)}});
	const Color ink = Color::rgb (0xf3ead7);
	spacedText (p, "BY OMYLL", 0, -138, 4, ink, 4, true);
	p.rect (-110, -110, 220, 2, ink.alpha (0.55));
	// sigil Onirique (logo officiel) + signature
	drawSigil (p, true, -85, -104, 170, kBone);
	spacedText (p, "ABRASION X ONIRIQUE", 0, 58, 2, ink, 2, true);
	spacedText (p, "TAMA-VOX", 0, 82, 3, ink, 5, true);
	font::drawText (p, "VST3 \xC2\xB7 AAX \xC2\xB7 8x OVERSAMPLING", 0, 110, 1, ink.alpha (0.8), font::Align::Centre);
	// vis et trou de reset
	for (double sxp : {-100.0, 100.0}) { p.ellipse (sxp, 124, 6, 6, Color::rgb (0x000000, 90)); p.ellipse (sxp, 123, 5, 5, Color::rgb (0xc9ced2)); p.line (sxp - 3, 123, sxp + 3, 123, 1.4, Color::rgb (0x4f555a)); }
	font::drawText (p, "double-clic : retour", 0, 176, 1, shell[3].alpha (0.9), font::Align::Centre);
}

void TamaScene::drawReadout (Painter& p)
{
	const double x = 80, y = 654, w = 560, h = 84;
	p.roundRect (x - 5, y - 5, w + 10, h + 10, 12, kNight);
	p.stroke (Painter::roundRectShape (x - 4.5, y - 4.5, w + 9, h + 9, 12), true, 1, kEmber.alpha (0.5));
	p.roundRect (x - 3, y - 3, w + 6, h + 6, 10, Color::rgb (0x2a1418));
	const Color bg = Color::rgb (0x76802f).mix (Color::rgb (0xc9d661), lcdBright);
	p.roundRect (x, y, w, h, 6, bg);
	const bool playing = (nowCached - lastSignalMs) < 1500.0;
	const double drive = host.getPlain (id::kDrive), sq = host.getPlain (id::kSqueeze);
	const bool raw = host.getPlain (id::kRaw) > 0.5 || host.getPlain (id::kBypass) > 0.5;
	const bool autoG = host.getPlain (id::kAuto) > 0.5;
	const double mk = host.getMeter (id::kMMakeup);
	std::string vals[6] = {fmt ("%.1f", drive), fmt ("%.0f", sq), playing ? fmt ("%.0f ms", sTau) : "---",
	                       fmt ("%.1f dB", (raw || !playing) ? 0.0 : sGr),
	                       playing ? fmt2 ("%.1f / %.1f", host.getMeter (id::kMLufsIn), host.getMeter (id::kMLufsOut) + (autoG ? mk : 0.0)) : "--- / ---",
	                       (autoG && playing) ? fmt ("%+.1f dB", mk) : "+0.0 dB"};
	const char* labels[6] = {"FLAVOR DRIVE", "SQUEEZE", "RELEASE T4", "GAIN REDUCTION", "LUFS IN / OUT", "AUTO GAIN"};
	const double cw = (w - 20) / 3.0;
	for (int i = 0; i < 6; ++i)
	{
		const double cx = x + 10 + (i % 3) * cw, cy = y + 8 + (i / 3) * 38;
		font::drawText (p, labels[i], cx, cy, 1, kLcdDark.alpha (0.7));
		font::drawText (p, vals[i], cx, cy + 11, 2, kLcdDark);
	}
	p.rect (x, y, w, 2, Color::rgb (0x000000, 40));
}

void TamaScene::drawStatus (Painter& p)
{
	const double y = kStatusY;
	const bool playing = (nowCached - lastSignalMs) < 1500.0;
	const bool pure = host.getPlain (id::kMode) > 0.5;
	const double drive = host.getPlain (id::kDrive);
	std::string st = "DELICIOUS";
	const bool raw = host.getPlain (id::kRaw) > 0.5, byp = host.getPlain (id::kBypass) > 0.5;
	if (!playing) st = "SLEEPING"; else if (byp) st = "BYPASSED"; else if (raw) st = "SAD (RAW)"; else if (host.getMeter (id::kMPeak) > -1.5) st = "TOO LOUD!";
	else if (sSquash > 0.6) st = "SQUEEZED!"; else if (sDuck > 3.0) st = "SSSS..."; else if (sLamp > 0.6) st = "CRUNCHY";
	const double volt = 9.1 - drive * 0.03 - (playing ? clampd ((host.getMeter (id::kMPeak) + 30) / 30, 0, 1) * 0.25 : 0);
	const std::string cells[3] = {std::string ("LATENCY: ") + (pure ? "64" : "0") + " SMP (" + (pure ? "PURE" : "SNAP") + ")", fmt ("VOLT: %.1fV", volt), "STATUS: " + st};
	double x = 6;
	for (int i = 0; i < 3; ++i)
	{
		const double w = i < 2 ? font::textWidth (cells[i], kScaleUi) + 34.0 : (kW - 6) - x;
		p.roundRect (x, y, w, kStatusH - 8, 5, kNight.alpha (0.7));
		p.stroke (Painter::roundRectShape (x + 0.5, y + 0.5, w - 1, kStatusH - 9, 5), true, 1, kLine);
		p.ellipse (x + 12, y + 10, 3, 3, (i == 2 && playing) ? kEmber : kHaze.alpha (0.5));
		if (i == 2 && playing) p.fillRadial (Painter::ellipseShape (x + 12, y + 10, 10, 10), {x + 12, y + 10}, 10, {{0, kEmber.alpha (0.5)}, {1, kEmber.alpha (0.0)}});
		font::drawText (p, cells[i], x + 24, y + 4, kScaleUi, i == 2 ? kBone : kHaze);
		x += w + 4;
	}
}

// ---------------------------------------------------------------------------------------------
// presets
// ---------------------------------------------------------------------------------------------
bool TamaScene::presetMatches (int i) const
{
	const FactoryPreset& f = kFactoryPresets[static_cast<size_t> (i)];
	return std::fabs (host.getPlain (id::kDrive) - f.drive) < 0.05 && static_cast<int> (host.getPlain (id::kFlavor) + 0.5) == f.flavor
	    && std::fabs (host.getPlain (id::kSqueeze) - f.squeeze) < 0.5 && static_cast<int> (host.getPlain (id::kMode) + 0.5) == f.mode
	    && static_cast<int> (host.getPlain (id::kModel) + 0.5) == f.model;
}
void TamaScene::presetEdit (int i) { host.applyPreset (i); }

// ---------------------------------------------------------------------------------------------
// menus
// ---------------------------------------------------------------------------------------------
std::vector<TamaScene::Item> TamaScene::buildMenu (int index) const
{
	std::vector<Item> v;
	auto action = [&v] (std::string l, std::function<void ()> f) { Item i; i.kind = Item::Action; i.label = std::move (l); i.run = std::move (f); v.push_back (std::move (i)); };
	auto check = [&v] (std::string l, bool on, std::function<void ()> f) { Item i; i.kind = Item::Check; i.label = std::move (l); i.on = on; i.run = std::move (f); v.push_back (std::move (i)); };
	auto radio = [&v] (std::string l, bool on, std::function<void ()> f) { Item i; i.kind = Item::Radio; i.label = std::move (l); i.on = on; i.run = std::move (f); v.push_back (std::move (i)); };
	auto slider = [&v] (std::string l, uint32_t pid, double step) { Item i; i.kind = Item::Slider; i.label = std::move (l); i.pid = pid; i.step = step; v.push_back (std::move (i)); };
	auto sep = [&v] { Item i; i.kind = Item::Sep; v.push_back (i); };
	IUiHost* h = &host;
	TamaScene* self = const_cast<TamaScene*> (this);
	auto setPlain = [h] (uint32_t pid, double val) { return [h, pid, val] { h->editPlain (pid, val); }; };

	switch (index)
	{
		case 0: // File
			action ("R\xC3\xA9initialiser les r\xC3\xA9glages", [h] { h->resetAll (); });
			action ("R\xC3\xA9initialiser le compresseur", [h] { h->resetCompressor (); });
			break;
		case 1: // Preset
			for (size_t i = 0; i < kFactoryPresets.size (); ++i)
				radio (kFactoryPresets[i].name, presetMatches (static_cast<int> (i)), [self, i] { self->presetEdit (static_cast<int> (i)); });
			break;
		case 2: // Flavor
		{
			const int fl = static_cast<int> (host.getPlain (id::kFlavor) + 0.5);
			for (int i = 0; i < 3; ++i) radio (kFlavorLong[i], fl == i, setPlain (id::kFlavor, i));
			sep ();
			slider ("Flavor drive", id::kDrive, 0.1);
			break;
		}
		case 3: // Squeeze
		{
			slider ("Squeeze", id::kSqueeze, 1.0);
			sep ();
			const double sq = host.getPlain (id::kSqueeze);
			const struct { const char* n; double v; } pr[4] = {{"Doux \xC2\xB7 20", 20}, {"Moyen \xC2\xB7 45", 45}, {"Fort \xC2\xB7 70", 70}, {"\xC3\x89" "cras\xC3\xA9 \xC2\xB7 100", 100}};
			for (auto& q : pr) radio (q.n, std::fabs (sq - q.v) < 0.5, setPlain (id::kSqueeze, q.v));
			break;
		}
		case 4: // Compresseur
			slider ("Threshold (d\xC3\xA9" "calage)", id::kThr, 0.5);
			slider ("Ratio (d\xC3\xA9" "calage)", id::kRatio, 0.1);
			slider ("Knee (d\xC3\xA9" "calage)", id::kKnee, 1.0);
			slider ("Attaque", id::kAttack, 0.1);
			sep ();
			slider ("Couleur : saturation", id::kColorK, 0.05);
			slider ("Couleur : paire/impaire", id::kColorB, 0.01);
			slider ("Graves", id::kLo, 0.5);
			slider ("M\xC3\xA9" "diums", id::kMid, 0.5);
			slider ("Aigus", id::kHi, 0.5);
			sep ();
			slider ("Release adaptatif T4", id::kAdapt, 1.0);
			slider ("Release mini", id::kRelMin, 5.0);
			slider ("Release maxi", id::kRelMax, 10.0);
			slider ("M\xC3\xA9moire d'\xC3\xA9nergie", id::kMemory, 10.0);
			sep ();
			for (size_t i = 0; i < kCompPresets.size (); ++i)
				action (std::string ("Preset : ") + kCompPresets[i].name, [h, i] { h->applyCompPreset (static_cast<int> (i)); });
			action ("R\xC3\xA9initialiser le compresseur", [h] { h->resetCompressor (); });
			break;
		case 5: // Modèle
		{
			const int m = static_cast<int> (host.getPlain (id::kModel) + 0.5);
			for (size_t i = 0; i < kModels.size (); ++i) radio (kModels[i].name, m == static_cast<int> (i), [h, i] { h->applyModel (static_cast<int> (i)); });
			sep ();
			action ("Fiche du mod\xC3\xA8le actuel...", [self, m] { self->showDialog (kModels[static_cast<size_t> (m)].name, kModels[static_cast<size_t> (m)].info); });
			break;
		}
		case 6: // Engine
		{
			const bool pure = host.getPlain (id::kMode) > 0.5;
			radio (kModeLong[0], !pure, setPlain (id::kMode, 0));
			radio (kModeLong[1], pure, setPlain (id::kMode, 1));
			sep ();
			check ("Auto-gain LUFS", host.getPlain (id::kAuto) > 0.5, [h] { h->editPlain (id::kAuto, h->getPlain (id::kAuto) > 0.5 ? 0.0 : 1.0); });
			check ("\xC3\x89" "coute RAW (comparaison)", host.getPlain (id::kRaw) > 0.5, [h] { h->editPlain (id::kRaw, h->getPlain (id::kRaw) > 0.5 ? 0.0 : 1.0); });
			check ("Bypass", host.getPlain (id::kBypass) > 0.5, [h] { h->editPlain (id::kBypass, h->getPlain (id::kBypass) > 0.5 ? 0.0 : 1.0); });
			break;
		}
		case 7: // Help
			action ("TAMA-VOX by Omyll \xC2\xB7 ABRASION X ONIRIQUE", [self] {
				self->showDialog ("TAMA-VOX", "TAMA-VOX by Omyll\nABRASION X ONIRIQUE\n\nVST3 \xC2\xB7 AAX\nSur\xC3\xA9" "chantillonnage 8x, transformateur WDF, opto T4 adaptatif, saturation ADAA ordre 2, air shelf et de-esser intelligents, auto-gain LUFS.");
			});
			sep ();
			action ("Retourner le Tamagotchi", [self] { self->flipTarget = self->flipTarget > 0.5 ? 0.0 : 1.0; });
			action ("Raccourcis souris...", [self] {
				self->showDialog ("Raccourcis", "Molette de l'\xC5\x93uf : glisse verticalement, ou utilise la molette de la souris. Double-clic : r\xC3\xA9glage par d\xC3\xA9" "faut (5.0).\n\nBoutons A / B / C : auto-gain, \xC3\xA9" "coute RAW, bypass.\n\nMolette de la souris sur l'\xC3\xA9" "cran : Squeeze. Maj = r\xC3\xA9glage fin.\n\nDouble-clic sur l'\xC5\x93uf : le retourner.");
			});
			break;
		default: break;
	}
	return v;
}

TamaScene::MenuGeom TamaScene::menuGeom (int index, const std::vector<Item>& items) const
{
	MenuGeom g;
	double w = 300;
	for (const Item& it : items) if (it.kind != Item::Sep) w = std::max (w, static_cast<double> (font::textWidth (it.label, kScaleUi)) + (it.kind == Item::Slider ? 190 : 70));
	double h = 6;
	for (const Item& it : items)
	{
		const double ih = it.kind == Item::Sep ? kSepH : (it.kind == Item::Slider ? kSliderH : kItemH);
		g.itemY.push_back (h); g.itemH.push_back (ih); h += ih;
	}
	g.w = w; g.h = h + 3;
	g.x = std::min (menuBarX (index), static_cast<double> (kW) - w - 6);
	g.y = kMenuY + kMenuH;
	return g;
}

std::string TamaScene::sliderText (const Item& it) const
{
	char b[96];
	formatPlain (it.pid, host.getPlain (it.pid), b, sizeof (b));
	return b;
}

void TamaScene::setSliderFromX (const Item& it, const MenuGeom& g, double x)
{
	const double tx = g.x + 24, tw = g.w - 48;
	double n = clampd ((x - tx) / tw, 0.0, 1.0);
	const ParamSpec& s = spec (it.pid);
	if (it.step > 0)
	{
		double pv = toPlain (s, n);
		pv = std::round (pv / it.step) * it.step;
		n = toNorm (s, clampd (pv, s.min, s.max));
	}
	host.uiSet (it.pid, n);
}

void TamaScene::drawMenus (Painter& p)
{
	if (openMenu < 0) return;
	const auto items = buildMenu (openMenu);
	const MenuGeom g = menuGeom (openMenu, items);
	// ombre portée + cadre
	p.roundRect (g.x + 3, g.y + 5, g.w, g.h, 8, Color::rgb (0x000000, 110));
	p.roundRect (g.x, g.y, g.w, g.h, 6, kNight);
	raised (p, g.x, g.y, g.w, g.h);
	for (size_t i = 0; i < items.size (); ++i)
	{
		const Item& it = items[i];
		const double iy = g.y + g.itemY[i], ih = g.itemH[i];
		const bool hov = static_cast<int> (i) == hoverItem && it.kind != Item::Sep && it.kind != Item::Slider;
		if (it.kind == Item::Sep)
		{
			p.rect (g.x + 8, iy + 4, g.w - 16, 1, kLine);
			continue;
		}
		if (hov) p.roundRect (g.x + 4, iy, g.w - 8, ih, 4, kNavy);
		const Color tc = hov ? kBone : kBone.alpha (0.86);
		if (it.kind == Item::Slider)
		{
			font::drawText (p, it.label, g.x + 24, iy + 3, kScaleUi, kInk);
			font::drawText (p, sliderText (it), g.x + g.w - 24, iy + 3, kScaleUi, kInk, font::Align::Right);
			const double tx = g.x + 24, tw = g.w - 48, ty = iy + 25;
			sunken (p, tx, ty, tw, 8, kNight);
			const double nv = host.getNorm (it.pid), thumbX = tx + nv * tw;
			p.roundRect (tx + 1, ty + 1, std::max (0.0, thumbX - tx - 1), 6, 3, kEmber.alpha (0.8));
			p.roundRect (thumbX - 5, ty - 5, 11, 18, 4, dragSlider == static_cast<int> (i) ? kEmber.lighten (0.2) : kBone);
			continue;
		}
		font::drawText (p, it.label, g.x + 28, iy + 4, kScaleUi, tc);
		if (it.kind == Item::Radio && it.on) p.ellipse (g.x + 15, iy + 11, 3.5, 3.5, tc);
		if (it.kind == Item::Check && it.on)
			p.stroke ({{g.x + 9, iy + 11}, {g.x + 13, iy + 15}, {g.x + 20, iy + 6}}, false, 2.2, tc);
	}
	// titre du menu actif surligné
	const double bx = menuBarX (openMenu), bw = font::textWidth (kMenuNames[openMenu], kScaleUi) + 14;
	p.roundRect (bx, kMenuY + 3, bw, kMenuH - 6, 5, kEmber);
	font::drawText (p, kMenuNames[openMenu], bx + 7, kMenuY + 6, kScaleUi, kBone);
}

// ---------------------------------------------------------------------------------------------
// boîte de dialogue
// ---------------------------------------------------------------------------------------------
void TamaScene::showDialog (const std::string& title, const std::string& text)
{
	dialogOpen = true; dialogTitle = title; dialogText = font::wrap (text, 40); openMenu = -1;
	licenseDialog = false;
}

// ---------------------------------------------------------------------------------------------
// licence ONIRIQUE
// ---------------------------------------------------------------------------------------------
std::string TamaScene::pillLabel () const
{
	using onirique::license::State;
	if (licenseStatus.state == State::licensed) return "ACTIV\xC3\x89";
	if (licenseStatus.state == State::trial) return "ESSAI \xC2\xB7 " + std::to_string (licenseStatus.daysLeft ()) + " J \xC2\xB7 ACTIVATION";
	return "MODE D\xC3\x89MO \xC2\xB7 ACTIVATION";
}
TamaScene::PillGeom TamaScene::pillGeom () const
{
	// entre « VOCAL PROCESSOR » et la signature, dans la barre de titre
	const double w = font::textWidth (pillLabel (), 1) + 30.0;
	return {std::floor (430.0 - w / 2), 5, w, 18};
}
bool TamaScene::hitPill (double x, double y) const
{
	const PillGeom g = pillGeom ();
	return x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h;
}
void TamaScene::drawLicensePill (Painter& p)
{
	const bool active = licenseStatus.state == onirique::license::State::licensed;
	const Color tint = active ? Color::rgb (0x6FDC9C) : kEmber;
	const PillGeom g = pillGeom ();
	p.roundRect (g.x, g.y, g.w, g.h, g.h / 2, active ? kNight.alpha (0.6) : tint.alpha (0.2));
	p.stroke (Painter::roundRectShape (g.x + 0.5, g.y + 0.5, g.w - 1, g.h - 1, g.h / 2), true, 1, tint.alpha (active ? 0.45 : 0.8));
	p.ellipse (g.x + 11, g.y + g.h / 2, 4.5, 4.5, tint.alpha (0.35));
	p.ellipse (g.x + 11, g.y + g.h / 2, 2.5, 2.5, tint);
	font::drawText (p, pillLabel (), g.x + 21, g.y + 6, 1, active ? kBone.alpha (0.85) : kBone);
}
void TamaScene::showLicense ()
{
	using onirique::license::State;
	const std::string account = licenseStatus.email.empty () ? std::string () : " sur le compte " + licenseStatus.email;
	if (licenseStatus.state == State::licensed)
		showDialog ("PLUGIN ACTIV\xC3\x89", "TAMA-VOX est activ\xC3\xA9 sur cette machine" + account + ".\n\nVos licences et vos machines activ\xC3\xA9" "es se g\xC3\xA8rent dans ONIRIQUE CENTRAL.");
	else if (licenseStatus.state == State::trial)
		showDialog ("ESSAI GRATUIT \xC2\xB7 " + std::to_string (licenseStatus.daysLeft ()) + " J",
		            "Essai de TAMA-VOX en cours" + account + ".\n\nLe plugin fonctionne normalement en temps r\xC3\xA9" "el. Tout export reste remplac\xC3\xA9 par un bruit blanc tant que le plugin n'est pas achet\xC3\xA9.\n\n"
		            "Achetez-le sur onirique.store ou dans ONIRIQUE CENTRAL : l'achat est pris en compte ici en quelques secondes.");
	else
		showDialog ("ACTIVATION REQUISE",
		            "TAMA-VOX n'est pas activ\xC3\xA9 sur cette machine.\n\nSans licence, l'\xC3\xA9" "coute est coup\xC3\xA9" "e par un bruit blanc toutes les trente secondes et tout export est remplac\xC3\xA9 par ce bruit.\n\n"
		            "Ouvrez ONIRIQUE CENTRAL, connectez-vous \xC3\xA0 votre compte, puis activez le plugin ou lancez son essai gratuit de 7 jours. L'activation est prise en compte ici en quelques secondes.");
	licenseDialog = true;
}

TamaScene::DialogGeom TamaScene::dialogGeom () const
{
	DialogGeom g;
	const double tw = std::max (260, font::textWidth (dialogText, kScaleUi));
	g.w = std::min (static_cast<double> (kW) - 40, tw + 70);
	const double th = font::textLines (dialogText) * font::kLineHeight * kScaleUi;
	g.h = 28 + 26 + th + 20 + 30 + 18;
	g.x = std::floor ((kW - g.w) / 2); g.y = std::floor (150);
	g.okW = 96; g.okH = 28; g.okX = g.x + g.w / 2 - g.okW / 2; g.okY = g.y + g.h - g.okH - 14;
	g.centralX = 0; g.centralW = 0;
	if (licenseDialog)
	{
		// page de la licence : « ONIRIQUE CENTRAL » à gauche de « OK »
		g.centralW = 196;
		g.centralX = g.x + g.w / 2 - (g.centralW + 12 + g.okW) / 2;
		g.okX = g.centralX + g.centralW + 12;
	}
	return g;
}

void TamaScene::drawDialog (Painter& p)
{
	p.rect (0, 0, kW, kH, Color::rgb (0x000000, 70));
	const DialogGeom g = dialogGeom ();
	p.roundRect (g.x + 4, g.y + 6, g.w, g.h, 8, Color::rgb (0x000000, 120));
	p.roundRect (g.x, g.y, g.w, g.h, 6, kNight);
	raised (p, g.x, g.y, g.w, g.h);
	p.fillLinear (Painter::rectShape (g.x + 3, g.y + 3, g.w - 6, 20), {g.x, g.y}, {g.x + g.w, g.y}, {{0, kEmber}, {1, kBlood}});
	font::drawText (p, dialogTitle, g.x + 8, g.y + 6, kScaleUi, kHi);
	font::drawText (p, dialogTitle, g.x + 9, g.y + 6, kScaleUi, kHi);
	// icône (i)
	const double ix = g.x + 28, iy = g.y + 52;
	p.ellipse (ix, iy, 13, 13, kBlood); p.ellipse (ix - 1, iy - 1, 12, 12, kEmber);
	font::drawText (p, "i", ix, iy - 7, 2, kHi, font::Align::Centre);
	font::drawText (p, dialogText, g.x + 54, g.y + 40, kScaleUi, kInk);
	raised (p, g.okX, g.okY, g.okW, g.okH, pressedTitleBtn == 100);
	font::drawText (p, "OK", g.okX + g.okW / 2, g.okY + 7, kScaleUi, kInk, font::Align::Centre);
	if (licenseDialog)
	{
		raised (p, g.centralX, g.okY, g.centralW, g.okH, pressedTitleBtn == 101);
		font::drawText (p, "ONIRIQUE CENTRAL", g.centralX + g.centralW / 2, g.okY + 7, kScaleUi, kInk, font::Align::Centre);
	}
	
}

// ---------------------------------------------------------------------------------------------
// événements
// ---------------------------------------------------------------------------------------------
void TamaScene::updateCursor (double x, double y)
{
	cur = CursorKind::Arrow;
	if (dialogOpen)
	{
		const DialogGeom g = dialogGeom ();
		const bool onRow = y >= g.okY && y < g.okY + g.okH;
		if (onRow && ((x >= g.okX && x < g.okX + g.okW) || (licenseDialog && x >= g.centralX && x < g.centralX + g.centralW))) cur = CursorKind::Hand;
		return;
	}
	if (openMenu < 0 && hitPill (x, y)) { cur = CursorKind::Hand; return; }
	if (openMenu >= 0)
	{
		const auto items = buildMenu (openMenu);
		const MenuGeom g = menuGeom (openMenu, items);
		if (x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h)
			for (size_t i = 0; i < items.size (); ++i)
				if (y >= g.y + g.itemY[i] && y < g.y + g.itemY[i] + g.itemH[i]) cur = items[i].kind == Item::Slider ? CursorKind::ResizeH : (items[i].kind == Item::Sep ? CursorKind::Arrow : CursorKind::Hand);
		return;
	}
	if (menuBarHit (x, y) >= 0) { cur = CursorKind::Hand; return; }
	if (flip > 0.97) { if (hitEgg (toEggSpace (x, y))) cur = CursorKind::Hand; return; }
	if (flip < 0.03)
	{
		const Pt e = toEggSpace (x, y);
		if (hitKnob (e)) cur = CursorKind::ResizeV; else if (hitButton (e) >= 0) cur = CursorKind::Hand;
	}
}

void TamaScene::mouseDown (double x, double y, int button, int mods, int clicks)
{
	(void) mods; (void) button;
	mx = x; my = y;
	if (dialogOpen)
	{
		const DialogGeom g = dialogGeom ();
		if (x >= g.okX && x < g.okX + g.okW && y >= g.okY && y < g.okY + g.okH) pressedTitleBtn = 100;
		else if (licenseDialog && x >= g.centralX && x < g.centralX + g.centralW && y >= g.okY && y < g.okY + g.okH) pressedTitleBtn = 101;
		return;
	}
	// pastille de la licence : la licence est relue tout de suite, puis sa page s'ouvre
	if (openMenu < 0 && hitPill (x, y))
	{
		license.refresh ();
		licenseStatus = license.status ();
		showLicense ();
		return;
	}
	// menus
	if (openMenu >= 0)
	{
		const auto items = buildMenu (openMenu);
		const MenuGeom g = menuGeom (openMenu, items);
		if (x >= g.x && x < g.x + g.w && y >= g.y && y < g.y + g.h)
		{
			for (size_t i = 0; i < items.size (); ++i)
			{
				if (y < g.y + g.itemY[i] || y >= g.y + g.itemY[i] + g.itemH[i]) continue;
				const Item& it = items[i];
				if (it.kind == Item::Slider)
				{
					dragSlider = static_cast<int> (i); dragMenu = openMenu;
					host.uiBegin (it.pid);
					setSliderFromX (it, g, x);
				}
				else if (it.kind != Item::Sep)
				{
					auto run = it.run;
					openMenu = -1; hoverItem = -1;
					if (run) run ();
				}
				return;
			}
			return;
		}
		const int b = menuBarHit (x, y);
		if (b >= 0 && b != openMenu) { openMenu = b; hoverItem = -1; return; }
		openMenu = -1; hoverItem = -1; // clic hors menu : fermeture (et on le consomme)
		return;
	}
	const int b = menuBarHit (x, y);
	if (b >= 0) { openMenu = b; hoverItem = -1; return; }
	// oeuf
	if (flip > 0.97)
	{
		if (hitEgg (toEggSpace (x, y)) && clicks >= 1) flipTarget = 0.0;
		return;
	}
	if (flip < 0.03)
	{
		const Pt e = toEggSpace (x, y);
		if (hitKnob (e))
		{
			if (clicks >= 2) { host.editPlain (id::kDrive, spec (id::kDrive).def); return; }
			dragKnob = true; knobLastY = y; host.uiBegin (id::kDrive);
			return;
		}
		const int bi = hitButton (e);
		if (bi >= 0) { pressedButton = bi; return; }
		if (hitEgg (e) && clicks >= 2) flipTarget = 1.0;
	}
}

void TamaScene::mouseMove (double x, double y, int mods)
{
	mx = x; my = y;
	if (dragKnob)
	{
		const double k = (mods & kShift) ? 0.0015 : 0.006;
		const double n = clampd (host.getNorm (id::kDrive) + (knobLastY - y) * k, 0.0, 1.0);
		knobLastY = y;
		host.uiSet (id::kDrive, n);
		return;
	}
	if (dragSlider >= 0 && openMenu >= 0)
	{
		const auto items = buildMenu (openMenu);
		if (dragSlider < static_cast<int> (items.size ())) setSliderFromX (items[static_cast<size_t> (dragSlider)], menuGeom (openMenu, items), x);
		return;
	}
	if (dialogOpen) { updateCursor (x, y); return; }
	hoverBar = menuBarHit (x, y);
	if (openMenu >= 0)
	{
		if (hoverBar >= 0 && hoverBar != openMenu) { openMenu = hoverBar; }
		const auto items = buildMenu (openMenu);
		const MenuGeom g = menuGeom (openMenu, items);
		hoverItem = -1;
		if (x >= g.x && x < g.x + g.w)
			for (size_t i = 0; i < items.size (); ++i)
				if (y >= g.y + g.itemY[i] && y < g.y + g.itemY[i] + g.itemH[i]) hoverItem = static_cast<int> (i);
	}
	const Pt e = toEggSpace (x, y);
	const bool front = flip < 0.03;
	hoverEgg = (openMenu < 0) && (flip < 0.03 || flip > 0.97) && hitEgg (e);
	hoverKnob = front && openMenu < 0 && hitKnob (e);
	hoverButton = front && openMenu < 0 ? hitButton (e) : -1;
	updateCursor (x, y);
}

void TamaScene::mouseUp (double x, double y, int button)
{
	(void) button;
	if (pressedTitleBtn == 101)
	{
		const DialogGeom g = dialogGeom ();
		if (x >= g.centralX && x < g.centralX + g.centralW && y >= g.okY && y < g.okY + g.okH) onirique::license::core::openCentral ();
		pressedTitleBtn = -1;
		return;
	}
	if (pressedTitleBtn == 100)
	{
		const DialogGeom g = dialogGeom ();
		if (x >= g.okX && x < g.okX + g.okW && y >= g.okY && y < g.okY + g.okH) dialogOpen = false;
		pressedTitleBtn = -1;
		return;
	}
	if (dragKnob) { dragKnob = false; host.uiEnd (id::kDrive); }
	if (dragSlider >= 0 && openMenu >= 0)
	{
		const auto items = buildMenu (openMenu);
		if (dragSlider < static_cast<int> (items.size ())) host.uiEnd (items[static_cast<size_t> (dragSlider)].pid);
		dragSlider = -1;
	}
	if (pressedButton >= 0)
	{
		const int b = pressedButton; pressedButton = -1;
		if (hitButton (toEggSpace (x, y)) == b) pressButton (b);
	}
}

void TamaScene::mouseLeave ()
{
	hoverEgg = false; hoverKnob = false; hoverButton = -1; hoverBar = -1; hoverItem = -1;
	cur = CursorKind::Arrow;
}

void TamaScene::wheel (double x, double y, double dy, int mods)
{
	if (dialogOpen || openMenu >= 0 || dy == 0) return;
	const double dir = dy > 0 ? 1.0 : -1.0;
	const bool fine = (mods & kShift) != 0;
	if (flip > 0.03) return;
	const Pt e = toEggSpace (x, y);
	if (hitKnob (e))
	{
		const double step = fine ? 0.1 : 0.5;
		host.editPlain (id::kDrive, clampd (host.getPlain (id::kDrive) + dir * step, 1.0, 11.0));
	}
	else if (hitLcd (e))
	{
		const double step = fine ? 0.5 : 2.0;
		host.editPlain (id::kSqueeze, clampd (host.getPlain (id::kSqueeze) + dir * step, 0.0, 100.0));
	}
}

bool TamaScene::keyDown (int vkey)
{
	// 27 = Echap, 13 = Entrée (codes VSTGUI traduits par la vue)
	if (vkey == 27 || vkey == 13)
	{
		if (dialogOpen) { dialogOpen = false; return true; }
		if (openMenu >= 0) { openMenu = -1; return true; }
	}
	return false;
}
} // namespace tama::ui
