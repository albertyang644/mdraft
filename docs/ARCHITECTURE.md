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
    "open this file in a tab" via `openFileAt()`. The listing follows a
    `QFileSystemWatcher` on the directory, plus an explicit refresh button
    shown only in DIR mode — the watcher silently reports nothing on
    network mounts and once the inotify watch limit is exhausted, so the
    manual path has to exist.
- **Middle** — a `QTabWidget`; each tab is one `MarkdownEditor`
  (`QPlainTextEdit` + `MarkdownHighlighter`). A guarded active-editor pointer
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

The lazily created WebView uses a dedicated `QWebEngineProfile`. JavaScript
is disabled, and a profile-level request interceptor permits only local
`about`, `data`, `file`, and `qrc` URLs. This keeps raw HTML compatible with
GFM while preventing a document from loading remote subresources when its
preview opens.

## Debouncing

Two independent debounce timers exist because they guard different costs:

- `MarkdownEditor` debounces its own `contentChanged` signal 180ms at the
  source, so stats/outline updates (cheap, regex-based) don't run on every
  keystroke but still feel instant.
- `MainWindow::m_previewDebounce` (250ms) sits between that and the actual
  preview refresh, because refreshing means spawning a `pandoc` process —
  worth debouncing separately from the cheap stuff above it.
- Each editor owns a 2000ms autosave timer, so edits in one tab cannot replace
  another tab's pending save.

## Autosave

A tab with a file path is saved automatically ~2s after you stop typing
(`MainWindow::flushAutosave`), and immediately when you switch away from
it, close it, or quit the app — so a path-having tab is essentially never
more than a couple of seconds out of sync with disk. A tab with no path
(Untitled) has nowhere to autosave to; that's the one case that prompts
before discarding unsaved content (closing the tab or quitting the app).
The `*` suffix on a tab's label reflects `QTextDocument::isModified()`.

Each tab has `DocumentFile` state. Saves use `QSaveFile` temp-and-commit
replacement, retain a fingerprint of the last known disk contents, and refuse
to overwrite external changes. A failed save keeps the tab dirty and prevents
tab/application close.

## External changes (Notepad++ semantics)

Open documents are watched with a `QFileSystemWatcher`. When one changes
underneath you, `MainWindow::promptReload()` asks whether to reload it:

- **Yes** reloads from disk and restores your caret line, so a reload does
  not also lose your place.
- **No** keeps your buffer, marks it modified, and calls
  `DocumentFile::acceptDiskState()` to re-baseline the fingerprint.

That re-baseline is load-bearing, not a detail. The external-change check
refuses saves while a conflict stands, and every close path routes through a
save — so without a way to resolve the conflict, a dirty-and-conflicted
document could not be saved, its tab could not be closed, and the application
could not be quit. Declining the reload is the escape hatch; `F5` /
**File → Reload from Disk** is the other direction.

Two details the watcher forces:

- Our own saves must not look like external edits. `QSaveFile` commits by
  rename, which both fires the watcher and drops the inotify watch, so every
  successful save re-baselines the fingerprint and re-arms the watch. A
  notification whose fingerprint still matches is ours and is ignored.
- Re-opening an already-open file (clicking it again in DIR) re-reads it
  rather than silently reusing a stale buffer.

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

`Exporter::exportMarkdown()` asynchronously streams the current editor buffer
to `pandoc` for HTML, PDF (`--pdf-engine=pdflatex`), and LaTeX. Every format
explicitly uses GFM input. None of these tools are probed at startup;
detection failures only surface when you actually try to export or open the
preview, per the performance goals in `GOALS.md`.
