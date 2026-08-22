# mdraft

A fast, native Markdown editor for Linux desktops, built with C++ and Qt6.
Editing starts instantly; the rendered HTML preview only exists while you
actually have it open.

## Features

- **Tabbed editor** — multiple documents open at once, each with syntax
  highlighting, live word/character counts, and font controls.
- **Native outline panel** — a real nested tree of the document's headings
  (H2 nests under the nearest H1, etc.), not a flat list. Click an entry to
  jump to it.
- **DIR view** — toggle the left panel from Outline to a listing of sibling
  `.md` files in the open document's directory; click one to open it in a
  new tab.
- **Rendered preview** — opens on demand (Ctrl+3), split 50/50 with the
  editor. Closing the panel destroys the preview's WebView; nothing about
  it exists at launch or while it's collapsed (see
  [`contracts/HARD_CONTRACT.md`](contracts/HARD_CONTRACT.md)).
- **Autosave** — a tab with a file path is saved a couple of seconds after
  you stop typing. Untitled documents can't be autosaved anywhere, so
  closing one with unsaved content is the one case that asks first. A `*`
  after the tab name means there are unsaved changes.
- **Export** — HTML, PDF, and LaTeX via [pandoc](https://pandoc.org/).
  Exported HTML is a standalone, styled document (the same CSS the live
  preview uses), not a bare fragment.
- **Light/Dark mode** — a sliding switch in the status bar; remembers
  whichever you last picked, across restarts.

## Dependencies

- Qt6 (Widgets, Gui, Core; WebEngineWidgets optional — preview/PDF-via-
  WebEngine are disabled at build time if it's not found)
- CMake ≥ 3.16, a C++17 compiler
- [`pandoc`](https://pandoc.org/) on `PATH` at runtime, for the live
  preview and HTML/PDF/LaTeX export
- `pdflatex` (e.g. from a TeX distribution) on `PATH` at runtime, only if
  you use PDF export

None of these are probed at startup — pandoc/pdflatex are only invoked when
you actually open the preview or export, so a missing tool never slows down
or blocks editing.

## Building

```sh
cmake -B build
cmake --build build
./build/mdraft
```

## Tests

```sh
cmake -B tests/build -S tests
cmake --build tests/build
./tests/build/core_smoke_test
./tests/build/preview_lifecycle_test
```

## Installing (Linux, per-user)

```sh
packaging/install-user.sh
```

Installs the binary to `~/.local/bin/mdraft` and a desktop entry so it
shows up in your application launcher and "Open with" menus. Reads and
writes settings via `~/.config/mdraft/mdraft.conf`.

## Command line

```sh
mdraft [file]
mdraft --file <path>
```

Both forms open the given file in a tab. The bare positional form is what
file managers actually invoke for "Open with".

## Project layout

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for how the pieces fit
together, and [`contracts/HARD_CONTRACT.md`](contracts/HARD_CONTRACT.md)
for the non-negotiable preview-lifecycle rule this app is built around.
