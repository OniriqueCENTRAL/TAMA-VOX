# Rendu PNG de l'interface avec Cairo (outil de développement, pas livré avec le plugin)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
add_executable(ui_preview ui_preview.cpp
  ${CMAKE_SOURCE_DIR}/src/ui/PixelFont.cpp ${CMAKE_SOURCE_DIR}/src/ui/Lcd.cpp ${CMAKE_SOURCE_DIR}/src/ui/TamaScene.cpp)
target_include_directories(ui_preview PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(ui_preview PRIVATE cxx_std_20)
target_link_libraries(ui_preview PRIVATE PkgConfig::CAIRO)
