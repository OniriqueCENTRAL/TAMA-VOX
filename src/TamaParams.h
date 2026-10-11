// TAMA-VOX : table unique des paramètres, partagée par le processeur, le contrôleur, l'interface et les tests.
// Aucune dépendance au SDK : tout est calculé ici (conversion normalisé <-> valeur réelle, texte).
#pragma once
#include "StdCommon.h"
#include "dsp/Models.h"
#include "dsp/TamaEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tama
{
namespace id
{
enum : uint32_t
{
	kDrive = 0, kFlavor, kSqueeze, kMode, kAuto, kRaw, kModel,
	kThr, kRatio, kKnee, kAttack, kAdapt, kRelMin, kRelMax, kMemory,
	kColorK, kColorB, kLo, kMid, kHi,
	kBypass,
	kNumParams, // 21

	// sorties (lecture seule, cachées) : compteurs envoyés du processeur vers l'interface
	kMeterBase = 100,
	kMLamp = 100, kMGr, kMDuck, kMTau, kMLufsIn, kMLufsOut, kMMakeup, kMSquash, kMGlitch, kMSat, kMPeak,
	kMeterEnd
};
} // namespace id

constexpr int kNumMeters = id::kMeterEnd - id::kMeterBase;

inline constexpr const char* kFlavorNames[3] = {"Pink", "Blue", "Black"};
inline constexpr const char* kFlavorLong[3] = {"Pink : triode H2", "Blue : FET 1176 H3", "Black : wavefolding dur"};
inline constexpr const char* kModeNames[2] = {"SNAP", "PURE"};
inline constexpr const char* kModeLong[2] = {"SNAP : phase minimale, 0 sample", "PURE : phase lineaire, 64 samples"};

struct ParamSpec
{
	uint32_t id;
	const char* name;
	const char* units;
	double min, max, def;
	int steps;      // 0 = continu ; n = n+1 valeurs discrètes
	double skew;    // 1 = linéaire ; >1 = plus de résolution dans le bas
	bool automatable;
};

inline constexpr ParamSpec kParams[id::kNumParams] = {
	{id::kDrive,   "Flavor Drive",       "",    1, 11, 5, 0, 1, true},
	{id::kFlavor,  "Flavor",             "",    0, 2, 0, 2, 1, true},
	{id::kSqueeze, "Squeeze",            "",    0, 100, 45, 0, 1, true},
	{id::kMode,    "Engine Mode",        "",    0, 1, 0, 1, 1, true},
	{id::kAuto,    "Auto-gain LUFS",     "",    0, 1, 1, 1, 1, true},
	{id::kRaw,     "Listen RAW",         "",    0, 1, 0, 1, 1, true},
	{id::kModel,   "Compressor Model",   "",    0, 9, 0, 9, 1, true},
	{id::kThr,     "Threshold Offset",   "dB",  -12, 12, 0, 0, 1, true},
	{id::kRatio,   "Ratio Offset",       "",    -6, 12, 0, 0, 1, true},
	{id::kKnee,    "Knee Offset",        "dB",  -20, 20, 0, 0, 1, true},
	{id::kAttack,  "Attack",             "ms",  0.1, 100, 3, 0, 3, true},
	{id::kAdapt,   "T4 Adaptive Release","%",   0, 100, 100, 0, 1, true},
	{id::kRelMin,  "Release Min",        "ms",  10, 400, 45, 0, 2, true},
	{id::kRelMax,  "Release Max",        "ms",  100, 2000, 800, 0, 2, true},
	{id::kMemory,  "Energy Memory",      "ms",  50, 1500, 350, 0, 2, true},
	{id::kColorK,  "Color Saturation",   "",    0, 3, 0, 0, 1, true},
	{id::kColorB,  "Color Even/Odd",     "",    0, 0.5, 0, 0, 1, true},
	{id::kLo,      "Color Low",          "dB",  -6, 6, 0, 0, 1, true},
	{id::kMid,     "Color Mid",          "dB",  -6, 6, 0, 0, 1, true},
	{id::kHi,      "Color High",         "dB",  -6, 6, 0, 0, 1, true},
	{id::kBypass,  "Bypass",             "",    0, 1, 0, 1, 1, true},
};

inline const ParamSpec& spec (uint32_t paramId) { return kParams[paramId]; }

inline double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

inline double toPlain (const ParamSpec& s, double norm)
{
	norm = clamp01 (norm);
	if (s.steps > 0) return std::floor (norm * s.steps + 0.5) * (s.max - s.min) / s.steps + s.min;
	return s.min + (s.max - s.min) * (s.skew == 1.0 ? norm : std::pow (norm, s.skew));
}
inline double toNorm (const ParamSpec& s, double plain)
{
	const double r = s.max - s.min;
	double n = r > 0 ? (plain - s.min) / r : 0.0;
	n = clamp01 (n);
	if (s.steps > 0) return std::floor (n * s.steps + 0.5) / s.steps;
	return s.skew == 1.0 ? n : std::pow (n, 1.0 / s.skew);
}
inline double defaultNorm (uint32_t paramId) { return toNorm (spec (paramId), spec (paramId).def); }

// Défauts d'un modèle de compresseur pour les paramètres qu'il pilote.
inline double modelDefault (int model, uint32_t paramId)
{
	const CompModel& m = kModels[static_cast<size_t> (std::clamp (model, 0, static_cast<int> (kModels.size ()) - 1))];
	switch (paramId)
	{
		case id::kThr: return m.thr;
		case id::kRatio: return m.ratio;
		case id::kKnee: return m.knee;
		case id::kAttack: return m.att;
		case id::kAdapt: return m.adapt * 100.0;
		case id::kRelMin: return m.rmin;
		case id::kRelMax: return m.rmax;
		case id::kMemory: return m.mem;
		case id::kColorK: return m.k;
		case id::kColorB: return m.b;
		case id::kLo: return m.lo;
		case id::kMid: return m.mid;
		case id::kHi: return m.hi;
		default: return spec (paramId).def;
	}
}
inline constexpr uint32_t kModelDrivenParams[] = {id::kThr, id::kRatio, id::kKnee, id::kAttack, id::kAdapt, id::kRelMin, id::kRelMax,
                                                  id::kMemory, id::kColorK, id::kColorB, id::kLo, id::kMid, id::kHi};

// Texte d'une valeur réelle (affichage hôte et interface).
inline void formatPlain (uint32_t paramId, double plain, char* out, size_t cap)
{
	const ParamSpec& s = spec (paramId);
	switch (paramId)
	{
		case id::kFlavor: std::snprintf (out, cap, "%s", kFlavorNames[std::clamp (static_cast<int> (plain + 0.5), 0, 2)]); return;
		case id::kMode: std::snprintf (out, cap, "%s", kModeNames[plain > 0.5 ? 1 : 0]); return;
		case id::kAuto: case id::kRaw: case id::kBypass: std::snprintf (out, cap, "%s", plain > 0.5 ? "On" : "Off"); return;
		case id::kModel: std::snprintf (out, cap, "%s", kModels[static_cast<size_t> (std::clamp (static_cast<int> (plain + 0.5), 0, 9))].name); return;
		case id::kDrive: std::snprintf (out, cap, "%.1f", plain); return;
		case id::kSqueeze: std::snprintf (out, cap, "%d", static_cast<int> (std::lround (plain))); return;
		case id::kAttack: std::snprintf (out, cap, "%.1f ms", plain); return;
		case id::kAdapt: std::snprintf (out, cap, "%d %%", static_cast<int> (std::lround (plain))); return;
		case id::kRelMin: case id::kRelMax: case id::kMemory: std::snprintf (out, cap, "%d ms", static_cast<int> (std::lround (plain))); return;
		case id::kColorK: case id::kColorB: std::snprintf (out, cap, "%.2f", plain); return;
		case id::kRatio: std::snprintf (out, cap, "%+.1f", plain); return;
		default: std::snprintf (out, cap, "%+.1f %s", plain, s.units); return;
	}
}

// Lecture d'un texte (saisie hôte). Renvoie false si illisible.
inline bool ieqN (const char* a, const char* b, size_t n)
{
	for (size_t i = 0; i < n; ++i)
	{
		const char ca = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char> (a[i] + 32) : a[i];
		const char cb = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char> (b[i] + 32) : b[i];
		if (ca != cb) return false;
		if (ca == 0) return true;
	}
	return true;
}

inline bool parsePlain (uint32_t paramId, const char* text, double& plain)
{
	const ParamSpec& s = spec (paramId);
	if (paramId == id::kFlavor)
	{
		for (int i = 0; i < 3; ++i) if (ieqN (text, kFlavorNames[i], 3)) { plain = i; return true; }
	}
	else if (paramId == id::kMode)
	{
		for (int i = 0; i < 2; ++i) if (ieqN (text, kModeNames[i], 4)) { plain = i; return true; }
	}
	else if (paramId == id::kModel)
	{
		for (size_t i = 0; i < kModels.size (); ++i) if (ieqN (text, kModels[i].name, 5)) { plain = static_cast<double> (i); return true; }
	}
	else if (s.steps == 1)
	{
		if (ieqN (text, "on", 2) || text[0] == '1') { plain = 1; return true; }
		if (ieqN (text, "off", 3) || text[0] == '0') { plain = 0; return true; }
	}
	char* end = nullptr;
	const double v = std::strtod (text, &end);
	if (end == text) return false;
	plain = std::clamp (v, s.min, s.max);
	return true;
}

//------------------------------------------------------------------------------------------------
// Compteurs : correspondance valeur <-> normalisé (0..1) pour le transport par paramètres de sortie.
struct MeterSpec { uint32_t id; double lo, hi; };
inline constexpr MeterSpec kMeterSpecs[kNumMeters] = {
	{id::kMLamp, 0, 1}, {id::kMGr, 0, 24}, {id::kMDuck, 0, 12}, {id::kMTau, 0, 2500}, {id::kMLufsIn, -70, 0}, {id::kMLufsOut, -70, 0},
	{id::kMMakeup, -24, 24}, {id::kMSquash, 0, 1}, {id::kMGlitch, 0, 1}, {id::kMSat, 0, 1}, {id::kMPeak, -60, 6},
};
inline double meterToNorm (uint32_t meterId, double v)
{
	const MeterSpec& m = kMeterSpecs[meterId - id::kMeterBase];
	return clamp01 ((v - m.lo) / (m.hi - m.lo));
}
inline double meterFromNorm (uint32_t meterId, double n)
{
	const MeterSpec& m = kMeterSpecs[meterId - id::kMeterBase];
	return m.lo + clamp01 (n) * (m.hi - m.lo);
}

// Plage de valeurs normalisées -> paramètres moteur
struct ParamSet
{
	double norm[id::kNumParams];
	ParamSet () { for (uint32_t i = 0; i < id::kNumParams; ++i) norm[i] = defaultNorm (i); }
	double plain (uint32_t i) const { return toPlain (spec (i), norm[i]); }
};

inline EngineParams makeEngineParams (const ParamSet& p)
{
	EngineParams e;
	e.drive = p.plain (id::kDrive);
	e.flavor = static_cast<int> (p.plain (id::kFlavor) + 0.5);
	e.squeeze = p.plain (id::kSqueeze);
	e.mode = p.plain (id::kMode) > 0.5 ? 1 : 0;
	e.autoGain = p.plain (id::kAuto) > 0.5;
	e.raw = p.plain (id::kRaw) > 0.5;
	e.bypass = p.plain (id::kBypass) > 0.5;
	e.model = static_cast<int> (p.plain (id::kModel) + 0.5);
	e.thrOff = p.plain (id::kThr); e.ratioOff = p.plain (id::kRatio); e.kneeOff = p.plain (id::kKnee);
	e.attackMs = p.plain (id::kAttack); e.adaptPct = p.plain (id::kAdapt);
	e.relMinMs = p.plain (id::kRelMin); e.relMaxMs = std::max (p.plain (id::kRelMax), e.relMinMs); e.memoryMs = p.plain (id::kMemory);
	e.colorK = p.plain (id::kColorK); e.colorB = p.plain (id::kColorB);
	e.loDb = p.plain (id::kLo); e.midDb = p.plain (id::kMid); e.hiDb = p.plain (id::kHi);
	return e;
}
} // namespace tama
