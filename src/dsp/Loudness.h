// TAMA-VOX : mesure de sonie ITU-R BS.1770 (pondération K) et auto-gain.
#pragma once
#include "../StdCommon.h"
#include "Biquad.h"

namespace tama
{
// Filtres de pondération K (shelf + RLB) dérivés pour Fs quelconque.
struct KWeighting
{
	static BiquadCoefs shelf (double fs) noexcept
	{
		const double G = 3.999843853973347, Q = 0.7071752369554196, fc = 1681.9744509555319;
		const double K = std::tan (kPi * fc / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
		const double a0 = 1.0 + K / Q + K * K;
		return {(Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0};
	}
	static BiquadCoefs rlb (double fs) noexcept { return BiquadCoefs::highpass (fs, 38.13547087613982, 0.5003270373253953); }
};

// Intégrateur de sonie momentanée (fenêtre exponentielle ~400 ms) pour N canaux.
class LoudnessMeter
{
public:
	static constexpr int kMaxCh = 2;
	void prepare (double fs, int channels) noexcept
	{
		nch = std::min(channels, kMaxCh);
		for (int c = 0; c < kMaxCh; ++c) { s1[c].set (KWeighting::shelf (fs)); s2[c].set (KWeighting::rlb (fs)); }
		a = onePole (0.4, fs);
		reset ();
	}
	void reset () noexcept { for (int c = 0; c < kMaxCh; ++c) { s1[c].reset (); s2[c].reset (); } ms = 0; }
	inline void push (int ch, double x) noexcept { const double y = s2[ch].process (s1[ch].process (x)); acc[ch] = y * y; }
	// à appeler une fois par échantillon après push() de tous les canaux
	inline void tick () noexcept { double z = 0; for (int c = 0; c < nch; ++c) z += acc[c]; ms += (z - ms) * a; }
	double meanSquare () const noexcept { return ms; }
	double lufs () const noexcept { return ms <= 1e-12 ? -120.0 : -0.691 + 10.0 * std::log10 (ms); }

private:
	Biquad s1[kMaxCh], s2[kMaxCh];
	double acc[kMaxCh]{}, ms = 0, a = 1e-4;
	int nch = 2;
};

// Auto-gain : égalise la sonie de sortie sur celle de l'entrée (portée ±18 dB), gelé sous -60 LUFS.
class AutoGain
{
public:
	void prepare (double fs) noexcept { a = onePole (1.2, fs); reset (); }
	void reset () noexcept { gDb = 0; }
	inline double process (double msIn, double msOut) noexcept
	{
		const double lIn = msIn <= 1e-12 ? -120.0 : -0.691 + 10.0 * std::log10 (msIn);
		if (lIn > -60.0 && msOut > 1e-12)
		{
			const double want = clampd (10.0 * std::log10 (msIn / msOut), -18.0, 18.0);
			gDb += (want - gDb) * a;
		}
		return gDb;
	}
	double gainDb () const noexcept { return gDb; }

private:
	double a = 1e-4, gDb = 0;
};
} // namespace tama
