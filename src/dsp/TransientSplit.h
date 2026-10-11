// TAMA-VOX : séparation transitoires / corps stationnaire par filtrage médian en sous-bandes.
//
// Le signal est découpé en 5 sous-bandes complémentaires (découpe par soustraction : somme des bandes == x
// exactement). Pour chaque bande :
//   - enveloppe rapide eF,
//   - médiane temporelle de eF sur ~30 ms (estimation du « stationnaire », robuste aux pics),
//   - rapport r = eF / médiane -> masque brut u (0 = stationnaire, 1 = transitoire),
//   - médiane FRÉQUENTIELLE à 3 voisins sur u (un transitoire percussif est large bande, une attaque de
//     voyelle est localisée en fréquence) -> masque final m_k, lissé.
// transitoire t = somme m_k * bande_k ; stationnaire s = x - t. Donc t + s == x : « phase lock » parfait
// quand rien n'est traité, et aucune rotation de phase entre les deux chemins.
#pragma once
#include "../StdCommon.h"
#include "Biquad.h"

namespace tama
{
class TransientSplit
{
public:
	static constexpr int kBands = 5;

	void prepare (double fs) noexcept
	{
		static constexpr double fc[kBands - 1] = {220.0, 750.0, 2200.0, 5500.0};
		for (int i = 0; i < kBands - 1; ++i)
			lp[i].set (BiquadCoefs::lowpass (fs, fc[i], 0.7071067811865476));
		aAtt = onePole (0.0002, fs);
		aRel = onePole (0.004, fs);
		mAtt = onePole (0.0002, fs);
		mRel = onePole (0.003, fs);
		hop = std::max (1, static_cast<int> (0.002 * fs));
		reset ();
	}
	void reset () noexcept
	{
		for (auto& b : lp) b.reset ();
		for (int k = 0; k < kBands; ++k)
		{
			env[k] = 1e-4; mask[k] = 0; raw[k] = 0; med[k] = 1e-4;
			for (int j = 0; j < kHist; ++j) hist[k][j] = 1e-4;
		}
		hopCount = 0; histPos = 0; activity = 0;
	}

	// sortie : transitoire t ; le stationnaire est x - t
	inline double process (double x) noexcept
	{
		double band[kBands];
		double rest = x;
		for (int k = 0; k < kBands - 1; ++k)
		{
			band[k] = lp[k].process (rest);
			rest -= band[k];
		}
		band[kBands - 1] = rest;

		double t = 0, act = 0;
		for (int k = 0; k < kBands; ++k)
		{
			const double a = std::fabs (band[k]);
			env[k] += (a - env[k]) * (a > env[k] ? aAtt : aRel);
		}
		if (++hopCount >= hop)
		{
			hopCount = 0;
			for (int k = 0; k < kBands; ++k)
			{
				hist[k][histPos] = env[k];
				double tmp[kHist];
				for (int j = 0; j < kHist; ++j) tmp[j] = hist[k][j];
				std::sort (tmp, tmp + kHist);
				med[k] = std::max (tmp[kHist / 2], 1e-7);
				const double r = env[k] / med[k];
				raw[k] = smoothstep (1.6, 4.5, r);
			}
			histPos = (histPos + 1) % kHist;
		}
		for (int k = 0; k < kBands; ++k)
		{
			const double l = raw[k > 0 ? k - 1 : 0], c = raw[k], r = raw[k < kBands - 1 ? k + 1 : kBands - 1];
			const double fm = std::max (std::min (l, c), std::min (std::max (l, c), r)); // médiane de 3
			mask[k] += (fm - mask[k]) * (fm > mask[k] ? mAtt : mRel);
			t += mask[k] * band[k];
			act = std::max (act, mask[k]);
		}
		activity = act;
		return t;
	}
	double transientActivity () const noexcept { return activity; }

private:
	static constexpr int kHist = 15;
	Biquad lp[kBands - 1];
	double env[kBands], mask[kBands], raw[kBands], med[kBands], hist[kBands][kHist];
	double aAtt = 0.1, aRel = 0.01, mAtt = 0.1, mRel = 0.01, activity = 0;
	int hop = 96, hopCount = 0, histPos = 0;
};
} // namespace tama
