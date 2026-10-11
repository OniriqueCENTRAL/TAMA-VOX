// TAMA-VOX : éditeur VST3 (VSTGUI) : crée le cadre, la vue unique et branche le contrôleur.
#pragma once
#include "../StdCommon.h"
#include "UiHost.h"
#include "public.sdk/source/vst/vstguieditor.h"

namespace tama::ui
{
class TamaEditor : public Steinberg::Vst::VSTGUIEditor
{
public:
	explicit TamaEditor (Steinberg::Vst::EditController* controller);
	~TamaEditor () override = default;

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) override;
	void PLUGIN_API close () override;

private:
	IUiHost* host = nullptr;
};
} // namespace tama::ui
