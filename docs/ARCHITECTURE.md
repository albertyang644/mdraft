# Architecture

## Panel model

`MainWindow` holds a `QSplitter` with three cells:

- **Left** — a `LeftPanel`, wrapped in a `ShutterPanel`. `LeftPanel` itself
  is a small segmented toggle (Outline / DIR) over a `QStackedWidget`:
  - **Outline** page: an `OutlineView` (`QTreeView`) backed by
    `OutlineModel`, a real `QAbstractItemModel` tree built from the
    document's heading levels (an H2 nests under the nearest preceding H1,
    an H3 under the nearest H2 or H1, etc.) — not a flat list.
  - **DIR** page: a `QListWidget` of sibling `.md` files in the open
    document's directory (built with `QDir`, no shelling out). Clicking an
    entry emits `fileActivated(path)`, which `MainWindow` turns into
    "open this file in a tab" via `openFileAt()`.
- **Middle** — a `QTabWidget`; each tab is one `MarkdownEditor`
  (`QPlainTextEdit` + `MarkdownHighlighter`). `MainWindow::m_editor` always
  points at the active tab's editor, so the rest of the class (menus, file
  ops, stats/outline/preview wiring) can keep treating "the editor" as a
  single object.
- **Right** — a `PreviewWidget`, wrapped in a `ShutterPanel`. See below.

`ShutterPanel` is a thin visibility wrapper: `setOpen(false)` hides the
whole panel (not just its content), so a closed panel takes zero space and
the splitter hands that space to the editor — there's no leftover
"collapsed bar" strip.

## Preview lifecycle (HARD_CONTRACT)

See [`contracts/HARD_CONTRACT.md`](../contracts/HARD_CONTRACT.md) for the
full rule; in short: no `QWebEngineView` exists until the user opens the
right panel, and it's destroyed when the panel closes. `PreviewWidget`
enforces this itself (`ensureWebView()` / `teardownPreview()`), independent
of whatever `MainWindow` does.

Rendering is: `PreviewWidget` shells out to `pandoc --from=gfm --to=html`
(async `QProcess`, never blocking the UI thread) to get body HTML, then
wraps it with `wrapMarkdownHtml()` (shared with `Exporter`'s HTML export,
in `markdown_html.cpp`) so the preview and exported files use the same
CSS. The last-rendered body HTML is cached so reopening the panel shows
content instantly instead of a blank-then-pop-in flash; the WebView itself
stays hidden until its first `loadFinished`, which avoids a first-paint
flash/flicker that's otherwise common with WebEngine on X11.

## Debouncing

Two independent debounce timers exist because they guard different costs:

- `MarkdownEditor` debounces its own `contentChanged` signal 180ms at the
  source, so stats/outline updates (cheap, regex-based) don't run on every
  keystroke but still feel instant.
- `MainWindow::m_previewDebounce` (250ms) sits between that and the actual
  preview refresh, because refreshing means spawning a `pandoc` process —
  worth debouncing separately from the cheap stuff above it.
- `MainWindow::m_autosaveDebounce` (2000ms) is separate again: autosave
  writes to disk, which should happen less eagerly than a preview refresh.

## Autosave

A tab with a file path is saved automatically ~2s after you stop typing
(`MainWindow::flushAutosave`), and immediately when you switch away from
it, close it, or quit the app — so a path-having tab is essentially never
more than a couple of seconds out of sync with disk. A tab with no path
(Untitled) has nowhere to autosave to; that's the one case that prompts
before discarding unsaved content (closing the tab or quitting the app).
The `*` suffix on a tab's label reflects `QTextDocument::isModified()`.

## Dark mode

The app-wide `QWidget { ... }` stylesheet doesn't reliably reach every
widget: `QAbstractScrollArea` subclasses (`QPlainTextEdit`, `QTreeView`,
`QListWidget`) paint their viewport separately and need their own rule,
and a few widgets (`LeftPanel`'s toggle bar, the top bar's file label) are
styled locally with higher-specificity selectors that win over the
app-wide cascade. Those are updated explicitly in `toggleDarkMode()` /
`LeftPanel::setDarkMode()` rather than relying on inheritance. The preview
panel's rendered HTML is a separate case again — it's not a Qt widget at
all, so `PreviewWidget::setDarkMode()` swaps its CSS instead.

## Export

`Exporter::exportTo()` shells out to `pandoc` for HTML, PDF
(`--pdf-engine=pdflatex`), and LaTeX. None of these tools are probed at
startup; detection failures only surface when you actually try to export
or open the preview, per the performance goals in `GOALS.md`.
