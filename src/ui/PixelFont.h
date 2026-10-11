// TAMA-VOX : police bitmap 5x7 (avec accents français), dessinée pixel par pixel. Aucune police système requise.
#pragma once
#include "../StdCommon.h"
#include "Painter.h"
#include <string_view>

namespace tama::ui::font
{
constexpr int kAdvance = 6;  // 5 colonnes + 1 d'espacement
constexpr int kLineHeight = 11; // hauteur de ligne (échelle 1)

enum class Align { Left, Centre, Right };

int textWidth (std::string_view utf8, int scale);                       // largeur en pixels de la plus longue ligne
int textLines (std::string_view utf8);
void drawText (Painter& p, std::string_view utf8, double x, double y, int scale, Color c, Align a = Align::Left);
// Coupe un texte en lignes de largeur maximale (en caractères), en respectant les \n existants.
std::string wrap (std::string_view utf8, int maxChars);
} // namespace tama::ui::font
