<p align="center">
  <img src="resources/owlio.png" width="128" alt="Owlio logo">
</p>

<h1 align="center">Owlio</h1>

<p align="center">
  A screenwriting app written in C++17 and Qt 6, with industry-standard formatting,<br>
  real pages on screen, and your scenes laid out as index cards.
</p>

<p align="center">
  <em><a href="README.pt-BR.md">Leia em português</a></em>
</p>

<p align="center">
  <img src="docs/screenshots/script-dark.png" width="49%" alt="Writing view, dark theme">
  <img src="docs/screenshots/cards-light.png" width="49%" alt="Index cards view, light theme">
</p>

---

## Why Owlio?

Professional screenplays follow a strict format: 12 pt Courier, fixed margins, and one page standing in for roughly one minute of screen time. Owlio puts that format first. It does not reflow text the way a word processor does. It lays the script out on a fixed grid of 10 characters and 6 lines per inch, so the page you see while writing is the same page that ends up in the PDF.

It also follows the keyboard-first flow screenwriters know from tools like Final Draft. You press **Enter** and **Tab**, and Owlio picks the next element for you: scene heading, action, character, dialogue.

## Features

### Writing
- **Six screenplay elements**: Scene Heading, Action, Character, Parenthetical, Dialogue, Transition.
- **Smart element flow**: Enter after a character name starts dialogue, Enter after dialogue starts the next character, and Tab turns an empty action line into a character cue. Ctrl+1 to Ctrl+6 pick an element directly.
- **Autocomplete**: suggests character names (most frequent first), locations already used, times of day, extensions such as (V.O.) and (O.S.), and common transitions. Accept with Tab, Enter or →.
- Parentheticals get their parentheses automatically, and full Unicode support covers accented input.

### Pages and pagination
- US Letter pages drawn on screen with exact industry margins (1.5" left, 1" elsewhere).
- Professional page breaks: scene headings and character cues never sit alone at the bottom of a page, and dialogue split across pages gets **(MORE)** and **CHARACTER (CONT'D)**. The Portuguese forms (MAIS)/(CONT.) are also available.
- Page numbers, optional scene numbers in both margins, and a title page.

### Index cards
- Every scene becomes a card showing its number, INT/EXT, location, time of day (color-coded), starting page, the characters who speak in it, and its length in eighths of a page.
- **Drag and drop cards to reorder scenes.** The whole scene moves in the script, and Ctrl+Z undoes the move. Alt+arrow keys move cards from the keyboard.
- Write a **synopsis** for each scene with F2. When a scene has no synopsis, the card shows the opening of its action.
- Double-click a card to jump to that scene in the script.

### Files
- **Fountain** (`.fountain`) is the native format: plain text, open, and readable by many other screenwriting tools.
- **Final Draft** (`.fdx`) import and export, including scene summaries.
- **PDF export** with embedded fonts and a title page, plus printing.

### Interface
- Light and dark themes that follow the system automatically.
- **Focus mode** dims everything except the speech or paragraph you are writing.
- Side panels list scenes (with page and cast) and characters (with line counts).
- Find and replace, unlimited undo and redo, and zoom.

<p align="center">
  <img src="docs/screenshots/cards-dark.png" width="49%" alt="Index cards, dark theme">
  <img src="docs/screenshots/pdf-page.png" width="35%" alt="Exported PDF page">
</p>

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| **Enter** | Next element (Scene Heading → Action, Character → Dialogue, Dialogue → Character, …) |
| **Tab / Shift+Tab** | Cycle the element type. At the end of a speech, Tab inserts a parenthetical |
| **Ctrl+1 … Ctrl+6** | Scene Heading, Action, Character, Parenthetical, Dialogue, Transition |
| **Tab / Enter / →** | Accept the grey autocomplete suggestion |
| **Ctrl+Shift+C / Ctrl+Shift+R** | Index cards / script view |
| **Ctrl+Shift+F** | Focus mode |
| **Ctrl+Shift+E** | Export PDF |
| **Ctrl+F** | Find and replace |
| **F2** (cards) | Edit the selected scene's synopsis |
| **Alt+arrows** (cards) | Move the selected card |
| **Ctrl+mouse wheel** | Zoom pages / resize cards |

## Building

Requirements: CMake 3.16 or newer, a C++17 compiler, and **Qt 6** (Widgets, PrintSupport, Svg, plus Test for the UI test).

### With Qt from your distribution

```bash
# Debian / Ubuntu
sudo apt install cmake g++ qt6-base-dev qt6-svg-dev libxcb-cursor0

cmake -S . -B build
cmake --build build -j
./build/owlio
```

### With Qt from aqtinstall (no root needed)

```bash
uvx --from aqtinstall aqt install-qt linux desktop 6.8.3 linux_gcc_64 --outputdir ~/Qt
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64
cmake --build build -j
./run.sh exemplos/a_ultima_sessao.fountain
```

`run.sh` points the dynamic loader at `~/Qt`. Qt 6 on X11 also needs `libxcb-cursor0`. If you cannot install it system-wide, extract the package into `~/Qt/extra` with `apt-get download libxcb-cursor0` and `dpkg -x`, and `run.sh` will find it there.

### Command line

```bash
./run.sh script.fountain                          # open a script
./run.sh script.fountain --cartoes                # open straight into the index cards
./run.sh script.fountain --export-pdf script.pdf  # export without opening a window
./run.sh --tema escuro                            # force the dark theme (claro = light)
```

## Tests

```bash
./build/owlio_tests      # core: Fountain/FDX round-trips, pagination, editing, scene reordering
./run.sh --help
LD_LIBRARY_PATH=$HOME/Qt/6.8.3/gcc_64/lib ./build/owlio_gui_test   # drives the real window with keyboard and mouse events
```

## Project layout

| Path | Purpose |
|---|---|
| `src/document.*` | Data model (paragraphs, elements, scene synopses), UTF-8/UTF-32 helpers |
| `src/layout.*` | Line wrapping and pagination on the 10 cpi × 6 lpi grid, with (MORE)/(CONT'D) |
| `src/editor.*` | Cursor, selection, Tab/Enter rules, autocomplete, undo, find, scene moves |
| `src/fountain.*`, `src/fdx.*` | Reading and writing Fountain and Final Draft files |
| `src/pagepainter.*` | Draws pages to screen, PDF or printer with exact glyph positions |
| `src/scriptview.*` | The page editor widget (keyboard, mouse, input methods, scrolling) |
| `src/sceneboard.*` | The index cards board (grid, drag and drop) |
| `src/mainwindow.*` | Menus, toolbar, side panels, dialogs |
| `src/theme.*`, `resources/themes/estilo.qss` | Light/dark palettes, the Qt style sheet, recolored SVG icons |
| `tests/` | Core unit tests and a QTest-based UI test |
| `exemplos/` | A short sample screenplay |

The core (`document`, `layout`, `editor`, `fountain`, `fdx`) has no Qt dependency.

To tweak the look without recompiling, run with `OWLIO_QSS=resources/themes/estilo.qss ./run.sh`. The style sheet reloads every time you save it.

## Status

Owlio is young. The interface is currently in **Brazilian Portuguese**, and the page size is US Letter. On the roadmap:

- English interface (Qt translations)
- A4 pages
- Dual dialogue
- Revision mode with colored pages
- Reports (scenes, characters, locations)
- Autosave and backups
