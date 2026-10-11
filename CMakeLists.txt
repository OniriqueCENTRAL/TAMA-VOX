cmake_minimum_required(VERSION 3.22)

# ------------------------------------------------------------------------------------------------
# TAMA-VOX - processeur vocal VST3 / AAX  (Windows, macOS, Linux)
# Pas de JUCE, pas de DRM. SDK VST3 de Steinberg (licence MIT/GPLv3 selon version, voir README).
# ------------------------------------------------------------------------------------------------

# macOS : binaire universel et cible minimale, à régler AVANT project()
if(APPLE)
  option(TAMA_MAC_UNIVERSAL "Construire x86_64 + arm64" ON)
  if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Version minimale de macOS")
  endif()
  if(TAMA_MAC_UNIVERSAL AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Architectures macOS")
  endif()
endif()

project(TAMA_VOX VERSION 1.0.0.0 DESCRIPTION "TAMA-VOX vocal processor" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ------------------------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------------------------
option(TAMA_ENABLE_GUI    "Interface graphique VSTGUI (sinon : plugin sans éditeur, utile pour tests)" ON)
option(TAMA_BUILD_TESTS   "Compiler les tests DSP (CTest)" ON)
option(TAMA_BUILD_AAX     "Compiler la cible AAX (nécessite le SDK AAX d'Avid, voir README)" OFF)
option(TAMA_BUILD_UI_PREVIEW "Outil de rendu PNG de l'interface (nécessite Cairo, dev seulement)" OFF)
set(TAMA_VST3_SDK_TAG "v3.7.11_build_10" CACHE STRING "Tag Git du SDK VST3 (FetchContent)")
set(TAMA_VST3_SDK_DIR "" CACHE PATH "Chemin d'un SDK VST3 déjà cloné (évite le téléchargement)")
set(TAMA_AAX_SDK_DIR "" CACHE PATH "Chemin du SDK AAX d'Avid (contient Interfaces/AAX.h)")

if(DEFINED ENV{CI})
  set(_tama_deploy_default OFF)
else()
  set(_tama_deploy_default ON)
endif()
option(TAMA_DEPLOY "Copier le .vst3 dans le dossier VST3 de l'utilisateur après la compilation" ${_tama_deploy_default})
set(TAMA_DEPLOY_DIR "" CACHE PATH "Dossier de déploiement (par défaut : dossier VST3 standard de l'OS)")

# ------------------------------------------------------------------------------------------------
# SDK VST3
# ------------------------------------------------------------------------------------------------
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE) # le `validator` est construit quand même
if(TAMA_ENABLE_GUI)
  set(SMTG_ENABLE_VSTGUI_SUPPORT ON CACHE BOOL "" FORCE)
else()
  set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
endif()
# Le déploiement est fait par TAMA_DEPLOY (copie), pas par lien symbolique (droits admin sous Windows).
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "Lancer le validator après la compilation")
if(TAMA_BUILD_AAX)
  if(NOT TAMA_AAX_SDK_DIR OR NOT EXISTS "${TAMA_AAX_SDK_DIR}/Interfaces/AAX.h")
    message(FATAL_ERROR "TAMA_BUILD_AAX=ON mais TAMA_AAX_SDK_DIR ne pointe pas sur le SDK AAX (Interfaces/AAX.h introuvable).")
  endif()
  set(SMTG_AAX_SDK_PATH "${TAMA_AAX_SDK_DIR}" CACHE PATH "" FORCE)
endif()

include(FetchContent)
if(TAMA_VST3_SDK_DIR)
  set(FETCHCONTENT_SOURCE_DIR_VST3SDK "${TAMA_VST3_SDK_DIR}")
  FetchContent_Declare(vst3sdk)
else()
  FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        ${TAMA_VST3_SDK_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
endif()
FetchContent_MakeAvailable(vst3sdk)
list(APPEND CMAKE_MODULE_PATH "${vst3sdk_SOURCE_DIR}/cmake/modules")
include(SMTG_AddVST3Library)
smtg_enable_vst3_sdk() # recommandé pour un projet tiers : point d'entrée (ModuleEntry) et variables du SDK
if(TAMA_BUILD_AAX)
  include(SMTG_AddAAXLibrary)
endif()

# ------------------------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------------------------
set(TAMA_DSP_HEADERS
  src/dsp/AdaaSaturator.h src/dsp/AirDeesser.h src/dsp/Biquad.h src/dsp/Common.h src/dsp/Loudness.h
  src/dsp/Models.h src/dsp/OptoDetector.h src/dsp/Oversampling.h src/dsp/Simd.h src/dsp/TamaEngine.h
  src/dsp/TransientSplit.h src/dsp/WdfTransformer.h)

set(TAMA_PLUGIN_SOURCES
  src/entry.cpp
  src/TamaProcessor.cpp src/TamaProcessor.h
  src/TamaController.cpp src/TamaController.h
  src/TamaParams.h src/TamaCids.h src/version.h
  ${TAMA_DSP_HEADERS})

# L'interface est coupée en deux : une scène indépendante de toute bibliothèque (testable, rendue
# par un « peintre » abstrait) et la colle VSTGUI.
set(TAMA_UI_SCENE_SOURCES
  src/ui/UiHost.h src/ui/Painter.h src/ui/PixelFont.h src/ui/PixelFont.cpp
  src/ui/SigilData.h src/ui/Lcd.h src/ui/Lcd.cpp src/ui/TamaScene.h src/ui/TamaScene.cpp)
set(TAMA_UI_VSTGUI_SOURCES
  src/ui/TamaEditor.h src/ui/TamaEditor.cpp src/ui/TamaView.h src/ui/TamaView.cpp)

if(TAMA_ENABLE_GUI)
  list(APPEND TAMA_PLUGIN_SOURCES ${TAMA_UI_SCENE_SOURCES} ${TAMA_UI_VSTGUI_SOURCES})
endif()

if(WIN32)
  list(APPEND TAMA_PLUGIN_SOURCES resource/TAMA_VOX.rc)
endif()

# Options communes de compilation du code TAMA-VOX (pas appliquées au SDK)
function(tama_target_defaults target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_definitions(${target} PRIVATE TAMA_HAS_GUI=$<BOOL:${TAMA_ENABLE_GUI}>)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8 /Zc:__cplusplus /DNOMINMAX /D_USE_MATH_DEFINES /fp:precise /W3 /wd4244 /wd4267 /wd4996)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -fno-fast-math -ffp-contract=off)
  endif()
endfunction()

# ------------------------------------------------------------------------------------------------
# Cible VST3
# ------------------------------------------------------------------------------------------------
smtg_add_vst3plugin(TAMA_VOX ${TAMA_PLUGIN_SOURCES})
smtg_target_configure_version_file(TAMA_VOX)
tama_target_defaults(TAMA_VOX)
target_link_libraries(TAMA_VOX PRIVATE sdk)
if(TAMA_ENABLE_GUI)
  target_link_libraries(TAMA_VOX PRIVATE vstgui_support)
endif()

if(SMTG_MAC)
  smtg_target_set_bundle(TAMA_VOX BUNDLE_IDENTIFIER "com.omyll.tamavox" COMPANY_NAME "Omyll")
endif()
if(SMTG_LINUX)
  target_link_libraries(TAMA_VOX PRIVATE ${CMAKE_DL_LIBS})
endif()

# Déploiement : copie du paquet .vst3 dans le dossier VST3 de l'utilisateur
if(TAMA_DEPLOY)
  if(TAMA_DEPLOY_DIR)
    set(_tama_dest "${TAMA_DEPLOY_DIR}")
  else()
    smtg_get_default_vst3_path()
    set(_tama_dest "${SMTG_PLUGIN_TARGET_DEFAULT_PATH}")
    if(WIN32)
      # dossier utilisateur : pas de droits administrateur nécessaires
      set(_tama_dest "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
    endif()
  endif()
  get_target_property(_tama_pkg TAMA_VOX SMTG_PLUGIN_PACKAGE_PATH)
  add_custom_command(TARGET TAMA_VOX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_tama_dest}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${_tama_pkg}" "${_tama_dest}/TAMA_VOX.vst3"
    COMMENT "TAMA-VOX : déploiement vers ${_tama_dest}")
endif()

# ------------------------------------------------------------------------------------------------
# Cible AAX (Windows / macOS uniquement ; le SDK AAX et la signature PACE ne sont pas fournis)
# ------------------------------------------------------------------------------------------------
if(TAMA_BUILD_AAX)
  if(NOT TARGET aaxwrapper)
    message(FATAL_ERROR "Le wrapper AAX du SDK VST3 n'a pas été créé : le SDK AAX est-il bien reconnu (SMTG_AAX_SDK_PATH) ? AAX n'existe que sous Windows et macOS.")
  endif()
  set(TAMA_AAX_SOURCES ${TAMA_PLUGIN_SOURCES} aax/tama_aax.cpp)
  smtg_add_aaxplugin(TAMA_VOX_AAX ${TAMA_AAX_SOURCES})
  smtg_target_configure_version_file(TAMA_VOX_AAX)
  tama_target_defaults(TAMA_VOX_AAX)
  target_link_libraries(TAMA_VOX_AAX PRIVATE sdk aaxwrapper)
  if(TAMA_ENABLE_GUI)
    target_link_libraries(TAMA_VOX_AAX PRIVATE vstgui_support)
  endif()
  if(SMTG_MAC)
    smtg_target_set_bundle(TAMA_VOX_AAX BUNDLE_IDENTIFIER "com.omyll.tamavox.aax" COMPANY_NAME "Omyll")
  endif()
endif()

# ------------------------------------------------------------------------------------------------
# Tests et outils de développement
# ------------------------------------------------------------------------------------------------
enable_testing()
if(TAMA_BUILD_TESTS OR TAMA_BUILD_UI_PREVIEW)
  add_subdirectory(tests)
endif()
