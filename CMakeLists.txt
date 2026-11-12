cmake_minimum_required(VERSION 3.16)
project(Owlio VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Núcleo sem dependência de interface (modelo, paginação, Fountain, FDX, edição).
add_library(owlio_core STATIC
  src/document.cpp
  src/layout.cpp
  src/fountain.cpp
  src/fdx.cpp
  src/editor.cpp
)
target_include_directories(owlio_core PUBLIC src)
target_compile_options(owlio_core PRIVATE -Wall -Wextra)

add_executable(owlio_tests tests/tests.cpp)
target_link_libraries(owlio_tests PRIVATE owlio_core)

# Interface gráfica em Qt 6.
find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Svg LinguistTools)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(UI_SOURCES
  src/mainwindow.cpp src/mainwindow.h
  src/scriptview.cpp src/scriptview.h
  src/pagepainter.cpp src/pagepainter.h
  src/theme.cpp src/theme.h
  src/carddelegate.cpp src/carddelegate.h
  src/sceneboard.cpp src/sceneboard.h
  src/avatars.cpp src/avatars.h
  src/titlepagedialog.cpp src/titlepagedialog.h
  src/printdialog.cpp src/printdialog.h
  src/characterdialog.cpp src/characterdialog.h
  src/settingsdialog.cpp src/settingsdialog.h
  src/i18n.cpp src/i18n.h
  src/autosave.cpp src/autosave.h
  src/beatboard.cpp src/beatboard.h
  src/beatdialog.cpp src/beatdialog.h
  resources/owlio.qrc
)
set(UI_LIBS owlio_core Qt6::Widgets Qt6::PrintSupport Qt6::Svg)

add_executable(owlio src/main.cpp ${UI_SOURCES})
target_link_libraries(owlio PRIVATE ${UI_LIBS})
target_compile_options(owlio PRIVATE -Wall -Wextra)

# Teste de interface: envia teclas reais para a janela (QTest).
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(owlio_gui_test tests/gui_test.cpp ${UI_SOURCES})
target_link_libraries(owlio_gui_test PRIVATE ${UI_LIBS} Qt6::Test)

# Traduções. O inglês é o idioma do código; owlio_en.ts só guarda as formas de plural.
# Depois de mudar textos da interface: cmake --build build --target update_translations
qt_add_translations(TARGETS owlio owlio_gui_test
  TS_FILES translations/owlio_pt_BR.ts
  PLURALS_TS_FILE translations/owlio_en.ts
  RESOURCE_PREFIX "/i18n"
  LUPDATE_OPTIONS -no-obsolete -locations none
)
