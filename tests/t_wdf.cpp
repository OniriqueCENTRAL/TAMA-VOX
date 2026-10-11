#include "../src/dsp/WdfTransformer.h"
#include <cstdio>
#include <vector>
using namespace tama;
// THD (harmoniques 2..9) et gain relatif d'un sinus passé à travers le transformateur
static void probe (double fs, double f, double amp, double gain, double* thd, double* gaindb, double* h2db, double* h3db)
{
	WdfTransformer w; w.prepare (fs);
	const int per = (int) std::round (fs / f), P = 40; const int N = per * P;
	double fe = fs / per; // fréquence exacte
	std::vector<double> y (N);
	for (int i = 0; i < N; ++i) y[i] = w.process (amp * std::sin (2 * kPi * fe * i / fs), gain);
	auto comp = [&] (int h) { double re = 0, im = 0; for (int i = N / 2; i < N; ++i) { re += y[i] * std::cos (2 * kPi * fe * h * i / fs); im += y[i] * std::sin (2 * kPi * fe * h * i / fs); } return std::hypot (re, im) * 2.0 / (N / 2); };
	const double f1 = comp (1); double s = 0; for (int h = 2; h <= 9; ++h) s += comp (h) * comp (h);
	*thd = 20 * std::log10 (std::sqrt (s) / f1 + 1e-12); *gaindb = 20 * std::log10 (f1 / amp); *h2db = 20 * std::log10 (comp (2) / f1 + 1e-12); *h3db = 20 * std::log10 (comp (3) / f1 + 1e-12);
}
int main ()
{
	const double fs = 48000 * 8;
	printf ("freq   gain(in)  | gain dB   THD dB    H2 dB    H3 dB\n");
	for (double g : {0.5, 1.0, 2.0, 4.0})
		for (double f : {60.0, 120.0, 250.0, 500.0, 1000.0, 4000.0})
		{
			double thd, gd, h2, h3; probe (fs, f, 0.5, g, &thd, &gd, &h2, &h3);
			printf ("%5.0f  %4.1f      | %7.2f  %8.1f  %8.1f  %8.1f\n", f, g, gd, thd, h2, h3);
		}
	// hystérésis : aire de la boucle B(H) pour un sinus 250 Hz fort
	WdfTransformer w; w.prepare (fs);
	double area = 0, hp = 0, bp = 0; int per = (int) (fs / 250.0);
	for (int i = 0; i < per * 20; ++i) { w.process (std::sin (2 * kPi * 250.0 * i / fs), 3.0); if (i > per * 10) { area += 0.5 * (w.lastB () + bp) * (w.lastH () - hp); } hp = w.lastH (); bp = w.lastB (); }
	printf ("boucle hystérésis : aire signée sur 10 périodes = %.4f (non nulle => mémoire)\n", area);
	// stabilité : bruit blanc fort + silence ensuite
	WdfTransformer s; s.prepare (fs); unsigned r = 1; double mx = 0; for (int i = 0; i < 400000; ++i) { r = r * 1664525u + 1013904223u; double x = ((r >> 8) / 8388608.0 - 1.0); mx = std::max (mx, std::fabs (s.process (x, 4.0))); }
	double tail = 0; for (int i = 0; i < 100000; ++i) tail = std::max (tail, std::fabs (s.process (0, 4.0)));
	printf ("bruit fort : crête sortie %.2f ; queue après silence %.2e\n", mx, tail);
}
