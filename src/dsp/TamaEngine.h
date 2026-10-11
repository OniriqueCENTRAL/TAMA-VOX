// TAMA-VOX : moteur complet, indépendant de tout SDK (testable hors plugin).
//
//  x[n] -> sur-échantillonneur 8x (IIR SNAP / FIR PURE, toujours chauds, fondu de 64 échantillons)
//       -> transformateur WDF -> séparateur transitoires/stationnaire
//            transitoires  -> clipper ADAA ordre 2 --------------------------+
//            stationnaire  -> compresseur opto T4 (détecteur lié, gain en dB) +-> sommation (phase lock)
//       -> saturation de corps ADAA -> blocage DC -> couleur du modèle (shelves + tanh ADAA)
//       -> air shelf intelligent + de-esser
//       -> décimation 8x -> auto-gain LUFS BS.1770 -> limiteur de sécurité -> sortie
//
// Règles temps réel : toute la mémoire est allouée dans prepare() ; process() n'alloue, ne verrouille
// et n'appelle aucun service système.
#pragma once
#include "../StdCommon.h"
#include "AdaaSaturator.h"
#include "AirDeesser.h"
#include "Biquad.h"
#include "Loudness.h"
#include "Models.h"
#include "OptoDetector.h"
#include "Oversampling.h"
#include "TransientSplit.h"
#include "WdfTransformer.h"
#include <vector>

namespace tama
{
struct EngineParams
{
	double drive = 5.0;     // 1..11
	int flavor = 0;         // 0 pink, 1 blue, 2 black
	double squeeze = 45.0;  // 0..100
	int mode = 0;           // 0 SNAP, 1 PURE
	bool autoGain = true;
	bool raw = false;       // écoute RAW (comparaison A/B)
	bool bypass = false;    // bypass de l'hôte
	int model = 0;          // index dans kModels
	double thrOff = 0, ratioOff = 0, kneeOff = 0;
	double attackMs = 3, adaptPct = 100, relMinMs = 45, relMaxMs = 800, memoryMs = 350;
	double colorK = 0, colorB = 0, loDb = 0, midDb = 0, hiDb = 0;
};

struct EngineMeters
{
	double lamp = 0, gr = 0, duck = 0, tauMs = 0, lufsIn = -120, lufsOut = -120, makeupDb = 0, peakDb = -120, squash = 0, glitch = 0, sat = 0;
};

class TamaEngine
{
public:
	static constexpr int kMaxCh = 2;
	static constexpr int kChunk = 64;
	static constexpr int kOs = 8;
	static constexpr int kPureLatency = Oversampler8x::kFirLatencySamples;

	void prepare (double sampleRate, int channels)
	{
		fs = sampleRate;
		fs8 = fs * kOs;
		nch = std::min (std::max (channels, 1), kMaxCh);
		for (int c = 0; c < kMaxCh; ++c)
		{
			os[c].prepare (fs);
			wdf[c].prepare (fs8);
			split[c].prepare (fs8);
			air[c].prepare (fs8);
			dc[c].set (BiquadCoefs::highpass (fs8, 16.0, 0.7071));
			const size_t n8 = static_cast<size_t> (kChunk) * kOs;
			for (auto* v : {&dry[c], &dnI[c], &dnF[c], &post[c]}) v->assign (kChunk, 0.0);
			for (auto* v : {&upI[c], &upF[c], &tProc[c], &sBuf[c], &y8[c]}) v->assign (n8, 0.0);
			dryRing[c].assign (kRing, 0.0);
		}
		aSmooth = onePole (0.02, fs);
		aLimRel = onePole (0.06, fs);
		aRaw = onePole (0.01, fs);
		detector.prepare (fs);
		meterIn.prepare (fs, nch);
		meterOut.prepare (fs, nch);
		autoGain.prepare (fs);
		applyParams (P, true);
		reset ();
	}

	void reset () noexcept
	{
		for (int c = 0; c < kMaxCh; ++c)
		{
			os[c].reset (); wdf[c].reset (); split[c].reset (); air[c].reset (); dc[c].reset ();
			satT[c].reset (); satB[c].reset (); colTanh[c].reset (); lo[c].reset (); mid[c].reset (); hi[c].reset ();
			std::fill (dryRing[c].begin (), dryRing[c].end (), 0.0);
		}
		detector.reset (); meterIn.reset (); meterOut.reset (); autoGain.reset ();
		ringPos = 0; gPrev = 1.0; gLim = 1.0; wRaw = 0; modeFrac = P.mode ? 1.0 : 0.0; inGainS = inGainT; preS = preT; makeS = 1.0;
		M = EngineMeters{};
	}

	void setParams (const EngineParams& p) noexcept { applyParams (p, false); }
	int latencySamples () const noexcept { return P.mode ? kPureLatency : 0; }
	const EngineMeters& meters () const noexcept { return M; }

	template <class T>
	void process (const T* const* in, T* const* out, int n, int channels) noexcept
	{
		ScopedNoDenormals nd;
		const int nc = std::min (channels, nch);
		int pos = 0;
		while (pos < n)
		{
			const int m = std::min (kChunk, n - pos);
			for (int c = 0; c < nc; ++c)
				for (int i = 0; i < m; ++i)
					dry[c][static_cast<size_t> (i)] = static_cast<double> (in[c][pos + i]);
			// canal 2 absent : on duplique pour garder un chemin stéréo cohérent
			if (nc < nch)
				for (int c = nc; c < nch; ++c)
					for (int i = 0; i < m; ++i) dry[c][static_cast<size_t> (i)] = dry[0][static_cast<size_t> (i)];
			processChunk (m);
			for (int c = 0; c < nc; ++c)
				for (int i = 0; i < m; ++i)
					out[c][pos + i] = static_cast<T> (post[c][static_cast<size_t> (i)]);
			pos += m;
		}
	}

private:
	static constexpr int kRing = 256, kRingMask = kRing - 1;

	void applyParams (const EngineParams& p, bool force) noexcept
	{
		const bool flavChanged = force || p.flavor != P.flavor;
		P = p;
		flavBody = P.flavor == 2 ? 1.4 : (P.flavor == 1 ? 1.15 : 1.0); // égalise la présence des trois saveurs
		const CompModel& md = kModels[static_cast<size_t> (std::clamp (P.model, 0, static_cast<int> (kModels.size ()) - 1))];
		const double q = P.squeeze;
		CompParams cp;
		cp.thrDb = clampd (-14.0 - q * 0.26 + P.thrOff, -80.0, 0.0);
		cp.ratio = clampd (2.0 + q * 0.1 + P.ratioOff, 1.0, 20.0);
		cp.kneeDb = clampd (20.0 - q * 0.12 + P.kneeOff, 0.0, 40.0);
		cp.attackMs = P.attackMs;
		cp.adapt = clampd (P.adaptPct / 100.0, 0.0, 1.0);
		cp.relMinMs = P.relMinMs; cp.relMaxMs = P.relMaxMs; cp.memoryMs = P.memoryMs;
		cp.liftDb = md.lift * (0.4 + 0.012 * q);
		detector.set (cp);
		if (flavChanged)
			for (int c = 0; c < kMaxCh; ++c) { satT[c].setFlavor (static_cast<Flavor> (std::clamp (P.flavor, 0, 2)), 0.0); satB[c].setFlavor (static_cast<Flavor> (std::clamp (P.flavor, 0, 2)), 0.0); }
		// couleur analogique
		for (int c = 0; c < kMaxCh; ++c)
		{
			lo[c].set (BiquadCoefs::lowShelf (fs8, 110.0, P.loDb));
			mid[c].set (BiquadCoefs::peaking (fs8, md.mf, 0.8, P.midDb));
			hi[c].set (BiquadCoefs::highShelf (fs8, 7500.0, P.hiDb));
			colTanh[c].set (P.colorK, P.colorB);
		}
		// gains de drive : 0.5 -> 4 sur 1..11 (18 dB)
		const double d = clampd ((P.drive - 1.0) / 10.0, 0.0, 1.0);
		preT = 0.5 * dbToLin (18.0 * d);
		inGainT = 0.7 * dbToLin (16.0 * d);
		for (int c = 0; c < kMaxCh; ++c) air[c].setAmounts (5.0, 0.22);
	}

	void processChunk (int m) noexcept
	{
		const size_t M8 = static_cast<size_t> (m) * kOs;
		// --- entrée : sonie, anneau de retard du dry ---
		for (int i = 0; i < m; ++i)
		{
			for (int c = 0; c < nch; ++c) { meterIn.push (c, dry[c][static_cast<size_t> (i)]); dryRing[c][static_cast<size_t> ((ringPos + i) & kRingMask)] = dry[c][static_cast<size_t> (i)]; }
			meterIn.tick ();
		}
		// --- suréchantillonnage (les deux chaînes restent chaudes) ---
		for (int c = 0; c < nch; ++c)
		{
			os[c].upIir (dry[c].data (), m, upI[c].data ());
			os[c].upFir (dry[c].data (), m, upF[c].data ());
		}
		const double tgtFrac = P.mode ? 1.0 : 0.0, stepFrac = 1.0 / 64.0;
		double wF[kChunk]; // poids PURE par échantillon de base
		for (int i = 0; i < m; ++i)
		{
			modeFrac += clampd (tgtFrac - modeFrac, -stepFrac, stepFrac);
			wF[i] = modeFrac;
		}
		double satPeak = 0;
		// --- passe A : mélange, transformateur, séparation, clipper ADAA sur les transitoires ---
		for (int c = 0; c < nch; ++c)
		{
			double* ti = tProc[c].data (); double* sb = sBuf[c].data ();
			const double* ui = upI[c].data (); const double* uf = upF[c].data ();
			double ig = inGainS, pr = preS;
			for (int i = 0; i < m; ++i)
			{
				ig += (inGainT - ig) * aSmooth; pr += (preT - pr) * aSmooth;
				const double w = wF[i];
				for (int j = 0; j < kOs; ++j)
				{
					const size_t k = static_cast<size_t> (i) * kOs + static_cast<size_t> (j);
					const double u = ui[k] + (uf[k] - ui[k]) * w;
					const double v = wdf[c].process (u, ig);
					const double t = split[c].process (v);
					const double s = v - t;
					const double xt = t * pr;
					satPeak = std::max (satPeak, std::fabs (xt));
					ti[k] = satT[c].process (xt) / pr;
					sb[k] = s;
				}
			}
		}
		// --- passe B : détecteur opto lié (énergie stationnaire moyennée sur les canaux) ---
		double gTarget[kChunk];
		for (int i = 0; i < m; ++i)
		{
			double e = 0;
			for (int c = 0; c < nch; ++c)
				for (int j = 0; j < kOs; ++j) { const double s = sBuf[c][static_cast<size_t> (i) * kOs + static_cast<size_t> (j)]; e += s * s; }
			e /= static_cast<double> (nch * kOs);
			gTarget[i] = dbToLin (detector.process (e));
		}
		const double g0 = gPrev;
		// --- passe C : application du gain, sommation, saturation de corps, couleur, air ---
		for (int c = 0; c < nch; ++c)
		{
			double g = g0;
			const double* ti = tProc[c].data (); const double* sb = sBuf[c].data (); double* yy = y8[c].data ();
			double pr = preS;
			for (int i = 0; i < m; ++i)
			{
				pr += (preT - pr) * aSmooth;
				const double preB = flavBody * 2.0 * std::pow (pr * 2.0, 1.3); // saturation de corps : +6 dB (drive 1) à +29 dB (drive 11)
				const double gn = gTarget[i], step = (gn - g) / kOs;
				for (int j = 0; j < kOs; ++j)
				{
					const size_t k = static_cast<size_t> (i) * kOs + static_cast<size_t> (j);
					g += step;
					double y = ti[k] + sb[k] * g;                   // sommation phase lock
					y = satB[c].process (y * preB) / preB;          // saturation de corps (même saveur, drive réduit)
					y = dc[c].process (y);
					y = colTanh[c].process (hi[c].process (mid[c].process (lo[c].process (y))));
					yy[k] = air[c].process (y);
				}
			}
		}
		gPrev = gTarget[m - 1];
		inGainS += (inGainT - inGainS) * (1.0 - std::pow (1.0 - aSmooth, m));
		preS += (preT - preS) * (1.0 - std::pow (1.0 - aSmooth, m));
		// --- décimation ---
		for (int c = 0; c < nch; ++c)
		{
			os[c].downIir (y8[c].data (), m, dnI[c].data ());
			os[c].downFir (y8[c].data (), m, dnF[c].data ());
		}
		(void) M8;
		// --- sortie : auto-gain, limiteur, RAW/bypass ---
		double peakOut = 0;
		const double ceil = 0.8913; // -1 dBFS
		for (int i = 0; i < m; ++i)
		{
			const double w = wF[i];
			double y[kMaxCh], d[kMaxCh];
			for (int c = 0; c < nch; ++c)
			{
				y[c] = dnI[c][static_cast<size_t> (i)] + (dnF[c][static_cast<size_t> (i)] - dnI[c][static_cast<size_t> (i)]) * w;
				meterOut.push (c, y[c]);
				// dry aligné sur la latence en vigueur (fondu entre 0 et 64 échantillons)
				const double dI = dryRing[c][static_cast<size_t> ((ringPos + i) & kRingMask)];
				const double dF = dryRing[c][static_cast<size_t> ((ringPos + i - kPureLatency) & kRingMask)];
				d[c] = dI + (dF - dI) * w;
			}
			meterOut.tick ();
			const double gDb = autoGain.process (meterIn.meanSquare (), meterOut.meanSquare ());
			const double mk = P.autoGain ? dbToLin (gDb) : 1.0;
			makeS += (mk - makeS) * aSmooth;
			// limiteur de sécurité à gain lié
			double pk = 0;
			for (int c = 0; c < nch; ++c) { y[c] *= makeS; pk = std::max (pk, std::fabs (y[c])); }
			const double need = pk > ceil ? ceil / pk : 1.0;
			if (need < gLim) gLim = need; else gLim += (1.0 - gLim) * aLimRel;
			wRaw += ((P.raw || P.bypass) ? 1.0 - wRaw : -wRaw) * aRaw;
			for (int c = 0; c < nch; ++c)
			{
				double o = y[c] * gLim;
				o = std::clamp (o, -1.0, 1.0);
				const double outv = o + (d[c] - o) * wRaw;
				post[c][static_cast<size_t> (i)] = outv;
				peakOut = std::max (peakOut, std::fabs (outv));
			}
		}
		ringPos = (ringPos + m) & kRingMask;
		// --- mesures pour l'interface ---
		const double lampT = clampd ((satPeak - 0.5) / 2.4, 0.0, 1.0);
		double sat = 0; for (int c = 0; c < nch; ++c) sat = std::max (sat, wdf[c].saturation ());
		M.sat = sat;
		const double lampTgt = std::max (lampT, clampd ((sat - 0.35) / 0.55, 0.0, 1.0));
		M.lamp += (lampTgt - M.lamp) * (lampTgt > M.lamp ? 0.5 : 0.08);
		M.gr = detector.reductionDb ();
		M.tauMs = detector.tauSeconds () * 1000.0;
		M.lufsIn = meterIn.lufs (); M.lufsOut = meterOut.lufs ();
		M.makeupDb = P.autoGain ? autoGain.gainDb () : 0.0;
		M.peakDb = linToDb (peakOut);
		M.squash = clampd (M.gr / 14.0, 0.0, 1.0);
		double duck = 0, act = 0;
		for (int c = 0; c < nch; ++c) { duck = std::max (duck, air[c].airDuckDb ()); act = std::max (act, split[c].transientActivity ()); }
		M.duck = duck; M.glitch = act;
	}

	double fs = 48000, fs8 = 384000;
	int nch = 2;
	EngineParams P;
	Oversampler8x os[kMaxCh];
	WdfTransformer wdf[kMaxCh];
	TransientSplit split[kMaxCh];
	AdaaSaturator satT[kMaxCh], satB[kMaxCh];
	Biquad dc[kMaxCh], lo[kMaxCh], mid[kMaxCh], hi[kMaxCh];
	ColorTanh colTanh[kMaxCh];
	AirDeesser air[kMaxCh];
	OptoDetector detector;
	LoudnessMeter meterIn, meterOut;
	AutoGain autoGain;
	std::vector<double> dry[kMaxCh], dnI[kMaxCh], dnF[kMaxCh], post[kMaxCh], upI[kMaxCh], upF[kMaxCh], tProc[kMaxCh], sBuf[kMaxCh], y8[kMaxCh], dryRing[kMaxCh];
	double flavBody = 1.0;
	double aSmooth = 0.001, aLimRel = 0.0003, aRaw = 0.002;
	double inGainT = 1.0, inGainS = 1.0, preT = 1.0, preS = 1.0, gPrev = 1.0, gLim = 1.0, wRaw = 0, modeFrac = 0, makeS = 1.0;
	int ringPos = 0;
	EngineMeters M;
};
} // namespace tama
