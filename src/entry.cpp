// TAMA-VOX : point d'entrée de la fabrique VST3 (et AAX via le wrapper du SDK).
#include "StdCommon.h"
#include "TamaCids.h"
#include "TamaController.h"
#include "TamaProcessor.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "version.h"

BEGIN_FACTORY_DEF (TAMA_COMPANY, TAMA_WEB, TAMA_MAIL)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (tama::kProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            TAMA_PLUGIN_NAME,
	            Vst::kDistributable,
	            TAMA_VST3_CATEGORY,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            tama::TamaProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (tama::kControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            TAMA_PLUGIN_NAME " Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            tama::TamaController::createInstance)

END_FACTORY
