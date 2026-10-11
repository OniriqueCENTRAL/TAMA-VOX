// TAMA-VOX : utilitaires DSP communs (aucune allocation, aucun verrou)
#pragma once
#include "../StdCommon.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#if defined(_MSC_VER)
#include <intrin.h>
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <immintrin.h>
#define TAMA_X86 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define TAMA_NEON 1
#endif

namespace tama
{
inline constexpr double kPi = std::numbers::pi;

inline double dbToLin (double db) noexcept { return std::pow (10.0, db * 0.05); }
inline double linToDb (double lin) noexcept { return 20.0 * std::log10 (std::max (lin, 1e-12)); }
inline double clampd (double v, double lo, double hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }
inline double smoothstep (double a, double b, double x) noexcept
{
	const double t = clampd ((x - a) / (b - a), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}
// coefficient d'un lisseur à un pôle pour une constante de temps en secondes
inline double onePole (double seconds, double fs) noexcept
{
	return seconds <= 0.0 ? 1.0 : 1.0 - std::exp (-1.0 / (seconds * fs));
}

// Désactive les dénormaux sur le thread audio (FTZ/DAZ), restaure à la sortie.
class ScopedNoDenormals
{
public:
	ScopedNoDenormals () noexcept
	{
#if defined(TAMA_X86)
		saved = _mm_getcsr ();
		_mm_setcsr (saved | 0x8040u);
#elif defined(TAMA_NEON) && (defined(__GNUC__) || defined(__clang__))
		uint64_t fpcr;
		asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
		saved = fpcr;
		asm volatile ("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
	}
	~ScopedNoDenormals () noexcept
	{
#if defined(TAMA_X86)
		_mm_setcsr (static_cast<unsigned> (saved));
#elif defined(TAMA_NEON) && (defined(__GNUC__) || defined(__clang__))
		asm volatile ("msr fpcr, %0" : : "r"(saved));
#endif
	}

private:
	uint64_t saved = 0;
};
} // namespace tama
