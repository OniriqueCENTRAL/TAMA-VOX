// Outil de développement : rend l'interface TAMA-VOX en PNG (sans DAW) pour la vérifier visuellement.
// Usage : ui_preview <dossier de sortie>
#include "CairoSink.h"
#include "ui/TamaScene.h"
#include <cstdio>
#include <string>

using namespace tama;
using namespace tama::ui;

struct MockHost : IUiHost
{
	ParamSet ps;
	double meters[kNumMeters] {};
	int edits = 0;
	double getNorm (uint32_t pid) const override { return ps.norm[pid]; }
	void uiBegin (uint32_t) override {}
	void uiSet (uint32_t pid, double n) override { ps.norm[pid] = clamp01 (n); ++edits; }
	void uiEnd (uint32_t) override {}
	double getMeter (uint32_t mid) const override { return meters[mid - id::kMeterBase]; }
	void applyModel (int m) override { ps.norm[id::kModel] = toNorm (spec (id::kModel), m); for (uint32_t p : kModelDrivenParams) ps.norm[p] = toNorm (spec (p), modelDefault (m, p)); }
	void applyPreset (int i) override
	{
		const auto& f = kFactoryPresets[static_cast<size_t> (i)];
		ps.norm[id::kDrive] = toNorm (spec (id::kDrive), f.drive); ps.norm[id::kFlavor] = toNorm (spec (id::kFlavor), f.flavor);
		ps.norm[id::kSqueeze] = toNorm (spec (id::kSqueeze), f.squeeze); ps.norm[id::kMode] = toNorm (spec (id::kMode), f.mode);
		applyModel (f.model);
	}
	void applyCompPreset (int) override {}
	void resetCompressor () override {}
	void resetAll () override { ps = ParamSet {}; }
	void meter (uint32_t mid, double v) { meters[mid - id::kMeterBase] = v; }
	void setP (uint32_t pid, double plain) { ps.norm[pid] = toNorm (spec (pid), plain); }
};

static void render (TamaScene& sc, const std::string& file, double& now, int frames = 40)
{
	cairo_surface_t* s = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, TamaScene::kW, TamaScene::kH);
	cairo_t* cr = cairo_create (s);
	for (int i = 0; i < frames; ++i)
	{
		now += 33.0;
		CairoSink sink (cr);
		Painter p (sink);
		sc.draw (p, now);
	}
	cairo_surface_write_to_png (s, file.c_str ());
	cairo_destroy (cr);
	cairo_surface_destroy (s);
	std::printf ("écrit %s\n", file.c_str ());
}

int main (int argc, char** argv)
{
	const std::string dir = argc > 1 ? argv[1] : ".";
	double now = 1000;
	{
		MockHost h; h.meter (id::kMLufsIn, -18); h.meter (id::kMLufsOut, -16); h.meter (id::kMPeak, -9); h.meter (id::kMLamp, 0.15); h.meter (id::kMTau, 260);
		TamaScene sc (h);
		render (sc, dir + "/ui_idle_pink.png", now);
	}
	{
		MockHost h; h.setP (id::kFlavor, 1); h.setP (id::kDrive, 8.5); h.setP (id::kSqueeze, 80); h.setP (id::kMode, 1);
		h.meter (id::kMLufsIn, -17); h.meter (id::kMLufsOut, -15); h.meter (id::kMPeak, -3); h.meter (id::kMLamp, 0.85); h.meter (id::kMGr, 11); h.meter (id::kMSquash, 0.8); h.meter (id::kMTau, 90); h.meter (id::kMMakeup, 3.2);
		TamaScene sc (h);
		render (sc, dir + "/ui_squeezed_blue.png", now);
	}
	{
		MockHost h; h.setP (id::kFlavor, 2); h.setP (id::kRaw, 1); h.meter (id::kMLufsIn, -20); h.meter (id::kMPeak, -12);
		TamaScene sc (h);
		sc.debugOpenMenu (4);
		render (sc, dir + "/ui_menu_comp_black.png", now);
	}
	{
		MockHost h; h.meter (id::kMDuck, 6); h.meter (id::kMLufsIn, -25); h.meter (id::kMPeak, -14);
		TamaScene sc (h);
		sc.debugOpenMenu (5);
		render (sc, dir + "/ui_menu_model.png", now);
		sc.debugOpenMenu (-1);
		sc.debugDialog ("Teletronix LA-2A", kModels[1].info);
		render (sc, dir + "/ui_dialog.png", now, 2);
	}
	{
		MockHost h;
		TamaScene sc (h);
		sc.debugFlip (true);
		render (sc, dir + "/ui_back.png", now);
	}
	{
		MockHost h; h.setP (id::kAuto, 0); h.setP (id::kBypass, 1);
		TamaScene sc (h);
		sc.debugOpenMenu (2);
		render (sc, dir + "/ui_menu_flavor.png", now);
	}
	return 0;
}
