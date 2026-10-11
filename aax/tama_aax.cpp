// TAMA-VOX : description AAX (Pro Tools) pour le wrapper VST3 -> AAX du SDK Steinberg.
// Compilé uniquement avec -DTAMA_BUILD_AAX=ON -DTAMA_AAX_SDK_DIR=<SDK AAX d'Avid> (Windows ou macOS).
//
// IMPORTANT : les identifiants ci-dessous (fabricant, produit, plug-in) doivent être enregistrés chez Avid
// avant toute distribution. Les valeurs fournies sont des valeurs de développement. Le binaire .aaxplugin
// doit ensuite être signé avec PACE/iLok (wraptool) pour être chargé par la version de production de Pro Tools.
#include "TamaCids.h"
#include "TamaParams.h"
#include "public.sdk/source/vst/aaxwrapper/aaxwrapper_description.h"
#include "pluginterfaces/base/futils.h"
#include <cstring>

using namespace Steinberg;

namespace
{
// Compteur de réduction de gain affiché dans Pro Tools
AAX_Meter_Desc kMeters[] = {
	{"GainReduction", tama::id::kMGr, 0 /*AAX_eMeterOrientation_Default*/, 2 /*AAX_eMeterType_CLGain*/},
	{nullptr, 0, 0, 0},
};

AAX_Plugin_Desc kPlugins[] = {
	// Mono
	{"com.omyll.tamavox.mono", "TAMA-VOX", CCONST ('O', 'T', 'V', '1'), CCONST ('O', 'T', 'A', '1'),
	 1, 1, 0, nullptr, nullptr, kMeters,
	 0 /* latence : SNAP = 0 ; en mode PURE le wrapper relit getLatencySamples() */},
	// Stéréo
	{"com.omyll.tamavox.stereo", "TAMA-VOX", CCONST ('O', 'T', 'V', '2'), CCONST ('O', 'T', 'A', '2'),
	 2, 2, 0, nullptr, nullptr, kMeters,
	 0},
	{nullptr, nullptr, 0, 0, 0, 0, 0, nullptr, nullptr, nullptr, 0},
};

AAX_Effect_Desc kEffect = {
	"Omyll",                       // fabricant
	"TAMA-VOX",                    // produit
	CCONST ('O', 'M', 'Y', 'L'),   // ID fabricant (à enregistrer chez Avid)
	CCONST ('T', 'V', 'O', 'X'),   // ID produit
	TAMA_VST3_CATEGORY,            // catégorie VST3 utilisée pour déduire la catégorie AAX
	{0},                           // UID VST3 : rempli dans AAXWrapper_GetDescription
	1,                             // version
	nullptr,                       // pas de fichier de pages
	kPlugins,
};
} // namespace

// Force l'édition de liens de tout le wrapper AAX
int* forceLinkAAXWrapper = &AAXWrapper_linkAnchor;

AAX_Effect_Desc* AAXWrapper_GetDescription ()
{
	std::memcpy (kEffect.mVST3PluginID, static_cast<const char*> (tama::kProcessorUID), sizeof (kEffect.mVST3PluginID));
	return &kEffect;
}
