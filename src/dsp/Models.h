// TAMA-VOX : modèles de compresseur (réglages + couleur analogique) et presets d'usine.
// Les valeurs de décalage (thr/ratio/knee) s'ajoutent à ce que calcule SQUEEZE.
#pragma once
#include "../StdCommon.h"
#include <array>
#include <cstddef>

namespace tama
{
struct CompModel
{
	const char* name;   // nom affiché
	const char* tag;    // étiquette courte
	double thr, ratio, knee;       // décalages
	double att, adapt, rmin, rmax, mem;
	double lo, mid, mf, hi;        // couleur : étagère graves, médiums (fréquence mf), étagère aigus (dB)
	double k, b;                   // saturation tanh : intensité, asymétrie (harmoniques paires)
	double lift;                   // relèvement des bas niveaux (dB)
	const char* info;
};

inline constexpr std::array<CompModel, 10> kModels = {{
	{"TAMA · Opto T4", "TAMA", 0, 0, 0, 3, 1.0, 45, 800, 350, 0, 0, 3000, 0, 0, 0, 1.5,
	 "Le détecteur maison du cahier des charges : un opto à release adaptatif. Plus la voix est dense, plus le release s'allonge (rap serré, chant tenu respirant). Neutre et transparent, la couleur vient surtout de la lampe 12AX7."},
	{"Teletronix LA-2A", "LA-2A", 0, -3, 15, 10, 1.0, 60, 1500, 800, 1.5, 0, 3000, -2, 1.1, 0.15, 1.0,
	 "Compresseur optique à tubes (cellule électroluminescente + photorésistance). Attaque lente (~10 ms), release en deux temps : rapide sur la première moitié puis très lent. Ratio doux, genou arrondi.\nGoût : chaud, épais, soyeux. Arrondit les aigus, ajoute des harmoniques paires. Le roi des voix chantées et des basses tenues, il ne s'entend presque pas."},
	{"UREI 1176 (FET)", "1176", 0, 6, -15, 0.3, 0.4, 30, 200, 120, 0, 1.5, 3500, 1, 2.2, 0.05, 0,
	 "Compresseur à transistor FET, attaque ultra rapide (de 20 à 800 microsecondes), release très court, ratios 4:1 à 20:1 (et le mode « tous les boutons » à fond).\nGoût : agressif, claquant, présent. Ajoute du grain impair et de la brillance dans les médiums hauts. Idéal pour les voix rap, rock et tout ce qui doit mordre."},
	{"Fairchild 670", "670", -2, -2.5, 10, 0.6, 0.7, 50, 900, 600, 2, 0, 3000, -1, 1.6, 0.2, 0,
	 "Compresseur à lampes à gain variable (variable-mu), avec des transformateurs massifs. Attaque très rapide, release en plusieurs positions (jusqu'à plusieurs secondes).\nGoût : large, luxueux, collé. Densité dans le bas, aigus enveloppés. Le son Beatles, une voix qui « fond » dans le mix."},
	{"Manley Vari-Mu", "Vari-Mu", 1, -3.5, 18, 18, 1.0, 120, 1400, 700, 1, 0, 3000, 0.5, 1.2, 0.2, 0,
	 "Compresseur à tubes variable-mu, ratio faible et genou très doux, attaque lente.\nGoût : soyeux et riche, une compression qu'on sent à peine, avec une saturation harmonique douce. Parfait pour lisser une voix sans la raidir."},
	{"Neve 33609", "33609", -1, -3, 5, 5, 0.7, 100, 500, 400, 2, 0, 3000, 1, 1.3, 0.12, 0,
	 "Compresseur/limiteur de console britannique, transformateurs Marinair, compression douce avec une attaque et un release ajustables.\nGoût : chaud et rond dans le bas, soyeux dans le haut. Donne du corps et de la proximité aux voix, un son « grand studio »."},
	{"SSL G Bus", "SSL G", -1, -2.5, -10, 10, 0.5, 100, 400, 300, 0.5, 0, 3000, 1.5, 0.6, 0, 0,
	 "Compresseur VCA du bus de console SSL. Attaques 0,1 à 30 ms, release auto ou fixe, ratios 2:1, 4:1, 10:1.\nGoût : net, ouvert, qui « colle » et pousse vers l'avant, avec un léger claquement. Peu de couleur, beaucoup de cohésion."},
	{"dbx 160", "160", -2, 4, -20, 15, 0.0, 120, 120, 250, 1, 0, 3000, 0, 1.0, 0, 0,
	 "Compresseur VCA à genou dur et détection RMS, attaque dépendante du programme.\nGoût : punch et densité très directs, un peu cru et « plaqué ». Il écrase de façon franche, très rock et très pop des années 80."},
	{"API 2500", "2500", -2, 2, -8, 3, 0.3, 60, 300, 200, 1, 1, 2000, 1.5, 1.0, 0, 0,
	 "Compresseur VCA à genou réglable, détecteur à « poussée » (Thrust) qui évite de réagir aux graves, transformateurs de sortie API.\nGoût : musclé, projeté, impact dans les médiums. La voix reste devant, avec de la tranche."},
	{"Empirical Distressor", "Distressor", -1, 0, 0, 1, 0.5, 50, 400, 250, 0, 1, 2500, 0.5, 1.6, 0.1, 0,
	 "Compresseur numérique contrôlé, analogique dans le chemin audio, avec des modes qui imitent les autres compresseurs et une distorsion harmonique réglable (deuxième ou troisième).\nGoût : le couteau suisse, du transparent à l'agressif, avec une saturation qu'on dose."},
}};

struct FactoryPreset
{
	const char* name;
	double drive;   // 1..11
	int flavor;     // 0 pink, 1 blue, 2 black
	double squeeze; // 0..100
	int mode;       // 0 snap, 1 pure
	int model;      // index dans kModels
};
inline constexpr std::array<FactoryPreset, 4> kFactoryPresets = {{
	{"Lead pop", 5.0, 0, 45, 1, 0},
	{"Rap dense", 7.0, 1, 70, 0, 2},
	{"Ballade tenue", 3.5, 0, 30, 1, 1},
	{"Crunch brut", 9.5, 2, 55, 0, 2},
}};

// Réglages de compresseur rapides (menu Compresseur) : décalages + temps, couleur inchangée.
struct CompPreset
{
	const char* name;
	double thr, ratio, knee, att, adapt, rmin, rmax, mem;
};
inline constexpr std::array<CompPreset, 5> kCompPresets = {{
	{"Opto T4 (défaut)", 0, 0, 0, 3, 100, 45, 800, 350},
	{"Rap rapide", -3, 2, -8, 1, 60, 25, 250, 150},
	{"Chant tenu, doux", 2, -0.5, 10, 12, 100, 120, 1200, 700},
	{"Pompe vintage", -6, 3, 0, 30, 0, 400, 400, 350},
	{"Limiteur dur", -4, 10, -18, 0.3, 20, 40, 120, 100},
}};
} // namespace tama
