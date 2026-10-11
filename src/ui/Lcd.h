// TAMA-VOX : écran LCD pixel-art 64x60 (une fille-chat qui s'écrase contre la vitre, lampe 12AX7 dans l'écran).
#pragma once
#include "../StdCommon.h"
#include <cstdint>

namespace tama::ui
{
constexpr int kLcdW = 64, kLcdH = 60;
enum LcdPix : uint8_t { kLcdBg = 0, kLcdInk = 1, kLcdGhost = 2, kLcdShade = 3 }; // fond, encre, buée légère, ombre

struct LcdState
{
	double lamp = 0;       // 0..1 : intensité de la lampe (saturation)
	double squash = 0;     // 0..1 : écrasement (réduction de gain)
	double grDb = 0;       // réduction de gain en dB
	double duckDb = 0;     // de-esser en dB
	double glitch = 0;     // > 0 : transitoire saturée -> parasites
	bool playing = false;  // signal présent (sinon la chatte dort)
	bool raw = false;      // écoute RAW (ou bypass) : elle est triste
	bool autoGain = true;
	bool pure = false;
	bool tooLoud = false;
};

// Remplit out[kLcdW*kLcdH] avec des LcdPix. `seed` change à chaque image (scintillement de la lampe).
void renderLcd (uint8_t* out, const LcdState& s, double nowMs, uint32_t seed);
} // namespace tama::ui
