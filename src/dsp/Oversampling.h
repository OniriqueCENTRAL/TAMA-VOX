// TAMA-VOX : suréchantillonnage 8x = cascade de 3 étages demi-bande (x2).
//   SNAP : IIR polyphase de deux chaînes d'allpass (elliptique demi-bande, phase minimale, latence 0 déclarée)
//   PURE : FIR demi-bande symétrique à phase linéaire (Kaiser), latence fixe de 64 échantillons à Fs
// Les deux chaînes tournent toujours en parallèle (états chauds) : la commutation SNAP/PURE ne coûte qu'un crossfade.
// Toute la mémoire est allouée dans prepare() ; aucun appel à process n'alloue.
#pragma once
#include "../StdCommon.h"
#include "Common.h"
#include "Simd.h"
#include <vector>

namespace tama
{
//------------------------------------------------------------------------
// Conception analytique d'un demi-bande IIR elliptique (coefficients des allpass du 1er ordre).
// tbw : largeur de transition normalisée à la fréquence haute, de 0 à 0.5.
//------------------------------------------------------------------------
struct IirHalfbandDesign
{
	static constexpr int kMaxCoefs = 12;

	static double nome (double k) noexcept
	{
		const double kk = std::pow (1.0 - k * k, 0.25), e = 0.5 * (1.0 - kk) / (1.0 + kk);
		const double e2 = e * e, e4 = e2 * e2;
		return e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
	}

	// écrit n coefficients croissants dans c[]
	static void coefs (int n, double tbw, double* c) noexcept
	{
		const double kt = std::tan ((1.0 - tbw * 2.0) * kPi / 4.0), k = kt * kt, q = nome (k);
		const int order = n * 2 + 1;
		for (int i = 0; i < n; ++i)
		{
			const int cc = i + 1;
			double num = 0, sgn = 1, t;
			for (int j = 0; j < 40; ++j, sgn = -sgn)
			{
				t = std::pow (q, j * (j + 1)) * std::sin ((2 * j + 1) * cc * kPi / order) * sgn;
				num += t;
				if (std::fabs (t) < 1e-100)
					break;
			}
			double den = 0;
			sgn = -1;
			for (int j = 1; j < 40; ++j, sgn = -sgn)
			{
				t = std::pow (q, j * j) * std::cos (2 * j * cc * kPi / order) * sgn;
				den += t;
				if (std::fabs (t) < 1e-100)
					break;
			}
			den = den * 2.0 + 1.0;
			const double ww = 2.0 * num * std::pow (q, 0.25) / den, w2 = ww * ww;
			const double x = std::sqrt (std::max (0.0, (1.0 - w2 * k) * (1.0 - w2 / k))) / (1.0 + w2);
			c[i] = (1.0 - x) / (1.0 + x);
		}
		std::sort (c, c + n);
	}

	// réponse en amplitude d'un jeu de coefficients à la pulsation w (rad/éch. à la fréquence haute)
	static double magnitude (const double* c, int n, double w) noexcept
	{
		double ar0 = 1, ai0 = 0, ar1 = 1, ai1 = 0; // phases des deux chaînes (module 1)
		auto chain = [&] (int start, double& pr, double& pi_) {
			double re = 1, im = 0;
			for (int i = start; i < n; i += 2)
			{
				const double a = c[i], zr = std::cos (-2 * w), zi = std::sin (-2 * w);
				const double nr = a + zr, ni = zi, dr = 1 + a * zr, di = a * zi;
				const double dd = dr * dr + di * di, qr = (nr * dr + ni * di) / dd, qi = (ni * dr - nr * di) / dd;
				const double tr = re * qr - im * qi, ti = re * qi + im * qr;
				re = tr; im = ti;
			}
			pr = re; pi_ = im;
		};
		chain (0, ar0, ai0);
		chain (1, ar1, ai1);
		const double zr = std::cos (-w), zi = std::sin (-w);
		const double br = zr * ar1 - zi * ai1, bi = zr * ai1 + zi * ar1;
		return 0.5 * std::hypot (ar0 + br, ai0 + bi);
	}

	// plus petit nombre de coefficients >= minCoefs tel que l'atténuation hors bande dépasse attDb
	static int choose (double tbw, int minCoefs, double attDb) noexcept
	{
		double c[kMaxCoefs];
		const double ws = 2.0 * kPi * (0.25 + tbw * 0.5);
		for (int n = std::max (2, minCoefs); n <= kMaxCoefs; ++n)
		{
			coefs (n, tbw, c);
			if (-20.0 * std::log10 (std::max (1e-30, magnitude (c, n, ws))) >= attDb)
				return n;
		}
		return kMaxCoefs;
	}
};

//------------------------------------------------------------------------
// Un étage x2 IIR (par canal).
//------------------------------------------------------------------------
class IirHalfband
{
public:
	void configure (double tbw, int minCoefs, double attDb) noexcept
	{
		const int n = IirHalfbandDesign::choose (tbw, minCoefs, attDb);
		double c[IirHalfbandDesign::kMaxCoefs];
		IirHalfbandDesign::coefs (n, tbw, c);
		n0 = n1 = 0;
		for (int i = 0; i < n; ++i)
			(i % 2 == 0 ? a0[n0++] : a1[n1++]) = c[i];
		reset ();
	}
	void reset () noexcept
	{
		for (int i = 0; i < IirHalfbandDesign::kMaxCoefs; ++i)
			su0[i] = su1[i] = sd0[i] = sd1[i] = 0;
		odd = 0;
	}
	int order () const noexcept { return (n0 + n1) * 2 + 1; }

	// n échantillons -> 2n
	void up (const double* x, int n, double* y) noexcept
	{
		for (int i = 0; i < n; ++i)
		{
			y[2 * i] = chain (a0, n0, su0, x[i]);
			y[2 * i + 1] = chain (a1, n1, su1, x[i]);
		}
	}
	// 2n échantillons -> n
	void down (const double* v, int n, double* y) noexcept
	{
		for (int i = 0; i < n; ++i)
		{
			const double e = chain (a0, n0, sd0, v[2 * i]);
			const double o = chain (a1, n1, sd1, odd);
			odd = v[2 * i + 1];
			y[i] = 0.5 * (e + o);
		}
	}

private:
	static inline double chain (const double* a, int n, double* s, double x) noexcept
	{
		for (int i = 0; i < n; ++i)
		{
			const double y = a[i] * x + s[i];
			s[i] = x - a[i] * y;
			x = y;
		}
		return x;
	}
	double a0[IirHalfbandDesign::kMaxCoefs]{}, a1[IirHalfbandDesign::kMaxCoefs]{};
	double su0[IirHalfbandDesign::kMaxCoefs]{}, su1[IirHalfbandDesign::kMaxCoefs]{};
	double sd0[IirHalfbandDesign::kMaxCoefs]{}, sd1[IirHalfbandDesign::kMaxCoefs]{};
	double odd = 0;
	int n0 = 0, n1 = 0;
};

//------------------------------------------------------------------------
// Un étage x2 FIR demi-bande à phase linéaire, longueur 4m+1, fenêtre de Kaiser.
// Latence de l'étage (cascade up + down) = 2m-1 échantillons à sa fréquence basse.
//------------------------------------------------------------------------
class FirHalfband
{
public:
	void prepare (int halfLen, double kaiserBeta)
	{
		m = halfLen;
		c2.assign (static_cast<size_t> (2 * m), 0.0);
		auto bessel0 = [] (double x) {
			double s = 1, t = 1;
			for (int k = 1; k < 60; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; if (t < 1e-18 * s) break; }
			return s;
		};
		const double i0b = bessel0 (kaiserBeta), half = 2.0 * m;
		std::vector<double> g (static_cast<size_t> (m));
		for (int i = 0; i < m; ++i)
		{
			const double j = 2.0 * i + 1.0, r = j / half;
			const double w = bessel0 (kaiserBeta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
			g[static_cast<size_t> (i)] = ((i & 1) ? -1.0 : 1.0) * 2.0 / (kPi * j) * w;
		}
		for (int k = 0; k < m; ++k)
		{
			c2[static_cast<size_t> (k)] = g[static_cast<size_t> (m - 1 - k)];
			c2[static_cast<size_t> (m + k)] = g[static_cast<size_t> (k)];
		}
		const size_t cap = static_cast<size_t> (2 * m);
		upBuf.assign (cap * 2, 0.0);
		dnOdd.assign (cap * 2, 0.0);
		dnEven.assign (static_cast<size_t> (m), 0.0);
		reset ();
	}
	void reset () noexcept
	{
		std::fill (upBuf.begin (), upBuf.end (), 0.0);
		std::fill (dnOdd.begin (), dnOdd.end (), 0.0);
		std::fill (dnEven.begin (), dnEven.end (), 0.0);
		upPos = dnPos = dnEvenPos = 0;
	}
	int latencyLow () const noexcept { return 2 * m - 1; }

	void up (const double* x, int n, double* y) noexcept
	{
		const int cap = 2 * m;
		for (int i = 0; i < n; ++i)
		{
			upBuf[static_cast<size_t> (upPos)] = upBuf[static_cast<size_t> (upPos + cap)] = x[i];
			upPos = (upPos + 1 == cap) ? 0 : upPos + 1;
			const double* w = &upBuf[static_cast<size_t> (upPos)]; // plus ancien -> plus récent
			y[2 * i] = w[m - 1];
			y[2 * i + 1] = simd::dot (c2.data (), w, cap);
		}
	}
	void down (const double* v, int n, double* y) noexcept
	{
		const int cap = 2 * m;
		for (int i = 0; i < n; ++i)
		{
			dnOdd[static_cast<size_t> (dnPos)] = dnOdd[static_cast<size_t> (dnPos + cap)] = v[2 * i + 1];
			dnPos = (dnPos + 1 == cap) ? 0 : dnPos + 1;
			dnEven[static_cast<size_t> (dnEvenPos)] = v[2 * i];
			dnEvenPos = (dnEvenPos + 1 == m) ? 0 : dnEvenPos + 1;
			const double centre = dnEven[static_cast<size_t> (dnEvenPos)]; // e[p-(m-1)]
			y[i] = 0.5 * (centre + simd::dot (c2.data (), &dnOdd[static_cast<size_t> (dnPos)], cap));
		}
	}

private:
	int m = 0, upPos = 0, dnPos = 0, dnEvenPos = 0;
	std::vector<double> c2, upBuf, dnOdd, dnEven;
};

//------------------------------------------------------------------------
// Chaîne 8x d'un canal : 3 étages en cascade, versions IIR et FIR.
//------------------------------------------------------------------------
class Oversampler8x
{
public:
	static constexpr int kFactor = 8;
	static constexpr int kFirM[3] = {28, 8, 4}; // latence cumulée : 55 + 7,5 + 1,75 = 64,25 -> 64 échantillons
	static constexpr int kFirLatencySamples = 64;
	static constexpr int kMaxChunk = 256;

	void prepare (double fs)
	{
		const double fpass = std::min (20000.0, 0.4535 * fs);
		for (int s = 0; s < 3; ++s)
		{
			const double hi = fs * static_cast<double> (2 << s);
			const double tbw = 2.0 * (0.25 - fpass / hi);
			iir[s].configure (tbw, s == 0 ? 3 : 3, 90.0);
			fir[s].prepare (kFirM[s], 10.0);
		}
		tmp.assign (static_cast<size_t> (kMaxChunk) * 8, 0.0);
		tmp2.assign (static_cast<size_t> (kMaxChunk) * 8, 0.0);
		reset ();
	}
	void reset () noexcept
	{
		for (int s = 0; s < 3; ++s) { iir[s].reset (); fir[s].reset (); }
	}
	int iirOrderStage1 () const noexcept { return iir[0].order (); }

	// n <= kMaxChunk
	void upIir (const double* in, int n, double* out) noexcept { iir[0].up (in, n, tmp.data ()); iir[1].up (tmp.data (), 2 * n, tmp2.data ()); iir[2].up (tmp2.data (), 4 * n, out); }
	void upFir (const double* in, int n, double* out) noexcept { fir[0].up (in, n, tmp.data ()); fir[1].up (tmp.data (), 2 * n, tmp2.data ()); fir[2].up (tmp2.data (), 4 * n, out); }
	void downIir (const double* in, int n, double* out) noexcept { iir[2].down (in, 4 * n, tmp.data ()); iir[1].down (tmp.data (), 2 * n, tmp2.data ()); iir[0].down (tmp2.data (), n, out); }
	void downFir (const double* in, int n, double* out) noexcept { fir[2].down (in, 4 * n, tmp.data ()); fir[1].down (tmp.data (), 2 * n, tmp2.data ()); fir[0].down (tmp2.data (), n, out); }

private:
	IirHalfband iir[3];
	FirHalfband fir[3];
	std::vector<double> tmp, tmp2;
};
} // namespace tama
