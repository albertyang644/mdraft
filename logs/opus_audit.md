# mdraft — Source Code Audit

Audit of the full `src/`, `tests/`, and build/packaging surface at commit
`425134e`. No code was changed. Findings are ordered by severity; each one
names the file and the concrete failure, not just a code smell.

Two findings were **empirically confirmed by running the app**, not just read
off the source — those are marked CONFIRMED with the evidence inline.

**Correction (2026-08-22):** the original audit overstated the HARD_CONTRACT.
`alwaysOpenWebview=true` caused `MainWindow` to construct a WebView during
startup. The Sol audit identified that violation correctly; it is included in
the resolution record below.

**Bottom line at audit time:** the architecture was generally sound, but there
was one reproducible crash, a startup-contract violation, and a cluster of
data-durability problems around autosave. C1 and H2-H4 required correction
before a public release.

---

## Critical

### C1 — Use-after-free: closing the last tab crashes the app (CONFIRMED)

**`src/mainwindow.cpp:338-378`**

Closing the only open tab reliably segfaults. Reproduced under gdb against
`build/mdraft` (this is also what is currently installed at
`~/.local/bin/mdraft`):

```
Thread 1 "mdraft" received signal SIGSEGV, Segmentation fault.
#0  QPlainTextEdit::document() const
#1  MainWindow::flushAutosave(MarkdownEditor*)
#2  MainWindow::onTabChanged(int)
#3  ...
#7  QTabWidget::currentChanged(int)
#10 QTabBar::insertTab(int, QIcon const&, QString const&)
#11 QTabWidget::insertTab(int, QWidget*, QIcon const&, QString const&)
```

Sequence:

1. `onTabCloseRequested()` calls `removeTab(index)`. With one tab left, the
   count drops to 0 and Qt emits `currentChanged(-1)`.
2. `onTabChanged(-1)` hits `if (index < 0) return;` — so `m_editor` is left
   pointing at the tab being removed.
3. `delete w` frees that editor. **`m_editor` is now dangling.**
4. `if (m_editorTabs->count() == 0) createEditorTab(...)` → `addTab()` →
   `currentChanged(0)` → `onTabChanged(0)` → `MarkdownEditor *previous =
   m_editor;` (freed) → `flushAutosave(previous)` → `ed->document()` on freed
   memory.

Repro: launch, click the ✕ on the only tab.

This is a **regression introduced in this session**, by me — the
`flushAutosave(previous)` call added to `onTabChanged()` for autosave-on-tab-
switch. Before that, `onTabChanged` never dereferenced `previous`, so the
dangling `m_editor` was latent and harmless. Worth stating plainly since the
commit message presents autosave as a safety improvement.

The root cause is broader than the one call: `m_editor` has no invariant
tying it to the tab widget's lifetime. It is a raw pointer written from three
places and dereferenced unguarded across ~15 methods.

---

## High

### H2 — Preview: double callback when pandoc dies abnormally (CONFIRMED UB)

**`src/preview_widget.cpp:99-122`**

`finishConversion` is wired to *both* `finished()` and `errorOccurred()`. When
a process **crashes** (as opposed to failing to start), Qt emits both. The
first call sets `m_pandocProcess = nullptr`; the second then calls a member
function on that null pointer:

- via `finishConversion`: `m_pandocProcess->deleteLater()`
- via the `finished` lambda: `m_pandocProcess->readAllStandardOutput()`

Confirmed by putting a `pandoc` on `PATH` that does `kill -SEGV $$` and opening
the preview:

```
QProcess: Destroyed while process ("pandoc") is still running.
QCoreApplication::postEvent: Unexpected null receiver
QCoreApplication::postEvent: Unexpected null receiver
```

On this Qt build it degrades to a warning rather than a crash, because
`deleteLater()` reaches `postEvent`, which null-checks. That is luck, not
safety: it is still a member call on `nullptr`, and if `finished()` happens to
arrive second on another Qt version/platform, the `readAllStandardOutput()`
path **does** dereference and will hard-crash.

Note these are the same warnings that appeared during `preview_lifecycle_test`
earlier today. The fix applied then (disconnecting in `teardownPreview`) only
closed the *teardown* path; this second path was left open.

### H3 — Autosave is not atomic and fails silently

**`src/mainwindow.cpp:393-407`**

`flushAutosave` opens the user's real file `WriteOnly` (which truncates it)
and streams the buffer into it, unattended, every ~2s of typing:

```cpp
QFile f(path);
if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
    return;                       // silent
QTextStream out(&f);
out << ed->toPlainText();
ed->document()->setModified(false);   // marked clean without verifying the write
```

Three distinct problems:

- **Truncate-then-write, no temp+rename.** A crash, OOM kill, or power loss
  between truncate and flush leaves the document truncated or empty. Autosave
  makes this *more* likely than manual saving, because it runs constantly and
  without the user watching. `QSaveFile` exists for exactly this.
- **No write-error check.** Disk full, quota exceeded, or a short write are not
  detected — and `setModified(false)` runs anyway, so the tab's `*` clears and
  the UI now claims the document is safely on disk when it is not.
- **Silent failure by design.** If the file is read-only or on a disconnected
  mount, autosave gives up with no indication whatsoever. The comment
  ("autosave shouldn't interrupt with a dialog") is reasonable, but the
  fallback should at least surface in the status bar and keep the tab dirty.

`saveToPath()` (mainwindow.cpp:702) and the HTML branch of
`Exporter::exportTo` (exporter.cpp:49-56) have the same unchecked-write issue,
though they at least run under user supervision.

### H4 — Autosave silently overwrites external modifications

**`src/mainwindow.cpp:393-407`**

No mtime or content check before writing. If the file changed on disk since it
was loaded, autosave clobbers it with no warning and no backup. This is not
theoretical for this app: the DIR panel actively encourages opening sibling
files, and multiple mdraft instances against the same tree are a normal
workflow (there were three running during this session). Two windows with the
same file open will now fight, last-writer-wins, every two seconds.

### H5 — PDF export always reports success

**`src/mainwindow.cpp:751-763`**

In the `MDRAFT_HAVE_WEBENGINE` branch — i.e. the default build — the return
value is discarded and success is reported unconditionally:

```cpp
QString dummyErr;
Exporter::exportTo(m_currentFile, outPath, "pdf", dummyErr);
statusBar()->showMessage(tr("PDF via Pandoc -> %1").arg(outPath), 3000);
```

If pandoc or `pdflatex` is missing (the common case — `pdflatex` is a heavy
optional dependency), the user is told the PDF was written and no file exists.
The non-WebEngine branch directly below handles this correctly, so the two
build configurations disagree about whether errors are reported.

The adjacent comment ("if that fails, copy raw text to `<pre>`") describes
behavior that was never implemented.

---

## Medium

### M1 — Preview executes arbitrary HTML/JS from the opened document

**`src/markdown_html.cpp`, `src/preview_widget.cpp:50`**

pandoc's gfm reader passes raw HTML straight through — verified:

```
$ printf '<script>alert(1)</script>\n<img src=x onerror="alert(2)">' | pandoc --from=gfm --to=html
<script>alert(1)</script>
<img src=x onerror="alert(2)">
```

That output is concatenated into `wrapMarkdownHtml()` and handed to
`QWebEngineView::setHtml()` with JavaScript enabled (the default). Opening an
untrusted `.md` — a downloaded README, a file from a shared drive — runs its
script in the preview pane, which can read the rendered document and beacon it
to a remote host via a subresource load. `LocalContentCanAccessRemoteUrls =
false` does not cover this, since the page is loaded via `setHtml` with an
`about:blank` base rather than as local content.

For a local editor this is moderate rather than urgent, but a Markdown *reader*
whose whole job is opening files you did not write should not be an arbitrary
JS execution surface. Cheapest fix: disable JS on the preview page
(`QWebEngineSettings::JavascriptEnabled, false`) — the preview needs none.
Stronger: `--from=gfm-raw_html`.

### M2 — Highlighter recompiles two regexes on every keystroke

**`src/highlighter.cpp:54, 71`**

```cpp
void MarkdownHighlighter::highlightBlock(const QString &text) {
    QRegularExpression headingRe(R"(^(#{1,6})\s+(.*)$)");   // constructed per call
    ...
    QRegularExpression fenceRe(R"(^\s*(```|~~~))");          // constructed per call
```

`highlightBlock` runs synchronously for every changed block on every
keystroke — this is the one true hot path in the app. The other four patterns
are correctly cached in `m_rules`; these two are rebuilt (and their PCRE
patterns recompiled) every call. `GOALS.md` says "typing latency is sacred";
this is the single clearest violation of it in the codebase, and it is a
one-line fix (`static const`).

### M3 — Syntax highlighting has no dark mode

**`src/highlighter.cpp:7-24`**

All formats are hardcoded light-theme values, and nothing calls back into the
highlighter when the theme flips. In dark mode the editor background is
`#1e1e1e` while inline code and code-fence lines are painted
`QColor(240,240,240)` — a near-white block with dark orange text. Given how
much of this session went into dark-mode polish, this is the most visible
remaining gap.

### M4 — `**bold**` is highlighted as italic

**`src/highlighter.cpp:36-43`**

Rules are applied in order, and `setFormat` overwrites rather than merges. The
bold rule `\*\*[^*]+\*\*` matches `**bold**` at offset 0; the italic rule
`\*[^*]+\*` then matches `*bold*` at offset 1 and repaints the interior. Bold
text renders as italic-orange with two stray blue asterisks.

### M5 — Fenced code blocks are ignored by both the outline and the highlighter

**`src/outline_model.cpp:19-38`, `src/highlighter.cpp:70-84`**

Neither tracks fence state. `QSyntaxHighlighter::setCurrentBlockState` /
`previousBlockState` exist for this and are unused. Consequences:

- A shell or Python block containing `# comment` lines injects fake entries
  into the document outline.
- Markdown inline rules are applied *inside* code blocks (`*ptr` in C renders
  italic).

The app's own seed document contains a fenced block, so this is trivially
reachable.

### M6 — Export reads the file on disk, and uses the wrong Markdown dialect

**`src/mainwindow.cpp:721-782`, `src/exporter.cpp:35-65`**

- All three exports pass `m_currentFile` to pandoc rather than the editor
  buffer, so they export whatever last reached disk. Autosave usually masks
  this, but when autosave silently fails (H3) the user exports stale content
  with no indication.
- The HTML path passes `--from=gfm`; **the PDF and LaTeX paths do not**, so
  they fall back to pandoc's default markdown dialect. GFM-specific constructs
  (tables, strikethrough, task lists) can render differently in the PDF than in
  the preview the user was just looking at. `GOALS.md` names GFM as *the*
  compatibility target.

### M7 — Export blocks the GUI thread for up to 60 seconds

**`src/exporter.cpp:12-22`**

`waitForStarted(3000)` + `waitForFinished(60000)` on the main thread. A PDF
export through pdflatex on a large document freezes the entire UI, with no
progress indication and no way to cancel. The preview path already does this
correctly with async `QProcess`; the exporter never got the same treatment.

### M8 — Opening a file from the command line leaves a stray "Untitled" tab

**`src/mainwindow.cpp:255-263, 463-473`**

The constructor seeds the first tab with the "Hello, mdraft" welcome text.
`openFileAt()` will only reuse that tab if it is *empty*:

```cpp
if (m_editorTabs->count() == 1 && filePathOfEditor(m_editor).isEmpty()
    && m_editor->toPlainText().isEmpty()) {
```

The seed text defeats the check, so every CLI/file-manager launch opens a
second tab and leaves the welcome tab behind. Visible in every screenshot taken
this session (`Untitled | nesttest.md`). The reuse branch is effectively dead
code.

### M9 — Desktop entry passes multiple files; only the first is opened

**`packaging/mdraft.desktop:7`, `src/main.cpp:41-42`**

`Exec=@BINDIR@/mdraft %F` — `%F` means *a list of files*. `main.cpp` reads
`positionalArguments().first()` and discards the rest. Selecting five `.md`
files and choosing "Open with mdraft" opens one. Since the app is now tabbed,
this is a natural fit that is silently dropped — either loop over the
positional args or downgrade the desktop entry to `%f`.

### M10 — Outline resets and re-expands on every edit

**`src/outline_model.cpp:10-66`, `src/outline_view.cpp:31-37`**

`setMarkdown` reparses the entire document and wraps it in
`beginResetModel`/`endResetModel`; `OutlineView` responds to `modelReset` with
`expandAll()`. Every debounced edit therefore discards selection and scroll
position in the outline. On a long document the outline jumps back to the top
while you type, which undercuts the panel's purpose as a navigation aid.

### M11 — Default build has no optimization

**`CMakeLists.txt`**

No `CMAKE_BUILD_TYPE` default is set, so the build command documented in the
README (`cmake -B build && cmake --build build`) compiles with an empty build
type — **no `-O` flags at all**. For a project whose stated reason to exist is
startup and typing performance, the default and documented path produces an
unoptimized binary. This also makes any future performance measurement
meaningless unless the type is set by hand.

### M12 — Test suite is disconnected and covers none of the risky code

**`tests/CMakeLists.txt`**

- `tests/` declares its own `project()` and is never referenced by the root
  `CMakeLists.txt`, so a normal build neither builds nor runs tests.
- No `enable_testing()` / `add_test()`, so `ctest` does nothing; the two
  binaries must be run by hand.
- Each test target recompiles source files directly (`../src/outline_model.cpp`
  …), so the source list must be manually mirrored. This already broke today —
  `markdown_html.cpp` had to be added by hand when `preview_widget.cpp` gained
  the dependency. A small `mdraft_core` static library would remove the whole
  class of breakage.
- **`MainWindow` is not compiled into any test binary.** Everything found in
  C1, H3, H4, H5, M6, and M8 lives in untested code. The 11 passing tests cover
  the outline model, the editor debounce, and the preview lifecycle — genuinely
  the right *contracts* to pin, but not where the bugs are.

---

## Low / hygiene

**Dead code**

- `Exporter::htmlToPdf` — never called; always returns false. Its comment
  claims WebEngine implements this path in MainWindow, which is untrue.
- `PreviewWidget::pdfFilePath()` / `m_lastPdfPath` — never assigned or read.
- `ShutterPanel::content()` and `m_side` — never used. Every call site passes a
  `side` argument the class ignores.
- `MarkdownEditor::updateStatsNow()` — declared, never defined or called.
- `MarkdownEditor::paintEvent` — overrides only to call the base implementation.
- `m_undoAction` … `m_selectAllAction` — assigned, never read; undo/redo are
  never enabled/disabled based on availability.
- `highlighter.cpp:60-61` — `QFont f; f.setPointSize(...)` is computed and
  discarded; `setFontWeight(Bold)` is called twice.
- Unused includes: `QIcon` (outline_model.cpp), `QPrinter`/`QTemporaryFile`
  (preview_widget.cpp), `QWebEngineProfile`/`QWebEngineSettings` (main.cpp).

**Correctness / robustness**

- `OutlineModel::index/parent/rowCount` call `.at()` on an index derived from
  `internalId()` with no bounds check, while `data()` and `blockNumberForIndex`
  do check. In release builds `QVector::at()` is unchecked — a stale index
  would read out of bounds.
- `PreviewWidget` has no timeout on the pandoc process. If pandoc hangs,
  `m_pandocProcess` stays non-null forever and the preview silently stops
  updating for the rest of the session.
- `teardownPreview()` calls `waitForFinished(200)` on the GUI thread.
- `m_autosaveTarget` holds only one pending tab; scheduling a second tab
  discards the first tab's pending autosave. Currently masked by the flush on
  tab-switch/close/quit, but the single slot contradicts the comment above it
  ("whichever tab was typed in, active or not").
- `main.cpp:47-53` — if the file cannot be opened, the app silently shows an
  empty editor with no message.
- Tab de-duplication in `openFileAt` compares raw path strings, so the same
  file opened by relative and absolute path yields two tabs.
- Outline: no setext (`Title\n====`) support and ATX closing sequences
  (`## Title ##`) are not stripped — both are valid GFM.
- DIR view lists only `*.md`, but the Open dialog accepts `*.markdown`; a
  `.markdown` file is invisible in the panel that is supposed to list siblings.
- DIR view has no `QFileSystemWatcher`; files created externally do not appear
  until you switch tabs or toggle the panel.
- `ShutterPanel::setOpen(false)` in the constructor emits `closed()` before
  `MainWindow` has connected to it. Harmless only because `teardownPreview()`
  on a never-created view is a no-op.
- Dragging the splitter to zero width leaves `isOpen()` reporting `true`, so
  Ctrl+1 appears to do nothing on the first press.
- `toggleDarkMode()` relies on `m_darkMode` being updated *before*
  `m_modeToggle->setChecked()` to avoid infinite recursion through the
  `toggled` handler. Correct today, but the guard is positional.
- Restoring dark mode at startup calls `toggleDarkMode()`, which writes the
  same value straight back to `QSettings`.
- Exported HTML has no `<!DOCTYPE html>` → browsers render it in quirks mode.
- `ToggleSwitch` paints no focus indicator and sets no accessible name; it is
  keyboard-operable but gives no visual feedback that it is focused.
- Without WebEngine, opening the preview panel leaves the placeholder reading
  "Preview panel is closed. Open it to render." while it is open.

**Maintainability**

- Theme colors are hardcoded in four files (`mainwindow.cpp`,
  `left_panel.cpp`, `markdown_html.cpp`, `toggle_switch.cpp`) and have already
  drifted: `#232323` (top bar / toggle bar) vs `#2b2b2b` (general widgets) vs
  `#1e1e1e` (editor / preview body). A single palette header would have
  prevented the dark-mode misses fixed today, and would prevent M3.
- Per-tab file paths are stored as stringly-typed dynamic QObject properties
  (`setProperty("mdraftFilePath", …)`), with no compile-time checking.
- `mainwindow.cpp` is 882 lines and owns tabs, menus, theming, file I/O,
  autosave, export, and formatting. The tab/document model is the natural
  thing to split out first, and would make C1's invariant enforceable.
- `QRegularExpression`, `QFile`, and `QTextStream` are used in
  `mainwindow.cpp`/`main.cpp` without direct includes, working only via
  transitive includes.
- The build emits a wall of deprecation warnings from the old
  `QMenu::addAction(text, receiver, slot, shortcut)` overloads; no
  `-Wall -Wextra` is configured, so real warnings would be lost in the noise.
- Include-guard typo: `HIGHIGHTER_H` in `src/highlighter.h`.
- `CMAKE_AUTOUIC` is on with no `.ui` files.
- The `.desktop` file registers `text/plain`, so mdraft offers itself as a
  handler for every plain-text file on the system (`.txt`, `.log`, `.conf`).
- Window geometry is hardcoded to 1280×820 and not persisted, though other
  UI state (dark mode, panel mode) now is.
- Two install paths exist that can drift: `install(TARGETS …)` in CMake
  (binary only) and `packaging/install-user.sh` (binary + icons + desktop
  entry).

---

## What holds up well

Worth recording, since an audit that only lists defects misrepresents the
codebase:

- **The widget-level lazy lifecycle is sound, but the original HARD_CONTRACT
  claim was wrong.** `PreviewWidget` owns the WebView lifecycle itself, but
  `MainWindow` could still open it during construction through the persisted
  `alwaysOpenWebview` setting. That caller-level violation was fixed and is now
  covered at the `MainWindow` level.
- **The debounce layering is deliberate and correct** — 180 ms at the editor
  for cheap work, 250 ms before spawning pandoc, 2000 ms before touching disk.
  Each tier matches its cost.
- **The preview render path is properly async** and handles the
  reentrancy/coalescing case (`m_conversionPending`) rather than piling up
  processes.
- **`OutlineModel` is a correct `QAbstractItemModel` tree**, including the
  fiddly `parent()`/`rowInParent` bookkeeping that is easy to get wrong.
- **No external tool is probed at startup**, honoring the stated performance
  goal; pandoc and pdflatex are invoked strictly on demand.
- Comments consistently explain *why* rather than restating the code — the
  notes on WebEngine first-paint flicker, on stylesheet specificity, and on
  construction ordering are the kind that prevent future regressions.

---

## Suggested order of work

1. **C1** — the installed binary crashes on a routine action. Fix the
   `m_editor` lifetime invariant, not just the one dereference.
2. **H3 / H4** — make autosave atomic (`QSaveFile`), check the write result,
   keep the tab dirty and surface a status-bar message on failure, and refuse
   to overwrite a file that changed underneath.
3. **H2 / H5** — guard the double-callback; stop reporting PDF success
   unconditionally.
4. **M2 / M11** — two nearly free performance fixes that the project's own
   goals call for.
5. **M12** — get `MainWindow` into a test binary and wire CTest up before
   fixing the rest, so the fixes land with regression cover.
6. **M1** — disable JavaScript in the preview before this is used on
   documents from other people.

---

*Note: an untracked `sol_audit.md` is present in the working tree. It is not
mine and I left it untouched.*

## Resolution — 2026-08-22

All findings in this audit and `sol_audit.md` were resolved together.

### Crashes, lifecycle, and contracts

- **C1:** the active editor is now a guarded `QPointer`; the `-1` tab state is
  handled explicitly, and close-last-tab has a `MainWindow` regression test.
- **H2:** preview completion is tied to the exact `QProcess`, duplicate terminal
  signals are ignored, a timeout was added, and the crashed-pandoc path is
  tested with a self-SIGSEGV helper.
- The persisted always-open behavior was removed. Startup creates neither a
  `QWebEngineView` nor a profile; this was checked with a constructor breakpoint
  and a `MainWindow` contract test.

### File safety and identity

- **H3/H4:** file persistence moved into `DocumentFile`. It uses `QSaveFile`,
  checks stream and commit results, fingerprints the last disk contents, and
  refuses to overwrite external changes. Failed named saves keep the tab dirty
  and block tab/application close.
- Paths are absolute/canonical where possible, so aliases and symlinks cannot
  open competing tabs. Save As also rejects a target already open elsewhere.
- Autosave state is per editor rather than a single shared target.
- A real `RLIMIT_FSIZE` failure verifies that the original file survives and
  the native `EFBIG` diagnostic is retained instead of becoming "Writing
  canceled by application."

### Preview and export

- **H5/M6/M7:** exports capture the current editor buffer, always select GFM,
  run asynchronously, time out, propagate converter failures, and publish
  output atomically. PDF/LaTeX use temporary destinations before replacement;
  HTML is standalone and includes a doctype.
- **M1:** JavaScript is disabled. The ineffective `gfm-raw_html` reader flag
  was removed, and the lazily created WebView now uses a dedicated profile with
  a request interceptor that blocks non-local schemes. A live localhost
  listener test proves raw HTML cannot load a remote subresource.
- The GFM-to-LaTeX regression test now uses bare `www.example.com`, which
  discriminates GFM (`\\href`) from Pandoc's default Markdown. The former pipe
  table assertion was removed because it passed under both readers.
- Preview source coalescing now guarantees that the newest buffer wins even
  after a quick A-to-B-to-A edit.

### Editor, outline, UI, and build

- **M2-M5:** highlighter expressions are cached, theme-aware colors are shared,
  bold/italic precedence is correct, and fenced blocks maintain block state.
  The outline skips fences, supports setext and closing ATX syntax, checks
  indices, and avoids a model reset when headings did not change.
- **M8/M9:** the seed document was removed so CLI files reuse the initial blank
  tab, and every `%F` positional file is opened.
- **M10-M12:** unchanged outlines retain view state; default single-config
  builds are Release; tests are integrated through CTest and a shared
  `mdraft_core`, including `MainWindow`, file, exporter, preview, and core tests.
- The audited hygiene items were cleaned: dead APIs and unused state were
  removed, theme constants centralized, directory watching and `.markdown`
  visibility added, splitter/toggle/accessibility behavior corrected, window
  geometry persisted, warnings/includes cleaned, desktop MIME scope narrowed,
  and CMake/install-script installation unified.

### Verification

- WebEngine-enabled build: 5/5 CTest executables passed.
- WebEngine-disabled build: 5/5 passed.
- ASan/UBSan build: 5/5 passed.
- Preview, exporter, and durability tests passed three consecutive runs.
- Empirical repros were repeated: close-last-tab exited normally, startup did
  not hit the WebView constructor breakpoint, crashed Pandoc completed once,
  remote raw-HTML content made no TCP connection, and `EFBIG` preserved both
  the original file and useful error text.
