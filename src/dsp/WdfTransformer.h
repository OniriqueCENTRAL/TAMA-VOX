// TAMA-VOX : transformateur magnétique à noyau fer/nickel, modélisé par Wave Digital Filter.
//
// Schéma : source de tension Vs (résistance série Rs) --- noeud --- [inductance de magnétisation NON LINÉAIRE]
//                                                          \-- charge RL
//   arbre WDF : adaptateur parallèle (source résistive, résistance de charge) dont le port amont est
//   sans réflexion et branché sur la racine = inductance non linéaire à hystérésis (Jiles-Atherton simplifié).
//   L'inductance est discrétisée par la règle des trapèzes (variable d'état = flux, lambda).
//
// Jiles-Atherton simplifié (normalisé : H, M, B sans unité) :
//   He = H + alpha M,  Man = tanh(He/a),  dMirr/dH = delta_m (Man - Mirr) / (k delta - alpha (Man - Mirr))
//   M = c Man + (1-c) Mirr,  B = M + eps H
// La racine est résolue sans itération : linéarisation de la pente dB/dH au point précédent (la sens du
// flux fixe delta), ce qui donne une équation du premier degré en v. L'état d'aimantation passé reste
// mémorisé (Mirr, H, M) : la saturation dépend de l'historique.
//
// Effet audible : dans le bas-médium (180-320 Hz) le flux est grand (lambda ~ V/(2 pi f)), le noyau
// sature d'abord là : assise et densité, harmoniques impairs doux, légère compression du grave.
#pragma once
#include "../StdCommon.h"
#include "Common.h"

namespace tama
{
// ---- éléments WDF ----
struct WdfResistiveVoltageSource
{
	double R = 1.0, Vs = 0.0;
	double wave () const noexcept { return Vs; } // onde émise vers l'adaptateur : b = Vs
};
struct WdfResistor
{
	double R = 1.0;
	double wave () const noexcept { return 0.0; } // résistance adaptée : b = 0
};
// Adaptateur parallèle à deux ports enfants, port amont sans réflexion
struct WdfParallelAdaptor2
{
	double R1 = 1, R2 = 1, Rup = 1, g1 = 0.5, g2 = 0.5;
	void setPorts (double r1, double r2) noexcept
	{
		R1 = r1; R2 = r2;
		const double G1 = 1.0 / R1, G2 = 1.0 / R2, Gu = G1 + G2;
		Rup = 1.0 / Gu; g1 = G1 / Gu; g2 = G2 / Gu;
	}
	double upWave (double a1, double a2) const noexcept { return g1 * a1 + g2 * a2; }
};

class WdfTransformer
{
public:
	void prepare (double fs, double lowCornerHz = 22.0) noexcept
	{
		T = 1.0 / fs;
		rs = 1.0; rl = 20.0;
		adaptor.setPorts (rs, rl);
		lambdaS = 1.0 / (2.0 * kPi * 70.0); // flux de saturation : un sinus de 1 V sature vers 70 Hz
		const double chi0 = c / a_ + 1e-3;
		L0 = adaptor.Rup / (2.0 * kPi * lowCornerHz);
		Is = lambdaS * (chi0 + eps) / L0;
		reset ();
	}
	void reset () noexcept { hPrev = 0; mIrr = 0; m = 0; vPrev = 0; dirSign = 1.0; }

	// drive : gain d'entrée en volts normalisés (1 = le noyau sature vers 70 Hz)
	inline double process (double x, double inGain) noexcept
	{
		src.Vs = x * inGain;
		// onde incidente sur la racine
		const double a = adaptor.upWave (src.wave (), res.wave ());
		const double R0 = adaptor.Rup;
		// pente de l'aimantation au point précédent selon le sens du flux
		const double delta = vPrev >= 0 ? 1.0 : -1.0;
		const double He = hPrev + alpha * m;
		const double man = std::tanh (He / a_);
		const double dMan = (1.0 - man * man) / a_;
		double dMirr = 0.0;
		const double diff = man - mIrr;
		if (diff * delta > 0.0)
			dMirr = diff / (k * delta - alpha * diff) * delta; // dMirr/dH >= 0
		dMirr = clampd (dMirr, 0.0, 40.0);
		const double chi = c * dMan + (1.0 - c) * dMirr;
		// équation de la racine : v = a - R0 i, i = Is (hPrev + dh), dh = (T/2)(v + vPrev) / (lambdaS (chi + eps))
		const double kap = Is * T / (2.0 * lambdaS * (chi + eps));
		const double v = (a - R0 * (Is * hPrev + kap * vPrev)) / (1.0 + R0 * kap);
		double dh = (T * 0.5) * (v + vPrev) / (lambdaS * (chi + eps));
		dh = clampd (dh, -2.0, 2.0);
		// mise à jour de l'état magnétique
		double hNew = clampd (hPrev + dh, -hMax, hMax);
		dh = hNew - hPrev;
		mIrr = clampd (mIrr + dMirr * dh, -1.0, 1.0);
		const double HeN = hNew + alpha * m;
		m = c * std::tanh (HeN / a_) + (1.0 - c) * mIrr;
		hPrev = hNew;
		vPrev = v;
		lastFlux = (m + eps * hNew); // B normalisé (indication de saturation)
		return v / (inGain * adaptor.g1 + 1e-12);
	}
	double saturation () const noexcept { return std::fabs (lastFlux); }
	double lastH () const noexcept { return hPrev; }
	double lastB () const noexcept { return lastFlux; }

private:
	WdfResistiveVoltageSource src;
	WdfResistor res;
	WdfParallelAdaptor2 adaptor;
	double T = 1.0 / 384000.0, rs = 1.0, rl = 20.0, lambdaS = 7.9e-4, L0 = 1e-3, Is = 1.0;
	// paramètres Jiles-Atherton normalisés
	const double a_ = 0.35, alpha = 0.01, k = 0.25, c = 0.2, eps = 0.02, hMax = 8.0;
	double hPrev = 0, mIrr = 0, m = 0, vPrev = 0, dirSign = 1.0, lastFlux = 0;
};
} // namespace tama
