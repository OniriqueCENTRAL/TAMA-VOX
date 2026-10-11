#include "StdCommon.h"
#include "TamaProcessor.h"
#include "TamaCids.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/vstaudioprocessoralgo.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace tama
{
namespace
{
constexpr int32 kStateMagic = 0x414D4154; // 'TAMA'
constexpr int32 kStateVersion = 1;
} // namespace

TamaProcessor::TamaProcessor ()
{
	setControllerClass (kControllerUID);
	for (uint32_t i = 0; i < id::kNumParams; ++i) norm[i].store (defaultNorm (i));
}

tresult PLUGIN_API TamaProcessor::initialize (FUnknown* context)
{
	const tresult r = AudioEffect::initialize (context);
	if (r != kResultOk) return r;
	addAudioInput (STR16 ("Input"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Output"), SpeakerArr::kStereo);
	return kResultOk;
}

tresult PLUGIN_API TamaProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns == 1 && numOuts == 1 && inputs[0] == outputs[0] && (inputs[0] == SpeakerArr::kMono || inputs[0] == SpeakerArr::kStereo))
		return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
	return kResultFalse;
}

tresult PLUGIN_API TamaProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	return (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64) ? kResultTrue : kResultFalse;
}

tresult PLUGIN_API TamaProcessor::setupProcessing (ProcessSetup& setup)
{
	sampleRate = setup.sampleRate;
	channels = 2;
	if (!audioInputs.empty ()) channels = SpeakerArr::getChannelCount (static_cast<AudioBus*> (audioInputs[0].get ())->getArrangement ());
	channels = std::clamp (channels, 1, 2);
	engine.prepare (sampleRate, channels); // seule allocation : hors temps réel
	meterInterval = std::max (64, static_cast<int> (sampleRate * 0.016));
	prepared = true;
	dirty.store (true);
	return AudioEffect::setupProcessing (setup);
}

tresult PLUGIN_API TamaProcessor::setActive (TBool state)
{
	if (state && prepared)
	{
		engine.reset ();
		dirty.store (true);
		meterCounter = 0;
	}
	return AudioEffect::setActive (state);
}

void TamaProcessor::rebuildEngineParams ()
{
	ParamSet ps;
	for (uint32_t i = 0; i < id::kNumParams; ++i) ps.norm[i] = norm[i].load (std::memory_order_relaxed);
	engine.setParams (makeEngineParams (ps));
	latency.store (ps.plain (id::kMode) > 0.5 ? TamaEngine::kPureLatency : 0, std::memory_order_relaxed);
}

void TamaProcessor::readParameterChanges (IParameterChanges* changes)
{
	if (!changes) return;
	const int32 n = changes->getParameterCount ();
	for (int32 i = 0; i < n; ++i)
	{
		IParamValueQueue* q = changes->getParameterData (i);
		if (!q) continue;
		const ParamID pid = q->getParameterId ();
		const int32 pts = q->getPointCount ();
		if (pid >= id::kNumParams || pts <= 0) continue;
		int32 offset = 0;
		ParamValue v = 0;
		if (q->getPoint (pts - 1, offset, v) != kResultTrue) continue;
		norm[pid].store (clamp01 (v), std::memory_order_relaxed);
		dirty.store (true, std::memory_order_relaxed);
		if (pid == id::kMode) latency.store (toPlain (spec (id::kMode), v) > 0.5 ? TamaEngine::kPureLatency : 0, std::memory_order_relaxed);
	}
}

void TamaProcessor::pushMeters (IParameterChanges* out, int numSamples)
{
	meterCounter += numSamples;
	if (!out || meterCounter < meterInterval) return;
	meterCounter = 0;
	const EngineMeters& m = engine.meters ();
	const double v[kNumMeters] = {m.lamp, m.gr, m.duck, m.tauMs, m.lufsIn, m.lufsOut, m.makeupDb, m.squash, m.glitch, m.sat, m.peakDb};
	for (int i = 0; i < kNumMeters; ++i)
	{
		int32 index = 0;
		const uint32_t mid = id::kMeterBase + static_cast<uint32_t> (i);
		if (IParamValueQueue* q = out->addParameterData (mid, index))
		{
			int32 pi = 0;
			q->addPoint (0, meterToNorm (mid, v[i]), pi);
		}
	}
}

template <class T>
void TamaProcessor::run (const T* const* in, T* const* out, int n, int ch, ProcessData& data)
{
	(void) data;
	engine.process<T> (in, out, n, ch);
}

tresult PLUGIN_API TamaProcessor::process (ProcessData& data)
{
	readParameterChanges (data.inputParameterChanges);
	if (dirty.exchange (false, std::memory_order_relaxed)) rebuildEngineParams ();

	if (data.numSamples > 0 && data.numInputs > 0 && data.numOutputs > 0 && prepared)
	{
		AudioBusBuffers& in = data.inputs[0];
		AudioBusBuffers& out = data.outputs[0];
		const int ch = static_cast<int> (std::min (in.numChannels, out.numChannels));
		if (ch > 0)
		{
			if (data.symbolicSampleSize == kSample64)
				run<double> (in.channelBuffers64, out.channelBuffers64, data.numSamples, ch, data);
			else
				run<float> (in.channelBuffers32, out.channelBuffers32, data.numSamples, ch, data);
			out.silenceFlags = 0;
		}
	}
	pushMeters (data.outputParameterChanges, data.numSamples);
	return kResultOk;
}

tresult PLUGIN_API TamaProcessor::setState (IBStream* state)
{
	if (!state) return kResultFalse;
	int32 magic = 0, version = 0, count = 0;
	if (state->read (&magic, sizeof (magic)) != kResultOk || magic != kStateMagic) return kResultFalse;
	if (state->read (&version, sizeof (version)) != kResultOk || version < 1) return kResultFalse;
	if (state->read (&count, sizeof (count)) != kResultOk || count < 0 || count > 4096) return kResultFalse;
	for (int32 i = 0; i < count; ++i)
	{
		double v = 0;
		if (state->read (&v, sizeof (v)) != kResultOk) return kResultFalse;
		if (i < static_cast<int32> (id::kNumParams) && std::isfinite (v)) norm[i].store (clamp01 (v), std::memory_order_relaxed);
	}
	latency.store (toPlain (spec (id::kMode), norm[id::kMode].load ()) > 0.5 ? TamaEngine::kPureLatency : 0, std::memory_order_relaxed);
	dirty.store (true);
	return kResultOk;
}

tresult PLUGIN_API TamaProcessor::getState (IBStream* state)
{
	if (!state) return kResultFalse;
	const int32 magic = kStateMagic, version = kStateVersion, count = static_cast<int32> (id::kNumParams);
	state->write (const_cast<int32*> (&magic), sizeof (magic));
	state->write (const_cast<int32*> (&version), sizeof (version));
	state->write (const_cast<int32*> (&count), sizeof (count));
	for (uint32_t i = 0; i < id::kNumParams; ++i)
	{
		double v = norm[i].load (std::memory_order_relaxed);
		state->write (&v, sizeof (v));
	}
	return kResultOk;
}
} // namespace tama
