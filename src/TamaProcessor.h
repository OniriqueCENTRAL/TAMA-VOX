// TAMA-VOX : composant audio VST3 (processeur). Aucune allocation ni verrou dans process().
#pragma once
#include "StdCommon.h"
#include "TamaParams.h"
#include "dsp/TamaEngine.h"
#include "License/LicenseCore.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include <atomic>

namespace tama
{
class TamaProcessor : public Steinberg::Vst::AudioEffect
{
public:
	TamaProcessor ();
	~TamaProcessor () override = default;

	static Steinberg::FUnknown* createInstance (void*) { return static_cast<Steinberg::Vst::IAudioProcessor*> (new TamaProcessor); }

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) override;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs, Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs, Steinberg::int32 numOuts) override;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) override;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) override;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) override;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) override;
	Steinberg::uint32 PLUGIN_API getLatencySamples () override { return static_cast<Steinberg::uint32> (latency.load (std::memory_order_relaxed)); }
	Steinberg::uint32 PLUGIN_API getTailSamples () override { return 0; }

	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) override;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) override;

private:
	template <class T> void run (const T* const* in, T* const* out, int n, int channels, Steinberg::Vst::ProcessData& data);
	void readParameterChanges (Steinberg::Vst::IParameterChanges* changes);
	void pushMeters (Steinberg::Vst::IParameterChanges* out, int numSamples);
	void rebuildEngineParams ();

	// Licence ONIRIQUE (acheté, essai, démo) : voir License/LicenseCore.h.
	onirique::license::core::Guard license {"onirique-x-abrasion-tama-vox"};

	TamaEngine engine;
	std::atomic<double> norm[id::kNumParams];
	std::atomic<bool> dirty {true};
	std::atomic<int> latency {0};
	double sampleRate = 48000.0;
	int channels = 2;
	int meterCounter = 0;
	int meterInterval = 800;
	bool prepared = false;
};
} // namespace tama
