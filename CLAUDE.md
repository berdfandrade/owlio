# Owlio

## Codebase Overview

Owlio is a screenwriting app (Final Draft style) that lays scripts out on real US Letter pages, with index cards, character sheets, dual dialogue, autosave and Fountain/FDX files. A Qt-free core (model, pagination, formats, editing) sits under a Qt Widgets UI that renders screen, PDF and print through one `PagePainter`.

**Stack**: C++17, Qt 6 (Widgets, PrintSupport, Svg, LinguistTools, Test), CMake.
**Structure**: `src/` (core: document, layout, editor, fountain, fdx; UI: mainwindow, scriptview, sceneboard, pagepainter, dialogs, theme, i18n, autosave), `resources/` (icons, QSS), `translations/` (pt_BR), `tests/` (core + QTest UI test), `examples/`.

For detailed architecture, see [docs/CODEBASE_MAP.md](docs/CODEBASE_MAP.md).

## Working notes

- Keep Qt out of the core files; core tests link only `owlio_core`.
- UI strings are English inside `tr()`. After changing them run `cmake --build build --target update_translations` and fill in `translations/owlio_pt_BR.ts`.
- New UI source files go into `UI_SOURCES` in `CMakeLists.txt`.
- In dialogs, call `setDefault(true)` on the primary button at the end of the constructor.
- After changing the `Editor` outside `ScriptView`, call `view_->refreshAfterExternalEdit()`.
- Run `./build/owlio_tests` and the UI test (`./build/owlio_gui_test`, with `~/Qt/6.8.3/gcc_64/lib` on `LD_LIBRARY_PATH`) before committing.
- Never commit straight to `main`: check the branch first and use a feature branch and PR.
