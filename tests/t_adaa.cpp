#include "../src/dsp/AdaaSaturator.h"
#include <cstdio>
#include <vector>
using namespace tama;
// vérifie F1' = f et F2' = F1 numériquement
template <class NL> void checkDeriv (const char* n)
{
	double e1 = 0, e2 = 0, h = 1e-5;
	for (double x = -3; x <= 3; x += 0.0137) { e1 = std::max (e1, std::fabs ((NL::F1 (x + h) - NL::F1 (x - h)) / (2 * h) - NL::f (x))); e2 = std::max (e2, std::fabs ((NL::F2 (x + h) - NL::F2 (x - h)) / (2 * h) - NL::F1 (x))); }
	printf ("%s: |F1'-f| max %.2e   |F2'-F1| max %.2e   f'(0)=%.4f\n", n, e1, e2, (NL::f (h) - NL::f (-h)) / (2 * h));
}
// erreur de repliement : sortie du processeur vs référence calculée à 16x puis filtrée idéalement (bins < Nyquist)
template <class NLf, class P> double aliasErrDb (NLf&& nl, P&& proc, double fs, int kbin, int N, double amp, bool delay = false)
{
	const int R = 16; const double f0 = fs * kbin / N;
	std::vector<double> y (N), r (N * R);
	for (int i = 0; i < N; ++i) y[i] = proc (amp * std::sin (2 * kPi * f0 / fs * i));
	for (int i = 0; i < N * R; ++i) r[i] = nl (amp * std::sin (2 * kPi * f0 / (fs * R) * (i - (delay ? R : 0))));
	double err = 0, ref = 0; std::vector<double> pr (N / 2), py (N / 2);
	for (int k = 1; k < N / 2; ++k)
	{
		double a = 0, b = 0, c = 0, d = 0; const double w = 2 * kPi * k / N;
		for (int i = 0; i < N; ++i) { a += y[i] * std::cos (w * i); b -= y[i] * std::sin (w * i); }
		for (int i = 0; i < N * R; ++i) { c += r[i] * std::cos (w * i / R); d -= r[i] * std::sin (w * i / R); }
		c /= R; d /= R;
		pr[k] = c * c + d * d; py[k] = a * a + b * b; ref += pr[k];
	}
	double mx = 0; for (int k = 1; k < N / 2; ++k) mx = std::max (mx, pr[k]);
	for (int k = 1; k < N / 2; ++k) if (pr[k] < 1e-9 * mx) err += py[k];   // bins sans harmonique vrai : repliement pur
	return 10 * std::log10 (err / ref + 1e-30);
}
int main ()
{
	checkDeriv<adaa::Pink> ("Pink"); checkDeriv<adaa::Blue> ("Blue"); checkDeriv<adaa::Black> ("Black");
	const double fs = 96000; const int N = 1024, kb = 241; // f0 = 6656 Hz, bin exact
	{
		Adaa2State<adaa::Pink> s; auto nl = [] (double x) { return adaa::Pink::f (x); };
		printf ("erreur repliement Pink  : naif %.1f dB  ADAA2 %.1f dB\n", aliasErrDb (nl, nl, fs, kb, N, 1.6), aliasErrDb (nl, [&] (double x) { return s.process (x); }, fs, kb, N, 1.6, true));
		Adaa2State<adaa::Blue> b; auto nb = [] (double x) { return adaa::Blue::f (x); };
		printf ("erreur repliement Blue  : naif %.1f dB  ADAA2 %.1f dB\n", aliasErrDb (nb, nb, fs, kb, N, 1.6), aliasErrDb (nb, [&] (double x) { return b.process (x); }, fs, kb, N, 1.6, true));
		Adaa2State<adaa::Black> k; auto nk = [] (double x) { return adaa::Black::f (x); };
		printf ("erreur repliement Black : naif %.1f dB  ADAA2 %.1f dB\n", aliasErrDb (nk, nk, fs, kb, N, 1.6), aliasErrDb (nk, [&] (double x) { return k.process (x); }, fs, kb, N, 1.6, true));
	}
	// continuité aux petits signaux : y ~ x
	Adaa2State<adaa::Pink> s; double e = 0; for (int i = 0; i < 2000; ++i) { double x = 0.01 * std::sin (0.05 * i); double y = s.process (x); if (i > 5) e = std::max (e, std::fabs (y - x)); }
	printf ("petit signal Pink: erreur max %.2e (attendu < 2e-4)\n", e);
	ColorTanh c; c.set (2.0, 0.1); double m = 0; for (int i = 0; i < 2000; ++i) { double x = 0.001 * std::sin (0.05 * i); double y = c.process (x); if (i > 5) m = std::max (m, std::fabs (y - x)); } printf ("ColorTanh petit signal: %.2e\n", m);
	// silence/constante : pas de NaN
	Adaa2State<adaa::Black> z; double acc = 0; for (int i = 0; i < 100; ++i) acc += z.process (0.3); printf ("constante: y=%.6f f(0.3)=%.6f\n", z.process (0.3), adaa::Black::f (0.3));
}
