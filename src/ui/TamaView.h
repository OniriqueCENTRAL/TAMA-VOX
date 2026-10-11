// TAMA-VOX : vue VSTGUI unique qui porte toute l'interface (scène + peintre VSTGUI).
#pragma once
#include "../StdCommon.h"
#include "TamaScene.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cvstguitimer.h"
#include <memory>

namespace tama::ui
{
class TamaView : public VSTGUI::CView
{
public:
	TamaView (const VSTGUI::CRect& size, IUiHost& host);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseExitEvent (VSTGUI::MouseExitEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;
	void onKeyboardEvent (VSTGUI::KeyboardEvent& event) override;
	bool attached (VSTGUI::CView* parent) override;
	bool removed (VSTGUI::CView* parent) override;

private:
	VSTGUI::CPoint localPoint (const VSTGUI::CPoint& framePoint) const;
	void applyCursor ();

	TamaScene scene;
	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
};
} // namespace tama::ui
