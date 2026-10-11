#include "../src/dsp/Oversampling.h"
#include <cstdio>
#include <vector>
using namespace tama;
int main ()
{
	for (double fs : {44100.0, 48000.0, 96000.0})
	{
		Oversampler8x os; os.prepare (fs);
		const int N = 8192; std::vector<double> x (N), up (N * 8), y (N), y2 (N);
		// impulsion : latence
		for (int mode = 0; mode < 2; ++mode)
		{
			os.reset ();
			std::fill (x.begin (), x.end (), 0.0); x[100] = 1.0;
			for (int o = 0; o < N; o += 256)
			{
				if (mode) os.upFir (&x[o], 256, &up[o * 8]); else os.upIir (&x[o], 256, &up[o * 8]);
				if (mode) os.downFir (&up[o * 8], 256, &y[o]); else os.downIir (&up[o * 8], 256, &y[o]);
			}
			int pk = 0; for (int i = 0; i < N; ++i) if (std::fabs (y[i]) > std::fabs (y[pk])) pk = i;
			// réponse en fréquence (gain à plusieurs fréquences) avec sinus
			double g[4]; int k = 0;
			for (double f : {1000.0, 10000.0, 18000.0, 20000.0})
			{
				os.reset ();
				double s1 = 0, s2 = 0;
				std::vector<double> xs (N);
				for (int i = 0; i < N; ++i) xs[i] = std::sin (2 * kPi * f / fs * i);
				for (int o = 0; o < N; o += 256)
				{
					if (mode) os.upFir (&xs[o], 256, &up[o * 8]); else os.upIir (&xs[o], 256, &up[o * 8]);
					if (mode) os.downFir (&up[o * 8], 256, &y2[o]); else os.downIir (&up[o * 8], 256, &y2[o]);
				}
				for (int i = 4096; i < N; ++i) { s1 += xs[i] * xs[i]; s2 += y2[i] * y2[i]; }
				g[k++] = 10 * std::log10 (s2 / s1);
			}
			// rejet des images : sinus 15k suréchantillonné, mesure de l'énergie à 8x au-dessus de Nyquist d'origine
			os.reset ();
			double f0 = std::min (15000.0, 0.34 * fs);
			std::vector<double> xs (N);
			for (int i = 0; i < N; ++i) xs[i] = std::sin (2 * kPi * f0 / fs * i);
			for (int o = 0; o < N; o += 256) { if (mode) os.upFir (&xs[o], 256, &up[o * 8]); else os.upIir (&xs[o], 256, &up[o * 8]); }
			// DFT naïve de l'énergie hors de la bande utile : on projette sur la sinusoïde pure et regarde le résidu
			double num = 0, den = 0, ca = 0, sa = 0; int M = N * 8, from = 8192 * 4, cnt = 0;
			for (int i = from; i < M; ++i) { ca += up[i] * std::cos (2 * kPi * f0 / (fs * 8) * i); sa += up[i] * std::sin (2 * kPi * f0 / (fs * 8) * i); ++cnt; }
			ca *= 2.0 / cnt; sa *= 2.0 / cnt;
			for (int i = from; i < M; ++i) { double r = up[i] - ca * std::cos (2 * kPi * f0 / (fs * 8) * i) - sa * std::sin (2 * kPi * f0 / (fs * 8) * i); num += r * r; den += up[i] * up[i]; }
			printf ("fs %.0f %s: latence pic=%d (iirStage1 ordre %d)  gain dB @1k %.3f @10k %.3f @18k %.3f @20k %.3f  residu images %.1f dB\n", fs, mode ? "FIR" : "IIR", pk - 100, os.iirOrderStage1 (), g[0], g[1], g[2], g[3], 10 * std::log10 (num / den + 1e-30));
		}
	}
}
