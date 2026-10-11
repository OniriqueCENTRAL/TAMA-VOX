// TAMA-VOX : Smart Air Shelf + de-esser (domaine 8x).
//
//  - Générateur de partiels purs : la bande 4-8 kHz est redressée/élevée au carré puis filtrée 8-18 kHz
//    (harmoniques 2 et 3 de la bande), ce qui injecte de l'« air » sans simple égaliseur.
//  - Étagère d'air dynamique 8 kHz et plus (jusqu'à +5 dB) réalisée par addition linéaire de la composante passe-haut.
//  - Détecteur d'énergie différentielle : énergie 5-8 kHz comparée au corps 1-3 kHz plus sa dérivée. Dès qu'une
//    sibilance (s, z, ch) est détectée, l'air est coupé de 4 à 8 dB avec une attaque < 0,5 ms (gain lissé à
//    0,15 ms), puis rouvert sur les voyelles (release ~ 18 ms). Un cran de de-essing 6,5 kHz s'y ajoute.
#pragma once
#include "../StdCommon.h"
#include "Biquad.h"

namespace tama
{
class AirDeesser
{
public:
	void prepare (double fs8) noexcept
	{
		sibBand.set (BiquadCoefs::bandpass (fs8, 6300.0, 1.4));
		bodyBand.set (BiquadCoefs::bandpass (fs8, 1800.0, 0.5));
		gen1.set (BiquadCoefs::bandpass (fs8, 5800.0, 1.2));
		genHp.set (BiquadCoefs::highpass (fs8, 8000.0, 0.7071));
		genLp.set (BiquadCoefs::lowpass (fs8, 18000.0, 0.7071));
		airHp1.set (BiquadCoefs::highpass (fs8, 9000.0, 0.7071));
		airHp2.set (BiquadCoefs::highpass (fs8, 9000.0, 0.7071));
		deBp.set (BiquadCoefs::bandpass (fs8, 6500.0, 1.2));
		eAtt = onePole (0.0001, fs8);
		eRel = onePole (0.008, fs8);
		bAtt = onePole (0.003, fs8);
		bRel = onePole (0.03, fs8);
		bRelFast = onePole (0.002, fs8);
		gAtt = onePole (0.00015, fs8);
		gRel = onePole (0.018, fs8);
		reset ();
	}
	void reset () noexcept
	{
		sibBand.reset (); bodyBand.reset (); gen1.reset (); genHp.reset (); genLp.reset (); airHp1.reset (); airHp2.reset (); deBp.reset ();
		eH = eB = 1e-6; eHslow = 1e-6; sib = 0; sibTarget = 0; duck = 1.0; ctl = 0;
	}
	// airDb : étagère d'air (0..8), genAmount : mélange du générateur (0..1)
	void setAmounts (double airDb, double genAmount) noexcept { airLin = dbToLin (airDb) - 1.0; gen = genAmount; }

	inline double process (double x) noexcept
	{
		// détecteur différentiel : enveloppes par échantillon, décision (log/pow) tous les kCtl échantillons
		const double s = sibBand.process (x), b = bodyBand.process (x);
		const double as = std::fabs (s), ab = std::fabs (b);
		eH += (as - eH) * (as > eH ? eAtt : eRel);
		eB += (ab - eB) * (ab > eB ? bAtt : bRelFast);
		eHslow += (eH - eHslow) * bRel;
		if (++ctl >= kCtl)
		{
			ctl = 0;
			const double ratio = eH / (eB * 0.7 + 1e-5);                         // sibilante >> corps
			const double diff = std::max (0.0, eH - eHslow) / (eHslow + 1e-5);   // montée brusque (différentiel)
			const double presence = smoothstep (-60.0, -45.0, 20.0 * std::log10 (eH + 1e-9));
			sibTarget = presence * std::max (smoothstep (1.2, 3.0, ratio), 0.7 * smoothstep (0.6, 2.0, diff) * smoothstep (0.25, 0.7, ratio));
		}
		sib += (sibTarget - sib) * (sibTarget > sib ? gAtt : gRel);
		// 4 à 8 dB de coupure selon la sibilance
		const double depthDb = (4.0 + 4.0 * sib) * sib;
		duck += (std::exp2 (-depthDb * 0.16609640474436813) - duck) * 0.02; // 2^(-dB/6.0206)
		const double gb = gen1.process (x);
		const double partial = genLp.process (genHp.process (gb * gb * 3.0));
		const double air = airHp2.process (airHp1.process (x));
		double y = x + air * airLin * duck + partial * gen * duck;
		y -= deBp.process (x) * (1.0 - std::exp2 (-4.0 * sib * 0.16609640474436813)) * 0.8;
		return y;
	}
	double sibilance () const noexcept { return sib; }
	double airDuckDb () const noexcept { return (4.0 + 4.0 * sib) * sib; }

private:
	Biquad sibBand, bodyBand, gen1, genHp, genLp, airHp1, airHp2, deBp;
	double eAtt = 0.1, eRel = 0.01, bAtt = 0.01, bRel = 0.001, bRelFast = 0.01, gAtt = 0.1, gRel = 0.01;
	static constexpr int kCtl = 8;
	double eH = 1e-6, eB = 1e-6, eHslow = 1e-6, sib = 0, sibTarget = 0, duck = 1.0, airLin = 0.78, gen = 0.2;
	int ctl = 0;
};
} // namespace tama
