// TAMA-VOX : identifiants VST3 (ne jamais les changer après une première publication).
#pragma once
#include "StdCommon.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

namespace tama
{
inline const Steinberg::FUID kProcessorUID (0x54414D41, 0x564F5850, 0x524F4331, 0x4F4D594C);
inline const Steinberg::FUID kControllerUID (0x54414D41, 0x564F5843, 0x54524C32, 0x4F4D594C);

#define TAMA_VST3_CATEGORY Steinberg::Vst::PlugType::kFxDynamics
#define TAMA_PLUGIN_NAME "TAMA-VOX"
#define TAMA_COMPANY "Omyll"
#define TAMA_WEB "https://omyll.invalid"
#define TAMA_MAIL "mailto:contact@omyll.invalid"
} // namespace tama
