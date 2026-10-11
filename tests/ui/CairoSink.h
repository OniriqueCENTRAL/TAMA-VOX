// Rendu de la scène TAMA-VOX avec Cairo (outil de développement : produit des PNG pour vérifier l'interface sans DAW).
#pragma once
#include "ui/Painter.h"
#include <cairo.h>

namespace tama::ui
{
class CairoSink : public IPrimitiveSink
{
public:
	explicit CairoSink (cairo_t* c) : cr (c) {}
	void fillPolygon (const Pt* p, int n, Color c) override { path (p, n, true); col (c); cairo_fill (cr); }
	void fillPolygonLinear (const Pt* p, int n, Pt g0, Pt g1, const GradStop* s, int ns) override
	{
		cairo_pattern_t* pat = cairo_pattern_create_linear (g0.x, g0.y, g1.x, g1.y);
		stops (pat, s, ns); path (p, n, true); cairo_set_source (cr, pat); cairo_fill (cr); cairo_pattern_destroy (pat);
	}
	void fillPolygonRadial (const Pt* p, int n, Pt c, double r, const GradStop* s, int ns) override
	{
		cairo_pattern_t* pat = cairo_pattern_create_radial (c.x, c.y, 0, c.x, c.y, r);
		stops (pat, s, ns); path (p, n, true); cairo_set_source (cr, pat); cairo_fill (cr); cairo_pattern_destroy (pat);
	}
	void strokePolyline (const Pt* p, int n, bool closed, double w, Color c) override
	{
		path (p, n, closed); col (c); cairo_set_line_width (cr, w); cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND); cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND); cairo_stroke (cr);
	}
	void fillRectAxis (double x, double y, double w, double h, Color c) override
	{
		// aligné sur la grille de pixels comme le fait le rendu VSTGUI
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
		cairo_rectangle (cr, std::round (x), std::round (y), std::max (1.0, std::round (x + w) - std::round (x)), std::max (1.0, std::round (y + h) - std::round (y)));
		col (c); cairo_fill (cr); cairo_set_antialias (cr, CAIRO_ANTIALIAS_DEFAULT);
	}

private:
	void col (Color c) { cairo_set_source_rgba (cr, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0); }
	void path (const Pt* p, int n, bool close) { cairo_new_path (cr); for (int i = 0; i < n; ++i) { if (i) cairo_line_to (cr, p[i].x, p[i].y); else cairo_move_to (cr, p[i].x, p[i].y); } if (close) cairo_close_path (cr); }
	void stops (cairo_pattern_t* pat, const GradStop* s, int ns) { for (int i = 0; i < ns; ++i) cairo_pattern_add_color_stop_rgba (pat, s[i].t, s[i].c.r / 255.0, s[i].c.g / 255.0, s[i].c.b / 255.0, s[i].c.a / 255.0); }
	cairo_t* cr;
};
} // namespace tama::ui
