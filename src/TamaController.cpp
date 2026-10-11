#include "StdCommon.h"
#include "TamaController.h"
#include "TamaCids.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstchannelcontextinfo.h"
#include "public.sdk/source/vst/vstparameters.h"
#if TAMA_HAS_GUI
#include "ui/TamaEditor.h"
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace tama
{
namespace
{
constexpr int32 kStateMagic = 0x414D4154;

// UTF-8 -> UTF-16 (les noms de modèles contiennent des accents et « · »).
void copyUtf8 (const char* in, TChar* out, size_t cap)
{
	size_t o = 0;
	const unsigned char* p = reinterpret_cast<const unsigned char*> (in);
	while (*p && o + 2 < cap)
	{
		uint32_t cp = *p;
		int extra = 0;
		if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
		else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
		else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
		++p;
		while (extra-- > 0 && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
		if (cp >= 0x10000) { cp -= 0x10000; out[o++] = static_cast<TChar> (0xD800 + (cp >> 10)); out[o++] = static_cast<TChar> (0xDC00 + (cp & 0x3FF)); }
		else out[o++] = static_cast<TChar> (cp);
	}
	out[o] = 0;
}

// Paramètre à conversion personnalisée (courbes, listes, unités) : tout vient de TamaParams.h.
class TamaParameter : public Parameter
{
public:
	TamaParameter (const ParamSpec& s, int32 flags)
	: Parameter (ParameterInfo {}), spec_ (s)
	{
		copyUtf8 (s.name, info.title, 128);
		copyUtf8 (s.name, info.shortTitle, 128);
		copyUtf8 (s.units, info.units, 128);
		info.id = s.id;
		info.stepCount = s.steps;
		info.defaultNormalizedValue = defaultNorm (s.id);
		info.flags = flags;
		info.unitId = kRootUnitId;
		setNormalized (info.defaultNormalizedValue);
		precision = 2;
	}

	void toString (ParamValue valueNormalized, String128 string) const override
	{
		char buf[96];
		formatPlain (spec_.id, tama::toPlain (spec_, valueNormalized), buf, sizeof (buf));
		copyUtf8 (buf, string, 128);
	}
	bool fromString (const TChar* string, ParamValue& valueNormalized) const override
	{
		char buf[96];
		UString (const_cast<TChar*> (string), 128).toAscii (buf, sizeof (buf));
		double plain = 0;
		if (!parsePlain (spec_.id, buf, plain)) return false;
		valueNormalized = tama::toNorm (spec_, plain);
		return true;
	}
	ParamValue toPlain (ParamValue valueNormalized) const override { return tama::toPlain (spec_, valueNormalized); }
	ParamValue toNormalized (ParamValue plainValue) const override { return tama::toNorm (spec_, plainValue); }

private:
	const ParamSpec& spec_;
};

const char* const kMeterNames[kNumMeters] = {"Lamp", "Gain Reduction", "De-ess", "Release T4", "LUFS In", "LUFS Out", "Makeup", "Squeeze", "Glitch", "Saturation", "Peak"};
} // namespace

tresult PLUGIN_API TamaController::initialize (FUnknown* context)
{
	const tresult r = EditControllerEx1::initialize (context);
	if (r != kResultOk) return r;

	for (uint32_t i = 0; i < id::kNumParams; ++i)
	{
		const ParamSpec& s = kParams[i];
		int32 flags = s.automatable ? ParameterInfo::kCanAutomate : 0;
		if (i == id::kFlavor || i == id::kMode || i == id::kModel) flags |= ParameterInfo::kIsList;
		if (i == id::kBypass) flags |= ParameterInfo::kIsBypass;
		parameters.addParameter (new TamaParameter (s, flags));
	}
	for (int i = 0; i < kNumMeters; ++i)
	{
		const uint32_t mid = id::kMeterBase + static_cast<uint32_t> (i);
		auto* p = new Parameter (USTRING (kMeterNames[i]), mid, nullptr, 0.0, 0, ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden);
		parameters.addParameter (p);
	}
	return kResultOk;
}

tresult PLUGIN_API TamaController::terminate () { return EditControllerEx1::terminate (); }

tresult PLUGIN_API TamaController::setComponentState (IBStream* state)
{
	if (!state) return kResultFalse;
	int32 magic = 0, version = 0, count = 0;
	if (state->read (&magic, sizeof (magic)) != kResultOk || magic != kStateMagic) return kResultFalse;
	if (state->read (&version, sizeof (version)) != kResultOk) return kResultFalse;
	if (state->read (&count, sizeof (count)) != kResultOk || count < 0 || count > 4096) return kResultFalse;
	loadingState = true;
	for (int32 i = 0; i < count; ++i)
	{
		double v = 0;
		if (state->read (&v, sizeof (v)) != kResultOk) break;
		if (i < static_cast<int32> (id::kNumParams) && std::isfinite (v)) EditControllerEx1::setParamNormalized (static_cast<ParamID> (i), clamp01 (v));
	}
	loadingState = false;
	if (componentHandler) componentHandler->restartComponent (kLatencyChanged);
	return kResultOk;
}

IPlugView* PLUGIN_API TamaController::createView (FIDString name)
{
#if TAMA_HAS_GUI
	if (name && FIDStringsEqual (name, ViewType::kEditor)) return new ui::TamaEditor (this);
#else
	(void) name;
#endif
	return nullptr;
}

tresult PLUGIN_API TamaController::setParamNormalized (ParamID tag, ParamValue value)
{
	const bool modeChange = tag == id::kMode && std::fabs (getParamNormalized (tag) - value) > 1e-9;
	const tresult r = EditControllerEx1::setParamNormalized (tag, value);
	// la latence dépend du mode (PURE = 64 échantillons) : on prévient l'hôte pour qu'il recompense le retard
	if (modeChange && !loadingState && componentHandler) componentHandler->restartComponent (kLatencyChanged);
	return r;
}

double TamaController::getNorm (uint32_t paramId) const
{
	return const_cast<TamaController*> (this)->getParamNormalized (paramId);
}

void TamaController::uiSet (uint32_t paramId, double norm)
{
	norm = clamp01 (norm);
	setParamNormalized (paramId, norm);
	performEdit (paramId, norm);
}

double TamaController::getMeter (uint32_t meterId) const
{
	if (meterId < id::kMeterBase || meterId >= id::kMeterEnd) return 0.0;
	return meterFromNorm (meterId, const_cast<TamaController*> (this)->getParamNormalized (meterId));
}

void TamaController::applyModel (int model)
{
	model = std::clamp (model, 0, static_cast<int> (kModels.size ()) - 1);
	editNorm (id::kModel, toNorm (spec (id::kModel), model));
	for (uint32_t pid : kModelDrivenParams) editPlain (pid, modelDefault (model, pid));
}

void TamaController::applyPreset (int preset)
{
	preset = std::clamp (preset, 0, static_cast<int> (kFactoryPresets.size ()) - 1);
	const FactoryPreset& p = kFactoryPresets[static_cast<size_t> (preset)];
	editPlain (id::kDrive, p.drive);
	editPlain (id::kFlavor, p.flavor);
	editPlain (id::kSqueeze, p.squeeze);
	editPlain (id::kMode, p.mode);
	applyModel (p.model);
}

void TamaController::applyCompPreset (int preset)
{
	preset = std::clamp (preset, 0, static_cast<int> (kCompPresets.size ()) - 1);
	const CompPreset& c = kCompPresets[static_cast<size_t> (preset)];
	editPlain (id::kThr, c.thr); editPlain (id::kRatio, c.ratio); editPlain (id::kKnee, c.knee);
	editPlain (id::kAttack, c.att); editPlain (id::kAdapt, c.adapt);
	editPlain (id::kRelMin, c.rmin); editPlain (id::kRelMax, c.rmax); editPlain (id::kMemory, c.mem);
}

void TamaController::resetCompressor ()
{
	applyModel (static_cast<int> (getPlain (id::kModel) + 0.5));
}

void TamaController::resetAll ()
{
	for (uint32_t i = 0; i < id::kNumParams; ++i)
		if (i != id::kBypass) editNorm (i, defaultNorm (i));
}
} // namespace tama
