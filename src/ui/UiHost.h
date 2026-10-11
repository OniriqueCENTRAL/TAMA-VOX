// TAMA-VOX : interface entre l'interface graphique (indépendante du SDK) et le contrôleur du plugin.
#pragma once
#include "../StdCommon.h"
#include "../TamaParams.h"

namespace tama::ui
{
struct IUiHost
{
	virtual ~IUiHost () = default;
	virtual double getNorm (uint32_t paramId) const = 0;        // valeur normalisée 0..1
	virtual void uiBegin (uint32_t paramId) = 0;
	virtual void uiSet (uint32_t paramId, double norm) = 0;     // à appeler entre uiBegin/uiEnd
	virtual void uiEnd (uint32_t paramId) = 0;
	virtual double getMeter (uint32_t meterId) const = 0;       // valeur réelle (dB, ms, 0..1 ...)

	// Opérations groupées (geste unique, annulable par l'hôte)
	virtual void applyModel (int model) = 0;
	virtual void applyPreset (int preset) = 0;
	virtual void applyCompPreset (int preset) = 0;
	virtual void resetCompressor () = 0;
	virtual void resetAll () = 0;

	double getPlain (uint32_t paramId) const { return toPlain (spec (paramId), getNorm (paramId)); }
	void editPlain (uint32_t paramId, double plain)
	{
		uiBegin (paramId);
		uiSet (paramId, toNorm (spec (paramId), plain));
		uiEnd (paramId);
	}
	void editNorm (uint32_t paramId, double norm)
	{
		uiBegin (paramId);
		uiSet (paramId, norm);
		uiEnd (paramId);
	}
};
} // namespace tama::ui
