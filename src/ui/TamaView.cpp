#include "../StdCommon.h"
#include "TamaView.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/events.h"
#include <chrono>

using namespace VSTGUI;

namespace tama::ui
{
namespace
{
CColor toC (Color c) { return CColor (c.r, c.g, c.b, c.a); }

// Peintre VSTGUI : transforme les primitives de Painter en appels CDrawContext.
class VstguiSink : public IPrimitiveSink
{
public:
	VstguiSink (CDrawContext& c, CPoint o) : ctx (c), origin (o) {}

	void fillPolygon (const Pt* p, int n, Color c) override
	{
		if (auto path = makePath (p, n, true))
		{
			ctx.setFillColor (toC (c));
			ctx.drawGraphicsPath (path, CDrawContext::kPathFilled);
			path->forget ();
		}
	}
	void fillPolygonLinear (const Pt* p, int n, Pt g0, Pt g1, const GradStop* s, int ns) override
	{
		auto path = makePath (p, n, true);
		if (!path) return;
		if (auto grad = makeGradient (s, ns))
		{
			ctx.fillLinearGradient (path, *grad, pt (g0), pt (g1));
			grad->forget ();
		}
		path->forget ();
	}
	void fillPolygonRadial (const Pt* p, int n, Pt c, double r, const GradStop* s, int ns) override
	{
		auto path = makePath (p, n, true);
		if (!path) return;
		if (auto grad = makeGradient (s, ns))
		{
			ctx.fillRadialGradient (path, *grad, pt (c), r);
			grad->forget ();
		}
		path->forget ();
	}
	void strokePolyline (const Pt* p, int n, bool closed, double width, Color c) override
	{
		if (auto path = makePath (p, n, closed))
		{
			ctx.setFrameColor (toC (c));
			ctx.setLineWidth (width);
			ctx.setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
			ctx.drawGraphicsPath (path, CDrawContext::kPathStroked);
			path->forget ();
		}
	}
	void fillRectAxis (double x, double y, double w, double h, Color c) override
	{
		// alignement sur la grille logique : pixels nets
		const double x0 = std::floor (x + 0.5), y0 = std::floor (y + 0.5);
		const double x1 = std::max (x0 + 1.0, std::floor (x + w + 0.5)), y1 = std::max (y0 + 1.0, std::floor (y + h + 0.5));
		ctx.setDrawMode (kAliasing);
		ctx.setFillColor (toC (c));
		ctx.drawRect (CRect (x0 + origin.x, y0 + origin.y, x1 + origin.x, y1 + origin.y), kDrawFilled);
		ctx.setDrawMode (kAntiAliasing);
	}

private:
	CPoint pt (Pt p) const { return CPoint (p.x + origin.x, p.y + origin.y); }
	CGraphicsPath* makePath (const Pt* p, int n, bool close)
	{
		if (n < 2) return nullptr;
		CGraphicsPath* path = ctx.createGraphicsPath ();
		if (!path) return nullptr;
		path->beginSubpath (pt (p[0]));
		for (int i = 1; i < n; ++i) path->addLine (pt (p[i]));
		if (close) path->closeSubpath ();
		return path;
	}
	CGradient* makeGradient (const GradStop* s, int ns)
	{
		if (ns < 2) return nullptr;
		VSTGUI::GradientColorStopMap map;
		for (int i = 0; i < ns; ++i) map.emplace (s[i].t, toC (s[i].c));
		return CGradient::create (map);
	}
	CDrawContext& ctx;
	CPoint origin;
};

int modsOf (const Modifiers& m)
{
	int r = 0;
	if (m.has (ModifierKey::Shift)) r |= kShift;
	if (m.has (ModifierKey::Control)) r |= kCtrl;
	if (m.has (ModifierKey::Alt)) r |= kAlt;
	return r;
}

double nowMs ()
{
	using namespace std::chrono;
	return duration<double, std::milli> (steady_clock::now ().time_since_epoch ()).count ();
}
} // namespace

TamaView::TamaView (const CRect& size, IUiHost& host) : CView (size), scene (host)
{
	setWantsFocus (true);
	setTransparency (false);
}

bool TamaView::attached (CView* parent)
{
	const bool ok = CView::attached (parent);
	timer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) { invalid (); applyCursor (); }, 33);
	return ok;
}

bool TamaView::removed (CView* parent)
{
	if (timer) { timer->stop (); timer = nullptr; }
	return CView::removed (parent);
}

CPoint TamaView::localPoint (const CPoint& framePoint) const
{
	CPoint p (framePoint);
	frameToLocal (p);
	return p;
}

void TamaView::applyCursor ()
{
	auto* f = getFrame ();
	if (!f) return;
	switch (scene.cursor ())
	{
		case CursorKind::Hand: f->setCursor (kCursorHand); break;
		case CursorKind::ResizeV: f->setCursor (kCursorVSize); break;
		case CursorKind::ResizeH: f->setCursor (kCursorHSize); break;
		default: f->setCursor (kCursorDefault); break;
	}
}

void TamaView::draw (CDrawContext* context)
{
	context->saveGlobalState ();
	context->setDrawMode (kAntiAliasing);
	context->setClipRect (getViewSize ());
	const CRect r = getViewSize ();
	VstguiSink sink (*context, r.getTopLeft ());
	Painter painter (sink);
	scene.draw (painter, nowMs ());
	context->restoreGlobalState ();
	setDirty (false);
}

void TamaView::onMouseDownEvent (MouseDownEvent& event)
{
	if (!event.buttonState.isLeft ()) return;
	const CPoint p = localPoint (event.mousePosition);
	scene.mouseDown (p.x, p.y, 0, modsOf (event.modifiers), static_cast<int> (event.clickCount));
	applyCursor ();
	invalid ();
	event.consumed = true;
}

void TamaView::onMouseMoveEvent (MouseMoveEvent& event)
{
	const CPoint p = localPoint (event.mousePosition);
	scene.mouseMove (p.x, p.y, modsOf (event.modifiers));
	applyCursor ();
	event.consumed = true;
}

void TamaView::onMouseUpEvent (MouseUpEvent& event)
{
	const CPoint p = localPoint (event.mousePosition);
	scene.mouseUp (p.x, p.y, 0);
	applyCursor ();
	invalid ();
	event.consumed = true;
}

void TamaView::onMouseCancelEvent (MouseCancelEvent& event)
{
	scene.mouseUp (-1000, -1000, 0);
	event.consumed = true;
}

void TamaView::onMouseExitEvent (MouseExitEvent& event)
{
	scene.mouseLeave ();
	event.consumed = true;
}

void TamaView::onMouseWheelEvent (MouseWheelEvent& event)
{
	const CPoint p = localPoint (event.mousePosition);
	double d = event.deltaY;
	if (event.flags & MouseWheelEvent::DirectionInvertedFromDevice) d = -d;
	if (event.flags & MouseWheelEvent::PreciseDeltas) d *= 0.1; // les trackpads envoient de petits pas
	scene.wheel (p.x, p.y, d, modsOf (event.modifiers));
	invalid ();
	event.consumed = true;
}

void TamaView::onKeyboardEvent (KeyboardEvent& event)
{
	if (event.type != EventType::KeyDown) return;
	int vk = 0;
	if (event.virt == VirtualKey::Escape) vk = 27;
	else if (event.virt == VirtualKey::Return || event.virt == VirtualKey::Enter) vk = 13;
	if (vk && scene.keyDown (vk)) { event.consumed = true; invalid (); }
}
} // namespace tama::ui
