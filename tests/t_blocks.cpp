#include "../src/dsp/TamaEngine.h"
#include <cstdio>
#include <random>
#include <vector>
using namespace tama;
int main ()
{
	const double fs = 48000;
	// 1) LUFS : sinus 997 Hz, -23 dBFS crête, stéréo -> -23.0 LUFS
	{
		LoudnessMeter lm; lm.prepare (fs, 2); const double A = dbToLin (-23.0);
		for (int i = 0; i < (int) (fs * 3); ++i) { double s = A * std::sin (2 * kPi * 997.0 * i / fs); lm.push (0, s); lm.push (1, s); lm.tick (); }
		printf ("LUFS 997Hz -23 dBFS stéréo : %.2f (attendu -23.0)\n", lm.lufs ());
	}
	// 2) séparation : somme t + s == x ; clic -> transitoire ; sinus tenu -> stationnaire
	{
		const double f8 = fs * 8; TransientSplit sp; sp.prepare (f8);
		std::vector<double> x ((int) (f8 * 0.6)); for (size_t i = 0; i < x.size (); ++i) x[i] = 0.2 * std::sin (2 * kPi * 300.0 * i / f8) + 0.1 * std::sin (2 * kPi * 2500.0 * i / f8);
		size_t clickAt = (size_t) (f8 * 0.4);
		for (size_t k = 0; k < 400; ++k) x[clickAt + k] += 0.8 * std::exp (-(double) k / 80.0) * ((k & 1) ? 1 : -1); // clic large bande
		double eT0 = 0, eT1 = 0, maxAct = 0, recErr = 0;
		for (size_t i = 0; i < x.size (); ++i)
		{
			double t = sp.process (x[i]); double s = x[i] - t; recErr = std::max (recErr, std::fabs (t + s - x[i]));
			if (i > f8 * 0.2 && i < f8 * 0.38) { eT0 += t * t; } if (i >= clickAt && i < clickAt + 2000) { eT1 += t * t; maxAct = std::max (maxAct, sp.transientActivity ()); }
		}
		printf ("séparation : reconstruction %.1e ; énergie t sur sinus tenu %.2e vs sur clic %.2e (x%.0f) ; activité max %.2f\n", recErr, eT0 / (f8 * 0.18), eT1 / 2000, (eT1 / 2000) / (eT0 / (f8 * 0.18) + 1e-30), maxAct);
	}
	// 3) release adaptatif : débit rapide (salves de 120 ms séparées) vs voyelle tenue
	{
		for (int tenu = 0; tenu < 2; ++tenu)
		{
			OptoDetector d; d.prepare (fs); CompParams p; d.set (p);
			double tauMin = 1e9, tauMax = 0, tauSum = 0; long tauN = 0;
			for (int i = 0; i < (int) (fs * 4); ++i)
			{
				double t = i / fs; bool on = tenu ? true : (std::fmod (t, 0.25) < 0.12);
				double a = on ? 0.2 : 0.0; d.process (a * a / 2 * (tenu ? 1 : 1)); if (t > 1.0) { tauMin = std::min (tauMin, d.tauSeconds ()); tauMax = std::max (tauMax, d.tauSeconds ()); tauSum += d.tauSeconds (); ++tauN; }
			}
			printf ("opto %s : tau release %.0f - %.0f ms, moyenne %.0f ms (attendu rap 40-60 ms .. tenu jusqu'à 800)\n", tenu ? "voyelle tenue " : "débit rapide  ", tauMin * 1000, tauMax * 1000, 1000 * tauSum / tauN);
		}
	}
	// 4) air / de-esser : voyelle (500 Hz + harmoniques) puis sifflante (bruit filtré 6-8k) : duck
	{
		const double f8 = fs * 8; AirDeesser a; a.prepare (f8); a.setAmounts (5.0, 0.2);
		std::mt19937 rng (2); std::normal_distribution<double> nd (0, 1); Biquad bp1, bp2; bp1.set (BiquadCoefs::bandpass (f8, 6500, 2)); bp2.set (BiquadCoefs::bandpass (f8, 6500, 2));
		double sibMax = 0, vowelDuck = 0, tAtt = -1;
		for (size_t i = 0; i < (size_t) (f8 * 0.5); ++i)
		{
			double t = i / f8, x = 0;
			if (t < 0.25) { for (int h = 1; h <= 8; ++h) x += std::sin (2 * kPi * 400 * h * t) / h; x *= 0.12; }
			else x = 0.25 * bp2.process (bp1.process (nd (rng)));
			a.process (x);
			if (t < 0.25) { if (t > 0.05) vowelDuck = std::max (vowelDuck, a.airDuckDb ()); } else { sibMax = std::max (sibMax, a.airDuckDb ()); if (tAtt < 0 && a.airDuckDb () > 3.0) tAtt = (t - 0.25) * 1000; }
		}
		printf ("air : coupure max sur voyelle %.2f dB ; sur sifflante %.2f dB ; 3 dB atteints après %.2f ms\n", vowelDuck, sibMax, tAtt);
	}
	// 5) changement de mode en pleine lecture : saut maximal entre échantillons (clic)
	{
		TamaEngine e; e.prepare (fs, 2); EngineParams p; p.autoGain = false; p.drive = 1; p.squeeze = 0; e.setParams (p); e.reset ();
		const int N = (int) (fs * 1.0); std::vector<double> y (N), x (N); for (int i = 0; i < N; ++i) x[i] = 0.3 * std::sin (2 * kPi * 440.0 * i / fs);
		for (int k = 0; k < N; k += 128)
		{
			if (k == 24000) { p.mode = 1; e.setParams (p); } if (k == 40000) { p.mode = 0; e.setParams (p); }
			const double* ip[2] = {&x[k], &x[k]}; double a[128], b[128]; double* op[2] = {a, b}; e.process (ip, op, std::min (128, N - k), 2); for (int i = 0; i < std::min (128, N - k); ++i) y[k + i] = a[i];
		}
		double jmpBefore = 0, jmpSwitch = 0; for (int i = 20000; i < 23000; ++i) jmpBefore = std::max (jmpBefore, std::fabs (y[i + 1] - y[i]));
		for (int i = 24000; i < 24300; ++i) jmpSwitch = std::max (jmpSwitch, std::fabs (y[i + 1] - y[i]));
		for (int i = 40000; i < 40300; ++i) jmpSwitch = std::max (jmpSwitch, std::fabs (y[i + 1] - y[i]));
		printf ("commutation SNAP<->PURE : saut max %.4f (régime établi %.4f) -> %s\n", jmpSwitch, jmpBefore, jmpSwitch < 2.5 * jmpBefore ? "sans clic" : "CLIC");
	}
	// 6) changement de saveur : saut maximal
	{
		TamaEngine e; e.prepare (fs, 2); EngineParams p; p.autoGain = false; e.setParams (p); e.reset ();
		const int N = (int) (fs * 0.5); std::vector<double> y (N), x (N); for (int i = 0; i < N; ++i) x[i] = 0.3 * std::sin (2 * kPi * 330.0 * i / fs);
		for (int k = 0; k < N; k += 128)
		{
			if (k == 12000) { p.flavor = 2; e.setParams (p); }
			const double* ip[2] = {&x[k], &x[k]}; double a[128], b[128]; double* op[2] = {a, b}; e.process (ip, op, std::min (128, N - k), 2); for (int i = 0; i < std::min (128, N - k); ++i) y[k + i] = a[i];
		}
		double j0 = 0, j1 = 0; for (int i = 9000; i < 11000; ++i) j0 = std::max (j0, std::fabs (y[i + 1] - y[i])); for (int i = 12000; i < 12600; ++i) j1 = std::max (j1, std::fabs (y[i + 1] - y[i]));
		printf ("changement de saveur : saut max %.4f (avant %.4f)\n", j1, j0);
	}
}
