# TAMA-VOX — processeur vocal VST3 / AAX (by Omyll · Abrasion x Onirique)

Habillage « Onirique » (rouge sang, braise, sigil blanc) sur le Tamagotchi d'origine.

C++20 / CMake, sans JUCE, **sans aucun DRM** (volontairement, malgré le cahier des charges).
SDK VST3 3.7.11 (FetchContent), interface VSTGUI 4.13 (Windows, macOS, Linux/X11).

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```
Le `.vst3` est copié après compilation dans le dossier VST3 de l'utilisateur (`-DTAMA_DEPLOY=OFF` pour désactiver, `-DTAMA_DEPLOY_DIR=...` pour changer).
Linux : paquets listés dans `.github/workflows/build.yml`. macOS : binaire universel x86_64+arm64 (10.15+). Windows : Visual Studio 2022.
Options utiles : `TAMA_ENABLE_GUI=OFF` (sans éditeur), `TAMA_VST3_SDK_DIR=...` (SDK local), `TAMA_BUILD_UI_PREVIEW=ON` (rendu PNG via Cairo).

## AAX (Windows / macOS)
Le SDK AAX d'Avid et la signature PACE/iLok ne sont pas redistribuables :
1. Obtenir le SDK AAX (compte développeur Avid), puis `-DTAMA_BUILD_AAX=ON -DTAMA_AAX_SDK_DIR=/chemin/AAX_SDK`.
2. Remplacer les identifiants provisoires `'OMYL'` / `'TVOX'` dans `aax/tama_aax.cpp` par ceux enregistrés chez Avid.
3. Signer le `.aaxplugin` avec PACE (wraptool) — Pro Tools refuse un plug-in non signé en version release.
La latence AAX est déclarée statiquement : 0 en mode SNAP ; le mode PURE ajoute 64 échantillons (à compenser par le DAW).

## Écarts / limites connus
- Aucun DRM. Face arrière : « BY OMYLL / ABRASION X ONIRIQUE » ; le sigil Onirique affiché au dos et dans l'en-tête est le logo fourni (masque 4 niveaux dans `src/ui/SigilData.h`).
- Testé ici (Linux) : DSP (CTest), validator VST3 officiel (47 réussis), rendu de la scène en PNG. **Non testé** : compilation Windows/macOS, AAX, rendu VSTGUI réel dans un DAW, plug-in avec GUI lié sous Linux.
