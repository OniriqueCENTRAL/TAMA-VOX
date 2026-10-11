// Tests automatiques (CTest) : paramètres, stabilité numérique, temps réel (zéro allocation), latence, blocs.
#include "../src/TamaParams.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

using namespace tama;

// --- compteur d'allocations (pour prouver que process() n'alloue pas) ---
static std::atomic<long> gAllocs {0};
static std::atomic<bool> gCount {false};
void* operator new (std::size_t n)
{
	if (gCount.load (std::memory_order_relaxed)) gAllocs.fetch_add (1, std::memory_order_relaxed);
	if (void* p = std::malloc (n ? n : 1)) return p;
	throw std::bad_alloc ();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

static int gFail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++gFail; std::printf ("  ECHEC %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void testParams ()
{
	std::printf ("[params]\n");
	for (uint32_t i = 0; i < id::kNumParams; ++i)
	{
		const ParamSpec& s = spec (i);
		CHECK (s.id == i, "ordre de la table %u", i);
		for (double n : {0.0, 0.13, 0.5, 0.77, 1.0})
		{
			const double p = toPlain (s, n);
			CHECK (p >= s.min - 1e-9 && p <= s.max + 1e-9, "%s plain hors plage %g", s.name, p);
			const double n2 = toNorm (s, p);
			const double p2 = toPlain (s, n2);
			CHECK (std::fabs (p2 - p) < 1e-9 * (1 + std::fabs (p)), "%s aller-retour %g -> %g", s.name, p, p2);
		}
		char b[96];
		formatPlain (i, s.def, b, sizeof (b));
		double back = 0;
		CHECK (parsePlain (i, b, back), "%s: texte '%s' illisible", s.name, b);
		CHECK (std::fabs (toNorm (s, s.def) - defaultNorm (i)) < 1e-12, "défaut");
	}
	for (int m = 0; m < static_cast<int> (kModels.size ()); ++m)
		for (uint32_t pid : kModelDrivenParams)
		{
			const double v = modelDefault (m, pid), n = toNorm (spec (pid), v);
			CHECK (std::fabs (toPlain (spec (pid), n) - v) <= 0.02 * (spec (pid).max - spec (pid).min), "modèle %d param %u valeur %g hors plage", m, pid, v);
		}
	for (int i = 0; i < kNumMeters; ++i)
	{
		const uint32_t mid = id::kMeterBase + i;
		CHECK (std::fabs (meterFromNorm (mid, meterToNorm (mid, 0.5 * (kMeterSpecs[i].lo + kMeterSpecs[i].hi))) - 0.5 * (kMeterSpecs[i].lo + kMeterSpecs[i].hi)) < 1e-9, "compteur %d", i);
	}
}

static std::vector<double> makeVoiceLike (double fs, double seconds, unsigned seed)
{
	std::mt19937 rng (seed);
	std::normal_distribution<double> nd (0.0, 1.0);
	const size_t n = static_cast<size_t> (fs * seconds);
	std::vector<double> x (n);
	for (size_t i = 0; i < n; ++i)
	{
		const double t = i / fs, env = 0.5 + 0.5 * std::sin (2 * kPi * 2.3 * t);
		double v = 0;
		for (int h = 1; h <= 12; ++h) v += std::sin (2 * kPi * 180.0 * h * t) / h;
		x[i] = 0.25 * env * v + 0.02 * nd (rng) + ((i % 24000) < 40 ? 0.6 : 0.0) * nd (rng); // voyelle + souffle + « plosives »
	}
	return x;
}

template <class T>
static void runBlocks (TamaEngine& e, const std::vector<double>& x, std::vector<std::vector<T>>& out, const std::vector<int>& sizes, bool countAllocs = false)
{
	const size_t n = x.size ();
	std::vector<T> l (n), r (n), il (n), ir (n);
	for (size_t i = 0; i < n; ++i) { il[i] = static_cast<T> (x[i]); ir[i] = static_cast<T> (x[i] * 0.8); }
	size_t pos = 0, k = 0;
	while (pos < n)
	{
		const int m = static_cast<int> (std::min<size_t> (static_cast<size_t> (sizes[k++ % sizes.size ()]), n - pos));
		const T* in[2] = {il.data () + pos, ir.data () + pos};
		T* o[2] = {l.data () + pos, r.data () + pos};
		gCount = countAllocs;
		e.process<T> (in, o, m, 2);
		gCount = false;
		pos += m;
	}
	out = {l, r};
}

static void testStabilityAndRealtime ()
{
	std::printf ("[stabilité / temps réel]\n");
	const double rates[] = {44100, 48000, 96000};
	for (double fs : rates)
		for (int mode = 0; mode < 2; ++mode)
			for (int flavor = 0; flavor < 3; ++flavor)
			{
				TamaEngine e;
				e.prepare (fs, 2);
				ParamSet ps;
				ps.norm[id::kMode] = mode; ps.norm[id::kFlavor] = flavor / 2.0; ps.norm[id::kDrive] = 1.0; ps.norm[id::kSqueeze] = 1.0;
				ps.norm[id::kModel] = (flavor * 4 % 10) / 9.0;
				e.setParams (makeEngineParams (ps));
				const auto x = makeVoiceLike (fs, 2.5, 7 + flavor);
				std::vector<std::vector<float>> o;
				gAllocs = 0;
				runBlocks<float> (e, x, o, {1, 7, 64, 333, 512, 1000, 31}, true);
				double mx = 0; bool finite = true;
				for (auto& ch : o) for (float v : ch) { if (!std::isfinite (v)) finite = false; mx = std::max<double> (mx, std::fabs (v)); }
				CHECK (finite, "NaN/Inf fs=%g mode=%d flavor=%d", fs, mode, flavor);
				CHECK (mx <= 1.0 + 1e-6, "dépassement %g", mx);
				CHECK (gAllocs.load () == 0, "%ld allocations dans process()", gAllocs.load ());
				CHECK (mx > 0.01, "sortie muette fs=%g", fs);
			}
	// paramètres changés en cours de route : aucune allocation, aucune explosion
	{
		TamaEngine e; e.prepare (48000, 2);
		const auto x = makeVoiceLike (48000, 3, 3);
		std::vector<float> l (512), r (512), il (512), ir (512);
		std::mt19937 rng (5);
		float lim = 1.0001f; // RAW / bypass laissent passer le dry tel quel : la limite est celle de l'entrée
		for (double v : x) lim = std::max (lim, static_cast<float> (std::fabs (v)) * 1.0001f);
		gAllocs = 0;
		bool finite = true;
		for (size_t pos = 0; pos + 512 <= x.size (); pos += 512)
		{
			ParamSet ps;
			for (uint32_t i = 0; i < id::kNumParams; ++i) if (i != id::kBypass) ps.norm[i] = (rng () % 1000) / 999.0;
			gCount = true;
			e.setParams (makeEngineParams (ps));
			gCount = false;
			for (int i = 0; i < 512; ++i) { il[i] = static_cast<float> (x[pos + i]); ir[i] = il[i]; }
			const float* in[2] = {il.data (), ir.data ()}; float* o[2] = {l.data (), r.data ()};
			gCount = true;
			e.process<float> (in, o, 512, 2);
			gCount = false;
			for (int i = 0; i < 512; ++i) if (!std::isfinite (l[i]) || !std::isfinite (r[i]) || std::fabs (l[i]) > lim || std::fabs (r[i]) > lim)
			{ if (finite) std::printf ("  bloc %zu sample %d : %g %g ", pos / 512, i, l[i], r[i]); finite = false; }
		}
		CHECK (finite, "automation aléatoire : sortie non finie ou > 0 dBFS");
		CHECK (gAllocs.load () == 0, "%ld allocations pendant l'automation", gAllocs.load ());
	}
	// mono
	{
		TamaEngine e; e.prepare (48000, 1);
		const auto x = makeVoiceLike (48000, 1, 1);
		std::vector<float> o (x.size ()), in (x.size ());
		for (size_t i = 0; i < x.size (); ++i) in[i] = static_cast<float> (x[i]);
		const float* pi[1] = {in.data ()}; float* po[1] = {o.data ()};
		e.process<float> (pi, po, static_cast<int> (x.size ()), 1);
		bool finite = true; double mx = 0;
		for (float v : o) { finite = finite && std::isfinite (v); mx = std::max<double> (mx, std::fabs (v)); }
		CHECK (finite && mx > 0.01, "mono");
	}
}

static void testLatency ()
{
	std::printf ("[latence]\n");
	for (int mode = 0; mode < 2; ++mode)
	{
		TamaEngine e; e.prepare (48000, 2);
		ParamSet ps; ps.norm[id::kMode] = mode; ps.norm[id::kRaw] = 1.0;
		e.setParams (makeEngineParams (ps));
		const int expected = mode ? TamaEngine::kPureLatency : 0;
		CHECK (e.latencySamples () == expected, "latence annoncée %d", e.latencySamples ());
		const int N = 48000;
		std::vector<float> a (N, 0.0f), b (N, 0.0f), oa (N), ob (N);
		a[20000] = 0.5f; b[20000] = 0.5f;
		const float* in[2] = {a.data (), b.data ()}; float* out[2] = {oa.data (), ob.data ()};
		e.process<float> (in, out, N, 2);
		int pk = 0; float mx = 0;
		for (int i = 0; i < N; ++i) if (std::fabs (oa[i]) > mx) { mx = std::fabs (oa[i]); pk = i; }
		CHECK (pk - 20000 == expected, "écoute RAW : impulsion à +%d (attendu %d)", pk - 20000, expected);
	}
}

static void testBlockInvariance ()
{
	std::printf ("[invariance au découpage en blocs]\n");
	const auto x = makeVoiceLike (48000, 2.0, 11);
	std::vector<std::vector<double>> a, b;
	for (int pass = 0; pass < 2; ++pass)
	{
		TamaEngine e; e.prepare (48000, 2);
		ParamSet ps; ps.norm[id::kMode] = 1.0;
		e.setParams (makeEngineParams (ps));
		runBlocks<double> (e, x, pass ? b : a, pass ? std::vector<int> {17, 64, 200, 5, 1024} : std::vector<int> {64});
	}
	double num = 0, den = 0;
	for (size_t i = 24000; i < x.size (); ++i) { const double d = a[0][i] - b[0][i]; num += d * d; den += a[0][i] * a[0][i]; }
	const double db = 10 * std::log10 (num / (den + 1e-30) + 1e-30);
	std::printf ("  différence entre découpages : %.1f dB\n", db);
	CHECK (db < -40.0, "le découpage en blocs change trop le son (%.1f dB)", db);
}

static void testDynamics ()
{
	std::printf ("[dynamique]\n");
	// SQUEEZE doit réduire la dynamique crête/RMS court terme d'un signal modulé
	auto spread = [] (double squeeze) {
		TamaEngine e; e.prepare (48000, 2);
		ParamSet ps; ps.norm[id::kSqueeze] = squeeze / 100.0; ps.norm[id::kAuto] = 0.0;
		e.setParams (makeEngineParams (ps));
		const int N = 48000 * 4;
		std::vector<float> l (N), r (N), o1 (N), o2 (N);
		for (int i = 0; i < N; ++i) { const double t = i / 48000.0, a = (std::fmod (t, 1.0) < 0.5 ? 0.6 : 0.06); l[i] = r[i] = static_cast<float> (a * std::sin (2 * kPi * 220 * t)); }
		const float* in[2] = {l.data (), r.data ()}; float* out[2] = {o1.data (), o2.data ()};
		e.process<float> (in, out, N, 2);
		auto rms = [&] (int a0, int a1) { double s = 0; for (int i = a0; i < a1; ++i) s += o1[i] * o1[i]; return 10 * std::log10 (s / (a1 - a0) + 1e-30); };
		return rms (48000 * 2 + 4000, 48000 * 2 + 20000) - rms (48000 * 3 + 20000, 48000 * 3 + 40000); // fort - faible
	};
	const double soft = spread (0), hard = spread (100);
	std::printf ("  écart fort/faible : SQUEEZE 0 = %.1f dB, SQUEEZE 100 = %.1f dB\n", soft, hard);
	CHECK (hard < soft - 2.0, "SQUEEZE ne comprime pas");
}

int main ()
{
	testParams ();
	testStabilityAndRealtime ();
	testLatency ();
	testBlockInvariance ();
	testDynamics ();
	if (gFail) { std::printf ("\n%d ECHEC(S)\n", gFail); return 1; }
	std::printf ("\nTOUS LES TESTS PASSENT\n");
	return 0;
}
