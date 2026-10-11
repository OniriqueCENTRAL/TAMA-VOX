#include "../StdCommon.h"
#include "TamaEditor.h"
#include "TamaScene.h"
#include "TamaView.h"
#include "../TamaController.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cview.h"

#if SMTG_OS_LINUX
#include "pluginterfaces/gui/iplugview.h"
#include "vstgui/lib/platform/linux/x11frame.h"
#include "base/source/fobject.h"
#include <vector>
#endif

using namespace VSTGUI;
using namespace Steinberg;

namespace tama::ui
{
#if SMTG_OS_LINUX
// Pont entre la boucle d'événements de l'hôte (IRunLoop VST3) et celle de VSTGUI (même rôle que dans VST3Editor).
class RunLoopBridge : public X11::IRunLoop, public AtomicReferenceCounted
{
public:
	struct EventHandler : Linux::IEventHandler, public FObject
	{
		X11::IEventHandler* handler {nullptr};
		void PLUGIN_API onFDIsSet (Linux::FileDescriptor) override { if (handler) handler->onEvent (); }
		DELEGATE_REFCOUNT (FObject)
		DEFINE_INTERFACES
			DEF_INTERFACE (Linux::IEventHandler)
		END_DEFINE_INTERFACES (FObject)
	};
	struct TimerHandler : Linux::ITimerHandler, public FObject
	{
		X11::ITimerHandler* handler {nullptr};
		void PLUGIN_API onTimer () final { if (handler) handler->onTimer (); }
		DELEGATE_REFCOUNT (FObject)
		DEFINE_INTERFACES
			DEF_INTERFACE (Linux::ITimerHandler)
		END_DEFINE_INTERFACES (FObject)
	};

	bool registerEventHandler (int fd, X11::IEventHandler* handler) final
	{
		if (!runLoop) return false;
		auto h = Steinberg::owned (new EventHandler ());
		h->handler = handler;
		if (runLoop->registerEventHandler (h, fd) == kResultTrue) { eventHandlers.push_back (h); return true; }
		return false;
	}
	bool unregisterEventHandler (X11::IEventHandler* handler) final
	{
		if (!runLoop) return false;
		for (auto it = eventHandlers.begin (); it != eventHandlers.end (); ++it)
			if ((*it)->handler == handler) { runLoop->unregisterEventHandler (*it); eventHandlers.erase (it); return true; }
		return false;
	}
	bool registerTimer (uint64_t interval, X11::ITimerHandler* handler) final
	{
		if (!runLoop) return false;
		auto h = Steinberg::owned (new TimerHandler ());
		h->handler = handler;
		if (runLoop->registerTimer (h, interval) == kResultTrue) { timerHandlers.push_back (h); return true; }
		return false;
	}
	bool unregisterTimer (X11::ITimerHandler* handler) final
	{
		if (!runLoop) return false;
		for (auto it = timerHandlers.begin (); it != timerHandlers.end (); ++it)
			if ((*it)->handler == handler) { runLoop->unregisterTimer (*it); timerHandlers.erase (it); return true; }
		return false;
	}
	explicit RunLoopBridge (FUnknown* rl) : runLoop (rl) {}

private:
	std::vector<IPtr<EventHandler>> eventHandlers;
	std::vector<IPtr<TimerHandler>> timerHandlers;
	FUnknownPtr<Linux::IRunLoop> runLoop;
};
#endif

TamaEditor::TamaEditor (Vst::EditController* controller)
: Vst::VSTGUIEditor (controller), host (dynamic_cast<TamaController*> (controller))
{
	ViewRect rect (0, 0, TamaScene::kW, TamaScene::kH);
	setRect (rect);
}

bool PLUGIN_API TamaEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame || !host) return false;
	const CRect size (0, 0, TamaScene::kW, TamaScene::kH);
	frame = new CFrame (size, this);
	frame->setBackgroundColor (CColor (192, 192, 192));
	auto* view = new TamaView (size, *host);
	frame->addView (view);

	IPlatformFrameConfig* config = nullptr;
#if SMTG_OS_LINUX
	X11::FrameConfig x11config;
	x11config.runLoop = VSTGUI::owned (new RunLoopBridge (plugFrame));
	config = &x11config;
#endif
	if (!frame->open (parent, platformType, config))
	{
		frame->forget ();
		frame = nullptr;
		return false;
	}
	frame->setFocusView (view);
	return true;
}

void PLUGIN_API TamaEditor::close ()
{
	if (frame)
	{
		frame->removeAll (true);
		frame->forget ();
		frame = nullptr;
	}
}
} // namespace tama::ui
