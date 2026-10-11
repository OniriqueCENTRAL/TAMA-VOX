// TAMA-VOX : couche de dessin 2D minimale, indépendante de toute bibliothèque graphique.
// La scène (TamaScene) ne dessine qu'avec cette classe ; chaque plate-forme fournit un IPrimitiveSink
// (VSTGUI dans le plugin, Cairo dans l'outil de prévisualisation PNG). Les courbes sont transformées en
// polygones ici, donc les deux rendus sont identiques à l'antialiasing près.
#pragma once
#include "../StdCommon.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tama::ui
{
struct Pt { double x = 0, y = 0; };

struct Color
{
	uint8_t r = 0, g = 0, b = 0, a = 255;
	static constexpr Color rgb (uint32_t hex, uint8_t alpha = 255) { return {static_cast<uint8_t> ((hex >> 16) & 255), static_cast<uint8_t> ((hex >> 8) & 255), static_cast<uint8_t> (hex & 255), alpha}; }
	Color alpha (double f) const { return {r, g, b, static_cast<uint8_t> (std::clamp (a * f, 0.0, 255.0))}; }
	Color mix (Color o, double t) const
	{
		auto m = [t] (uint8_t x, uint8_t y) { return static_cast<uint8_t> (std::clamp (x + (y - x) * t, 0.0, 255.0)); };
		return {m (r, o.r), m (g, o.g), m (b, o.b), m (a, o.a)};
	}
	Color lighten (double t) const { return mix ({255, 255, 255, a}, t); }
	Color darken (double t) const { return mix ({0, 0, 0, a}, t); }
	bool operator== (const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
};

struct GradStop { double t; Color c; };

// Ce que chaque plate-forme doit savoir faire.
class IPrimitiveSink
{
public:
	virtual ~IPrimitiveSink () = default;
	virtual void fillPolygon (const Pt* p, int n, Color c) = 0;
	virtual void fillPolygonLinear (const Pt* p, int n, Pt g0, Pt g1, const GradStop* stops, int ns) = 0;
	virtual void fillPolygonRadial (const Pt* p, int n, Pt centre, double radius, const GradStop* stops, int ns) = 0;
	virtual void strokePolyline (const Pt* p, int n, bool closed, double width, Color c) = 0;
	virtual void fillRectAxis (double x, double y, double w, double h, Color c) = 0; // rectangle aligné sur les axes
};

class Painter
{
public:
	explicit Painter (IPrimitiveSink& s) : sink (s) { stack.push_back ({}); }

	// ---- transformation ----
	void save () { stack.push_back (stack.back ()); }
	void restore () { if (stack.size () > 1) stack.pop_back (); }
	void translate (double x, double y) { M& m = stack.back (); m.e += m.a * x + m.c * y; m.f += m.b * x + m.d * y; }
	void rotate (double rad)
	{
		M& m = stack.back ();
		const double cs = std::cos (rad), sn = std::sin (rad);
		const M o = m;
		m.a = o.a * cs + o.c * sn; m.b = o.b * cs + o.d * sn;
		m.c = -o.a * sn + o.c * cs; m.d = -o.b * sn + o.d * cs;
	}
	void scale (double sx, double sy) { M& m = stack.back (); m.a *= sx; m.b *= sx; m.c *= sy; m.d *= sy; }
	Pt toWorld (Pt p) const { const M& m = stack.back (); return {m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f}; }
	Pt toLocal (Pt p) const
	{
		const M& m = stack.back ();
		const double det = m.a * m.d - m.b * m.c;
		const double x = p.x - m.e, y = p.y - m.f;
		return {(m.d * x - m.c * y) / det, (-m.b * x + m.a * y) / det};
	}

	// ---- formes (coordonnées locales) ----
	static std::vector<Pt> rectShape (double x, double y, double w, double h) { return {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}}; }
	static std::vector<Pt> ellipseShape (double cx, double cy, double rx, double ry, int seg = 0)
	{
		if (seg <= 0) seg = std::clamp (static_cast<int> (std::max (rx, ry) * 0.9) + 12, 16, 120);
		std::vector<Pt> v (static_cast<size_t> (seg));
		for (int i = 0; i < seg; ++i) { const double t = 2 * 3.14159265358979 * i / seg; v[static_cast<size_t> (i)] = {cx + rx * std::cos (t), cy + ry * std::sin (t)}; }
		return v;
	}
	static std::vector<Pt> roundRectShape (double x, double y, double w, double h, double r)
	{
		r = std::min ({r, w * 0.5, h * 0.5});
		if (r <= 0.5) return rectShape (x, y, w, h);
		const int seg = std::clamp (static_cast<int> (r * 0.7) + 3, 4, 14);
		std::vector<Pt> v;
		const double cx[4] = {x + w - r, x + w - r, x + r, x + r}, cy[4] = {y + r, y + h - r, y + h - r, y + r};
		for (int k = 0; k < 4; ++k)
			for (int i = 0; i <= seg; ++i)
			{
				const double t = (k * 90.0 + 90.0 * i / seg - 90.0) * 3.14159265358979 / 180.0;
				v.push_back ({cx[k] + r * std::cos (t), cy[k] + r * std::sin (t)});
			}
		return v;
	}

	// ---- remplissages ----
	void fill (const std::vector<Pt>& shape, Color c) { if (c.a) { auto w = world (shape); sink.fillPolygon (w.data (), static_cast<int> (w.size ()), c); } }
	void fillLinear (const std::vector<Pt>& shape, Pt g0, Pt g1, std::initializer_list<GradStop> stops)
	{
		auto w = world (shape);
		sink.fillPolygonLinear (w.data (), static_cast<int> (w.size ()), toWorld (g0), toWorld (g1), stops.begin (), static_cast<int> (stops.size ()));
	}
	void fillRadial (const std::vector<Pt>& shape, Pt centre, double radius, std::initializer_list<GradStop> stops)
	{
		auto w = world (shape);
		const M& m = stack.back ();
		const double sc = std::sqrt (std::fabs (m.a * m.d - m.b * m.c));
		sink.fillPolygonRadial (w.data (), static_cast<int> (w.size ()), toWorld (centre), radius * sc, stops.begin (), static_cast<int> (stops.size ()));
	}
	void stroke (const std::vector<Pt>& shape, bool closed, double width, Color c)
	{
		if (!c.a) return;
		auto w = world (shape);
		const M& m = stack.back ();
		sink.strokePolyline (w.data (), static_cast<int> (w.size ()), closed, width * std::sqrt (std::fabs (m.a * m.d - m.b * m.c)), c);
	}

	void rect (double x, double y, double w, double h, Color c)
	{
		if (!c.a || w <= 0 || h <= 0) return;
		const M& m = stack.back ();
		if (m.b == 0 && m.c == 0) { sink.fillRectAxis (m.a * x + m.e, m.d * y + m.f, m.a * w, m.d * h, c); return; }
		fill (rectShape (x, y, w, h), c);
	}
	void roundRect (double x, double y, double w, double h, double r, Color c) { fill (roundRectShape (x, y, w, h, r), c); }
	void ellipse (double cx, double cy, double rx, double ry, Color c) { fill (ellipseShape (cx, cy, rx, ry), c); }
	void ellipseStroke (double cx, double cy, double rx, double ry, double w, Color c) { stroke (ellipseShape (cx, cy, rx, ry), true, w, c); }
	void line (double x0, double y0, double x1, double y1, double w, Color c) { stroke ({{x0, y0}, {x1, y1}}, false, w, c); }
	void frame (double x, double y, double w, double h, Color c) // cadre 1 px à l'intérieur du rectangle
	{
		rect (x, y, w, 1, c); rect (x, y + h - 1, w, 1, c); rect (x, y + 1, 1, h - 2, c); rect (x + w - 1, y + 1, 1, h - 2, c);
	}

private:
	struct M { double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0; };
	std::vector<Pt> world (const std::vector<Pt>& s) const
	{
		std::vector<Pt> w (s.size ());
		for (size_t i = 0; i < s.size (); ++i) w[i] = toWorld (s[i]);
		return w;
	}
	IPrimitiveSink& sink;
	std::vector<M> stack;
};
} // namespace tama::ui
