// TAMA-VOX : contrôleur VST3 (paramètres, états, création de l'éditeur).
#pragma once
#include "StdCommon.h"
#include "TamaParams.h"
#include "ui/UiHost.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include <atomic>

namespace tama
{
class TamaController : public Steinberg::Vst::EditControllerEx1, public ui::IUiHost
{
public:
	static Steinberg::FUnknown* createInstance (void*) { return static_cast<Steinberg::Vst::IEditController*> (new TamaController); }

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) override;
	Steinberg::tresult PLUGIN_API terminate () override;
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) override;
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) override;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) override;

	// IUiHost
	double getNorm (uint32_t paramId) const override;
	void uiBegin (uint32_t paramId) override { beginEdit (paramId); }
	void uiSet (uint32_t paramId, double norm) override;
	void uiEnd (uint32_t paramId) override { endEdit (paramId); }
	double getMeter (uint32_t meterId) const override;
	void applyModel (int model) override;
	void applyPreset (int preset) override;
	void applyCompPreset (int preset) override;
	void resetCompressor () override;
	void resetAll () override;

	OBJ_METHODS (TamaController, EditControllerEx1)
	DEFINE_INTERFACES
	END_DEFINE_INTERFACES (EditControllerEx1)
	REFCOUNT_METHODS (EditControllerEx1)

private:
	bool loadingState = false;
};
} // namespace tama
