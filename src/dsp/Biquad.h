// TAMA-VOX : biquad double précision (forme transposée II), conceptions RBJ
#pragma once
#include "../StdCommon.h"
#include "Common.h"

namespace tama
{
struct BiquadCoefs
{
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

	static BiquadCoefs lowpass (double fs, double f, double q) noexcept { return make (fs, f, q, 0, 0); }
	static BiquadCoefs highpass (double fs, double f, double q) noexcept { return make (fs, f, q, 1, 0); }
	static BiquadCoefs bandpass (double fs, double f, double q) noexcept { return make (fs, f, q, 2, 0); }
	static BiquadCoefs peaking (double fs, double f, double q, double gainDb) noexcept { return make (fs, f, q, 3, gainDb); }
	static BiquadCoefs lowShelf (double fs, double f, double gainDb) noexcept { return make (fs, f, 0.7071067811865476, 4, gainDb); }
	static BiquadCoefs highShelf (double fs, double f, double gainDb) noexcept { return make (fs, f, 0.7071067811865476, 5, gainDb); }

private:
	static BiquadCoefs make (double fs, double f, double q, int type, double gainDb) noexcept
	{
		f = clampd (f, 1.0, fs * 0.49);
		const double w0 = 2.0 * kPi * f / fs, cw = std::cos (w0), sw = std::sin (w0);
		const double alpha = sw / (2.0 * q);
		const double A = std::pow (10.0, gainDb / 40.0);
		double b0, b1, b2, a0, a1, a2;
		switch (type)
		{
			case 0: b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
			case 1: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
			case 2: b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
			case 3: b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
			case 4:
			{
				const double s = 2 * std::sqrt (A) * alpha;
				b0 = A * ((A + 1) - (A - 1) * cw + s); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s);
				a0 = (A + 1) + (A - 1) * cw + s; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s;
				break;
			}
			default:
			{
				const double s = 2 * std::sqrt (A) * alpha;
				b0 = A * ((A + 1) + (A - 1) * cw + s); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s);
				a0 = (A + 1) - (A - 1) * cw + s; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s;
				break;
			}
		}
		return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
	}
};

class Biquad
{
public:
	void set (const BiquadCoefs& c) noexcept { k = c; }
	void reset () noexcept { s1 = s2 = 0; }
	inline double process (double x) noexcept
	{
		const double y = k.b0 * x + s1;
		s1 = k.b1 * x - k.a1 * y + s2;
		s2 = k.b2 * x - k.a2 * y;
		return y;
	}
	const BiquadCoefs& coefs () const noexcept { return k; }

private:
	BiquadCoefs k;
	double s1 = 0, s2 = 0;
};
} // namespace tama
