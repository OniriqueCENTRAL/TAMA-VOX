// TAMA-VOX : détecteur de dynamique opto-chimique (cellule T4).
//
// Équation de relaxation non linéaire liée à l'énergie cumulée :
//     tau_release[n] = tau0 + k * g( integrale_{t-T..t} |x|^2 dt )
// où l'intégrale est une mémoire d'énergie continue (moyenne quadratique à fuite, constante T réglable) et
// g est un profil cubique de l'énergie normalisée (sépare nettement un débit rapide d'une phrase tenue) :
//     débit rapide (syllabes courtes, rap)  -> tau ~ 40-60 ms, la diction est préservée
//     phrase tenue (chant continu)           -> tau jusqu'à ~800 ms, aucun pompage audible
// Le paramètre « adapt » (0..1) mélange ce comportement et un release fixe médian.
//
// Un faible relèvement des bas niveaux (upward) est intégré au même détecteur : il évite d'écraser la
// dynamique avec des coupures multibandes rigides.
#pragma once
#include "../StdCommon.h"
#include "Common.h"

namespace tama
{
struct CompParams
{
	double thrDb = -25.0;   // seuil
	double ratio = 6.5;
	double kneeDb = 14.0;
	double attackMs = 3.0;
	double adapt = 1.0;     // 0 = release fixe, 1 = T4 adaptatif
	double relMinMs = 45.0; // tau0
	double relMaxMs = 800.0;
	double memoryMs = 350.0;
	double liftDb = 1.5;    // relèvement maximal des bas niveaux
};

class OptoDetector
{
public:
	void prepare (double fsBase) noexcept
	{
		fs = fsBase;
		aLevel = onePole (0.005, fs);
		aPeak = onePole (3.0, fs);
		reset ();
	}
	void reset () noexcept { lvl = 1e-9; energy = 1e-6; peakRef = 1e-6; gr = 0; lift = 0; tau = 0.045; }
	void set (const CompParams& p) noexcept { prm = p; aMem = onePole (std::max (0.02, p.memoryMs * 0.001), fs); aAtt = onePole (std::max (0.00005, p.attackMs * 0.001), fs); }

	// ms : énergie moyenne (x^2) du chemin stationnaire sur cet échantillon de base (canaux liés).
	// Retourne le gain en dB (négatif = réduction).
	inline double process (double ms) noexcept
	{
		lvl += (ms - lvl) * aLevel;
		energy += (ms - energy) * aMem;
		// profil de release : densité d'énergie = énergie cumulée / référence crête lente (mémoire de 3 s).
		// Voyelle tenue -> densité ~ 1 ; syllabes courtes séparées par des silences -> densité ~ 0,4-0,6.
		peakRef = std::max (lvl, peakRef - (peakRef - lvl) * aPeak);
		const double dens = energy / (peakRef + 1e-9);
		const double e = smoothstep (0.6, 0.95, dens);
		const double w = prm.adapt * e * e + (1.0 - prm.adapt) * 0.5;
		tau = (prm.relMinMs + (std::max (prm.relMaxMs, prm.relMinMs) - prm.relMinMs) * w) * 0.001;
		const double aRel = onePole (tau, fs);

		const double L = 10.0 * std::log10 (std::max (lvl, 1e-12)); // dB RMS du chemin stationnaire
		// calculateur de gain statique à genou doux
		const double over = L - prm.thrDb;
		double target;
		if (2.0 * over < -prm.kneeDb)
			target = 0.0;
		else if (2.0 * std::fabs (over) <= prm.kneeDb)
		{
			const double t = over + prm.kneeDb * 0.5;
			target = (1.0 - 1.0 / prm.ratio) * t * t / (2.0 * std::max (prm.kneeDb, 1e-6));
		}
		else
			target = over * (1.0 - 1.0 / prm.ratio);
		// ballistique en dB : attaque rapide, release adaptatif T4
		gr += (target - gr) * (target > gr ? aAtt : aRel);
		// relèvement doux des bas niveaux (au-dessus du plancher de bruit)
		const double lowT = prm.thrDb - 25.0;
		double tl = clampd ((lowT - L) / 12.0, 0.0, 1.0) * prm.liftDb * smoothstep (-78.0, -58.0, L);
		lift += (tl - lift) * aRel;
		return -gr + lift;
	}
	double reductionDb () const noexcept { return gr; }
	double tauSeconds () const noexcept { return tau; }
	double energyMemory () const noexcept { return energy; }

private:
	CompParams prm;
	double fs = 48000, aLevel = 0.01, aMem = 1e-4, aAtt = 0.01, aPeak = 1e-5, peakRef = 1e-6;
	double lvl = 1e-9, energy = 1e-6, gr = 0, lift = 0, tau = 0.045;
};
} // namespace tama
