# TASKS.md

- [x] Decide primary implementation direction: C++ + Qt.
- [x] Decide compiled desktop app as the performance baseline.
- [x] Decide load-on-open-only semantics for the rendered WebView preview.
- [x] Record hard WebView startup contract.
- [x] Choose Qt major version and required modules. — Qt6 (Widgets, Gui, Core; optional WebEngineWidgets)
- [x] Choose Markdown parser/renderer for GitHub Flavored Markdown. — pandoc (`--from=gfm`), shelled out on demand
- [x] Create C++/Qt project skeleton.
- [x] Create main application window.
- [x] Add three-panel layout.
- [x] Add hide/show behavior for left index panel. — full collapse, not just a thin bar
- [x] Add hide/show behavior for right preview panel.
- [x] Implement native left outline/index panel. — nested tree matching heading structure, plus a DIR view of sibling .md files
- [x] Implement central Markdown editor. — now a tabbed multi-document editor
- [x] Add bottom status area. — word/char counts, open file's directory, light/dark switch
- [x] Add live word count.
- [x] Add live character count.
- [x] Add font family control.
- [x] Add font size control.
- [x] Add light/dark mode toggle. — persists across restarts
- [x] Add Ghostwriter-inspired menu structure.
- [x] Implement lazy preview creation when right panel opens.
- [x] Implement preview destruction when right panel closes.
- [x] Verify no WebView is created on initial launch. — covered by `preview_lifecycle_test.cpp`
- [x] Debounce rendered preview updates. — 250ms debounce before shelling out to pandoc
- [x] Debounce or safely background outline/statistics updates. — editor's own contentChanged is debounced 180ms at the source
- [x] Add HTML export engine. — standalone styled document, shares CSS with the live preview
- [x] Add PDF export engine.
- [x] Add LaTeX/Pandoc-compatible export path where available.
- [x] Ensure export engine detection does not block initial editor startup. — pandoc/pdflatex are only invoked on demand, never probed at startup
- [x] Add startup/performance tests or checks. — MainWindow construction is timed and asserts no WebView exists, even with the removed legacy preference set
- [x] Add editor responsiveness tests or checks. — `editorStatsSignalDebounced` in `core_smoke_test.cpp`
- [x] Add documentation after success criteria are met. — `README.md` (overview, build/run, dependencies) and `docs/ARCHITECTURE.md`

## Beyond the original scope

- [x] Tabbed multi-document editor.
- [x] DIR view: browse and open sibling .md files from the left panel.
- [x] Atomic autosave (debounced, path-having tabs only) with external-change
  detection and a `*` dirty indicator; close/quit refuses failed named saves
  and confirms discarding dirty untitled documents.
- [x] Light/Dark mode persists across restarts (not just the toggle itself).
- [x] "Open with" from a file manager (positional CLI argument).
- [x] DIR listing follows the directory via `QFileSystemWatcher`, plus an
  explicit refresh button for filesystems where watching silently does nothing.
- [x] Scroll lock between the editor and the rendered preview, driven from an
  isolated script world so page JavaScript stays disabled.
- [x] External-change reconciliation with Notepad++ semantics: watch open
  files, offer to reload when one changes underneath you, and treat declining
  as "keep mine" so the document can still be saved and closed. `F5` reloads
  on demand; re-opening an already-open file re-reads it.
