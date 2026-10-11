// TAMA-VOX : saturation ADAA (antiderivative antialiasing) d'ordre 2.
//
// Pour une non-linéarité f avec première primitive F1 et seconde primitive F2 (formes closes) :
//     D[n] = (F2(x[n]) - F2(x[n-1])) / (x[n] - x[n-1])
//     y[n] = 2 (D[n] - D[n-1]) / (x[n] - x[n-2])
// C'est la forme à pas non uniformes (Parker/Esqueda/Bilbao). Elle se réduit à
// (F2[n] - 2 F2[n-1] + F2[n-2]) / dx^2 du cahier des charges quand l'échantillonnage de x est régulier.
// Les cas mal conditionnés (x[n] ~ x[n-1], x[n] ~ x[n-2]) sont traités par développement de Taylor
// (évaluation au milieu), ce qui évite toute division par zéro.
//
// Trois saveurs, toutes avec primitives analytiques :
//   PINK  : écrêtage doux cubique décalé (biais -> harmonique 2, triode)
//   BLUE  : écrêtage quintique lissé (symétrique -> harmonique 3, FET)
//   BLACK : repli sinusoïdal (wavefolding dur)
#pragma once
#include "../StdCommon.h"
#include "Common.h"

namespace tama
{
enum class Flavor : int { Pink = 0, Blue = 1, Black = 2 };

namespace adaa
{
// ---- PINK ----
struct Pink
{
	static constexpr double bias = 0.22;
	// cubique : g(u) = u - u^3/3 sur [-1,1], +-2/3 au-delà
	static double g (double u) noexcept { const double a = std::fabs (u); return a <= 1 ? u - u * u * u / 3 : (u > 0 ? 2.0 / 3 : -2.0 / 3); }
	static double G1 (double u) noexcept
	{
		const double a = std::fabs (u);
		return a <= 1 ? u * u / 2 - u * u * u * u / 12 : 5.0 / 12 + (2.0 / 3) * (a - 1);
	}
	static double G2 (double u) noexcept
	{
		const double a = std::fabs (u);
		double r;
		if (a <= 1) r = a * a * a / 6 - a * a * a * a * a / 60;
		else { const double d = a - 1; r = 0.15 + (5.0 / 12) * d + (2.0 / 3) * d * d / 2; }
		return u < 0 ? -r : r;
	}
	static double norm () noexcept { return 1.0 / (1.0 - bias * bias); }
	static double f (double x) noexcept { return (g (x + bias) - g (bias)) * norm (); }
	static double F1 (double x) noexcept { return (G1 (x + bias) - G1 (bias) - g (bias) * x) * norm (); }
	static double F2 (double x) noexcept { return (G2 (x + bias) - G2 (bias) - G1 (bias) * x - g (bias) * x * x / 2) * norm (); }
};
// ---- BLUE ----
struct Blue
{
	// s(u) = (15u - 10u^3 + 3u^5)/8 sur [-1,1] ; pente 15/8 en 0
	static double s (double u) noexcept { const double a = std::fabs (u); return a <= 1 ? (15 * u - 10 * u * u * u + 3 * u * u * u * u * u) / 8 : (u > 0 ? 1.0 : -1.0); }
	static double S1 (double u) noexcept
	{
		const double a = std::fabs (u), a2 = a * a;
		return a <= 1 ? (7.5 * a2 - 2.5 * a2 * a2 + 0.5 * a2 * a2 * a2) / 8 : 0.6875 + (a - 1);
	}
	static double S2 (double u) noexcept
	{
		const double a = std::fabs (u);
		double r;
		if (a <= 1) { const double a3 = a * a * a; r = (2.5 * a3 - 0.5 * a3 * a * a + (0.5 / 7) * a3 * a3 * a) / 8; }
		else { const double d = a - 1; r = 0.2589285714285714 + 0.6875 * d + d * d / 2; }
		return u < 0 ? -r : r;
	}
	static double norm () noexcept { return 8.0 / 15.0; }
	static double f (double x) noexcept { return s (x) * norm (); }
	static double F1 (double x) noexcept { return S1 (x) * norm (); }
	static double F2 (double x) noexcept { return S2 (x) * norm (); }
};
// ---- BLACK ----
struct Black
{
	static constexpr double a = 1.4;
	static double f (double x) noexcept { return std::sin (a * x) / a; }
	static double F1 (double x) noexcept { return (1.0 - std::cos (a * x)) / (a * a); }
	static double F2 (double x) noexcept { return (a * x - std::sin (a * x)) / (a * a * a); }
};
} // namespace adaa

// État ADAA d'ordre 2 pour un canal et une non-linéarité NL.
template <class NL>
struct Adaa2State
{
	double x1 = 0, x2 = 0, d1 = 0;
	void warm (double x) noexcept { x1 = x2 = x; d1 = NL::F1 (x); }
	inline double process (double x) noexcept
	{
		constexpr double eps = 1e-4;
		const double dx = x - x1;
		const double dn = std::fabs (dx) < eps ? NL::F1 (0.5 * (x + x1)) : (NL::F2 (x) - NL::F2 (x1)) / dx;
		const double dd = x - x2;
		double y;
		if (std::fabs (dd) < eps)
			y = NL::f (0.25 * (x + 2.0 * x1 + x2));
		else
			y = 2.0 * (dn - d1) / dd;
		x2 = x1; x1 = x; d1 = dn;
		return y;
	}
};

// Saturateur multi-saveurs avec fondu enchaîné lors du changement de saveur (anti-clic).
class AdaaSaturator
{
public:
	void reset () noexcept { pink = {}; blue = {}; black = {}; active = prev = Flavor::Pink; fade = 0; }
	void setFlavor (Flavor f, double lastInput) noexcept
	{
		if (f == active)
			return;
		prev = active;
		active = f;
		warm (active, lastInput);
		fade = kFadeLen;
	}
	Flavor flavor () const noexcept { return active; }
	inline double process (double x) noexcept
	{
		const double y = run (active, x);
		if (fade > 0)
		{
			const double w = static_cast<double> (fade) / kFadeLen; // 1 -> 0
			const double yo = run (prev, x);
			--fade;
			return y + (yo - y) * w;
		}
		return y;
	}

private:
	static constexpr int kFadeLen = 2048; // à 8x : 256 échantillons à Fs
	void warm (Flavor f, double x) noexcept
	{
		switch (f) { case Flavor::Pink: pink.warm (x); break; case Flavor::Blue: blue.warm (x); break; default: black.warm (x); }
	}
	inline double run (Flavor f, double x) noexcept
	{
		switch (f) { case Flavor::Pink: return pink.process (x); case Flavor::Blue: return blue.process (x); default: return black.process (x); }
	}
	Adaa2State<adaa::Pink> pink;
	Adaa2State<adaa::Blue> blue;
	Adaa2State<adaa::Black> black;
	Flavor active = Flavor::Pink, prev = Flavor::Pink;
	int fade = 0;
};

// ADAA d'ordre 1 de tanh avec biais (étage de couleur analogique des modèles de compresseur).
class ColorTanh
{
public:
	void set (double k_, double b_) noexcept
	{
		k = k_; b = b_;
		const double t = std::tanh (k * b);
		off = t;
		inv = k < 0.01 ? 1.0 : 1.0 / (k * (1.0 - t * t));
	}
	void reset () noexcept { x1 = 0; }
	inline double process (double x) noexcept
	{
		if (k < 0.01)
			return x;
		const double dx = x - x1;
		double y;
		if (std::fabs (dx) < 1e-6)
			y = std::tanh (k * (0.5 * (x + x1) + b));
		else
			y = (lcosh (k * (x + b)) - lcosh (k * (x1 + b))) / (k * dx);
		x1 = x;
		return (y - off) * inv;
	}

private:
	static double lcosh (double u) noexcept { const double a = std::fabs (u); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }
	double k = 0, b = 0, off = 0, inv = 1, x1 = 0;
};
} // namespace tama
