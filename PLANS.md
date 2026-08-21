# PLANS.md

## Approved Direction

Build `mdraft` as a compiled C++/Qt desktop Markdown editor with a fast
editor-first startup path and an optional, load-on-open rendered preview.

The application should be inspired by Ghostwriter's purpose and feature set,
but it must not copy Ghostwriter's always-available preview cost. The core
design principle is that editing must be available immediately, while heavier
rendered preview functionality is paid for only when the user opens it.

## Architecture

- Language: C++.
- UI framework: Qt.
- Primary platform: Linux desktop.
- Markdown target: GitHub Flavored Markdown.
- Main window: native Qt application shell with menu bar, panels, editor, and
  status area.
- Left panel: native Qt outline/index model generated from Markdown headings
  and document structure.
- Middle panel: native Markdown source editor.
- Right panel: optional rendered preview.
- Web preview: created only when the user opens the rendered preview panel and
  destroyed when that panel closes.
- Export engines: invoked on demand only.

## Panel Model

### Left Panel

The left panel is for document navigation. It should use native Qt widgets such
as tree/list views backed by a document outline model. It must not require a
web engine.

Expected contents may include:

- Heading outline.
- Table of contents.
- Document sections.
- File or project navigation later if needed.

### Middle Panel

The middle panel is the primary writing surface. It must be available as soon
as the app opens. All secondary systems must be designed around preserving
typing responsiveness.

Expected behavior:

- Markdown editing.
- Font family control.
- Font size control.
- Light/dark theme integration.
- Live word and character counts in the status area.

### Right Panel

The right panel is the rendered preview. It may use a web preview engine for
accurate HTML/CSS rendering, but the engine must be lazy-created.

Expected behavior:

- Not loaded during initial launch.
- Created only when preview opens.
- Updated on debounce rather than every keystroke.
- Destroyed when preview closes.

## Performance Strategy

- Treat typing latency as the highest priority.
- Avoid doing whole-document work on every keystroke.
- Debounce preview updates.
- Debounce or incrementally update outline and statistics.
- Keep export engines out of the live editing path.
- Keep WebView creation out of initial app startup.
- Use native widgets for navigation and editor UI wherever possible.

## Export Strategy

Export support should be engine-based and invoked only on demand.

Initial export targets:

- HTML.
- PDF.
- LaTeX/Pandoc-compatible paths where available.

The app may detect external tools such as Pandoc or LaTeX engines, but detection
must not block initial editor startup.

## Implementation Phases

1. Establish Qt/C++ project skeleton and application window.
2. Build the native editor-first shell with status area.
3. Add hideable left and right panel layout.
4. Add native left outline/index panel without web rendering.
5. Add live word and character count with safe update behavior.
6. Add theme toggle and font controls.
7. Add lazy right preview lifecycle.
8. Add Markdown rendering pipeline for preview.
9. Add export engine detection and on-demand export commands.
10. Add tests and startup/performance checks.
11. Write user-facing documentation after success criteria are met.

## Non-Negotiable Contract

The hard project contract is in `contracts/HARD_CONTRACT.md`. It must be read
before implementation work. Any implementation that creates or warms a WebView
on initial launch violates the project design.
