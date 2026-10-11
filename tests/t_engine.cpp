#include "../src/dsp/TamaEngine.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
using namespace tama;
static double rmsDb (const std::vector<double>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return 10 * std::log10 (s / (b - a) + 1e-30); }
int main ()
{
	const double fs = 48000; const int N = (int) (fs * 6);
	TamaEngine e; e.prepare (fs, 2);
	std::vector<double> x (N), l (N), r (N);
	// signal vocal-ish : sinus 180 Hz + harmoniques modulés en enveloppe lente + salves de bruit (consonnes)
	std::mt19937 rng (1); std::normal_distribution<double> nd (0, 1);
	for (int i = 0; i < N; ++i)
	{
		double t = i / fs; double env = 0.5 + 0.5 * std::sin (2 * kPi * 0.7 * t); env *= env;
		double v = 0; for (int h = 1; h <= 12; ++h) v += std::sin (2 * kPi * 180 * h * t) / h;
		x[i] = 0.25 * env * v;
		if (std::fmod (t, 1.0) > 0.45 && std::fmod (t, 1.0) < 0.5) x[i] += 0.08 * nd (rng) * (1 - std::fabs (std::fmod (t, 1.0) - 0.475) / 0.025);
	}
	std::vector<double> y (N);
	for (int mode = 0; mode < 2; ++mode)
	{
		EngineParams p; p.mode = mode; e.setParams (p); e.reset ();
		auto t0 = std::chrono::steady_clock::now ();
		for (int o = 0; o < N; o += 512)
		{
			int n = std::min (512, N - o); const double* ip[2] = {&x[o], &x[o]}; double ol[512], orr[512]; double* op[2] = {ol, orr};
			e.process (ip, op, n, 2);
			for (int i = 0; i < n; ++i) y[o + i] = ol[i];
		}
		double sec = std::chrono::duration<double> (std::chrono::steady_clock::now () - t0).count ();
		const auto& m = e.meters ();
		bool bad = false; for (double v : y) if (!std::isfinite (v) || std::fabs (v) > 1.0001) bad = true;
		printf ("mode %s : CPU %.1f%% d'un coeur (stéréo, %g Hz) | in %.1f dB out %.1f dB | LUFS in %.1f out %.1f makeup %.2f dB | GR %.2f dB tau %.0f ms lamp %.2f duck %.1f sat %.2f %s\n",
			mode ? "PURE" : "SNAP", 100.0 * sec / (N / fs), fs, rmsDb (x, N / 2, N), rmsDb (y, N / 2, N), m.lufsIn, m.lufsOut, m.makeupDb, m.gr, m.tauMs, m.lamp, m.duck, m.sat, bad ? "NaN/clip!" : "ok");
	}
	// latence PURE : impulsion
	{
		EngineParams p; p.mode = 1; p.autoGain = false; p.drive = 1; e.setParams (p); e.reset ();
		std::vector<double> imp (4096, 0.0); imp[200] = 0.3; std::vector<double> o (4096);
		for (int k = 0; k < 4096; k += 256) { const double* ip[2] = {&imp[k], &imp[k]}; double a[256], b[256]; double* op[2] = {a, b}; e.process (ip, op, 256, 2); for (int i = 0; i < 256; ++i) o[k + i] = a[i]; }
		int pk = 0; for (int i = 0; i < 4096; ++i) if (std::fabs (o[i]) > std::fabs (o[pk])) pk = i;
		printf ("latence PURE mesurée : %d échantillons (déclarée %d)\n", pk - 200, e.latencySamples ());
	}
	// bypass/raw : sortie == entrée retardée
	{
		EngineParams p; p.mode = 1; p.raw = true; e.setParams (p); e.reset ();
		std::vector<double> o (N);
		for (int k = 0; k < N; k += 256) { int n = std::min (256, N - k); const double* ip[2] = {&x[k], &x[k]}; double a[256], b[256]; double* op[2] = {a, b}; e.process (ip, op, n, 2); for (int i = 0; i < n; ++i) o[k + i] = a[i]; }
		double err = 0; for (int i = 20000; i < N; ++i) err = std::max (err, std::fabs (o[i] - x[i - 64]));
		printf ("RAW (PURE) vs entrée retardée de 64 : erreur max %.2e\n", err);
	}
	// silence -> pas de NaN, pas de bruit
	{
		EngineParams p; e.setParams (p); e.reset (); std::vector<double> z (4800, 0.0), o (4800); const double* ip[2] = {z.data (), z.data ()}; double a[4800], b[4800]; double* op[2] = {a, b}; e.process (ip, op, 4800, 2);
		double mx = 0; for (int i = 0; i < 4800; ++i) mx = std::max (mx, std::fabs (a[i])); printf ("silence : sortie max %.2e\n", mx);
	}
}
