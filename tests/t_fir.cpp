#include "../src/dsp/Oversampling.h"
#include <cstdio>
using namespace tama;
// gain d'un étage FIR up->down à sa fréquence basse fl pour sinus f
static double gainStage (int m, double beta, double fl, double f)
{
	FirHalfband h; h.prepare (m, beta);
	const int N = 4096; std::vector<double> x (N), up (2 * N), y (N);
	for (int i = 0; i < N; ++i) x[i] = std::sin (2 * kPi * f / fl * i);
	h.up (x.data (), N, up.data ()); h.down (up.data (), N, y.data ());
	double a = 0, b = 0; for (int i = 2048; i < N; ++i) { a += x[i] * x[i]; b += y[i] * y[i]; }
	return 10 * std::log10 (b / a);
}
int main ()
{
	for (double beta : {3.0, 5.0, 8.0, 10.0})
		for (int m : {4, 8, 28})
		{
			double fl = (m == 28) ? 44100 : (m == 8 ? 88200 : 176400);
			printf ("beta %.0f m %d: @10k %.3f @18k %.3f @20k %.3f\n", beta, m, gainStage (m, beta, fl, 10000), gainStage (m, beta, fl, 18000), gainStage (m, beta, fl, 20000));
		}
}
