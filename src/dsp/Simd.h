// TAMA-VOX : noyau SIMD explicite (AVX2+FMA avec dispatch à l'exécution, NEON, SSE2, repli scalaire)
#pragma once
#include "../StdCommon.h"
#include "Common.h"

namespace tama::simd
{
#if defined(TAMA_X86)
inline bool cpuHasAvx2Fma () noexcept
{
	static const bool has = [] {
#if defined(_MSC_VER)
		int r[4];
		__cpuid (r, 0);
		if (r[0] < 7)
			return false;
		__cpuid (r, 1);
		const bool osxsave = (r[2] & (1 << 27)) != 0, fma = (r[2] & (1 << 12)) != 0;
		if (!osxsave || !fma)
			return false;
		if ((_xgetbv (0) & 6) != 6)
			return false;
		__cpuidex (r, 7, 0);
		return (r[1] & (1 << 5)) != 0;
#else
		__builtin_cpu_init ();
		return __builtin_cpu_supports ("avx2") && __builtin_cpu_supports ("fma");
#endif
	}();
	return has;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__ ((target ("avx2,fma")))
#endif
inline double dotAvx2 (const double* a, const double* b, int n) noexcept
{
	__m256d acc0 = _mm256_setzero_pd (), acc1 = _mm256_setzero_pd ();
	int i = 0;
	for (; i + 8 <= n; i += 8)
	{
		acc0 = _mm256_fmadd_pd (_mm256_loadu_pd (a + i), _mm256_loadu_pd (b + i), acc0);
		acc1 = _mm256_fmadd_pd (_mm256_loadu_pd (a + i + 4), _mm256_loadu_pd (b + i + 4), acc1);
	}
	for (; i + 4 <= n; i += 4)
		acc0 = _mm256_fmadd_pd (_mm256_loadu_pd (a + i), _mm256_loadu_pd (b + i), acc0);
	acc0 = _mm256_add_pd (acc0, acc1);
	__m128d lo = _mm256_castpd256_pd128 (acc0), hi = _mm256_extractf128_pd (acc0, 1);
	lo = _mm_add_pd (lo, hi);
	lo = _mm_add_sd (lo, _mm_unpackhi_pd (lo, lo));
	double r = _mm_cvtsd_f64 (lo);
	for (; i < n; ++i)
		r += a[i] * b[i];
	return r;
}

inline double dotSse2 (const double* a, const double* b, int n) noexcept
{
	__m128d acc0 = _mm_setzero_pd (), acc1 = _mm_setzero_pd ();
	int i = 0;
	for (; i + 4 <= n; i += 4)
	{
		acc0 = _mm_add_pd (acc0, _mm_mul_pd (_mm_loadu_pd (a + i), _mm_loadu_pd (b + i)));
		acc1 = _mm_add_pd (acc1, _mm_mul_pd (_mm_loadu_pd (a + i + 2), _mm_loadu_pd (b + i + 2)));
	}
	acc0 = _mm_add_pd (acc0, acc1);
	acc0 = _mm_add_sd (acc0, _mm_unpackhi_pd (acc0, acc0));
	double r = _mm_cvtsd_f64 (acc0);
	for (; i < n; ++i)
		r += a[i] * b[i];
	return r;
}
#endif

// produit scalaire a·b sur n éléments
inline double dot (const double* a, const double* b, int n) noexcept
{
#if defined(TAMA_X86)
	if (cpuHasAvx2Fma ())
		return dotAvx2 (a, b, n);
	return dotSse2 (a, b, n);
#elif defined(TAMA_NEON)
	float64x2_t acc0 = vdupq_n_f64 (0.0), acc1 = vdupq_n_f64 (0.0);
	int i = 0;
	for (; i + 4 <= n; i += 4)
	{
		acc0 = vfmaq_f64 (acc0, vld1q_f64 (a + i), vld1q_f64 (b + i));
		acc1 = vfmaq_f64 (acc1, vld1q_f64 (a + i + 2), vld1q_f64 (b + i + 2));
	}
	double r = vaddvq_f64 (vaddq_f64 (acc0, acc1));
	for (; i < n; ++i)
		r += a[i] * b[i];
	return r;
#else
	double r = 0;
	for (int i = 0; i < n; ++i)
		r += a[i] * b[i];
	return r;
#endif
}

// y[i] += g * x[i]
inline void addScaled (double* y, const double* x, double g, int n) noexcept
{
	for (int i = 0; i < n; ++i)
		y[i] += g * x[i];
}
} // namespace tama::simd
