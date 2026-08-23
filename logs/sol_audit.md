# Code Audit

Audit date: 2026-08-22

Scope: current repository source, build configuration, contracts, and tests. No implementation changes were made.

## Findings

### 1. High: failed autosaves do not stop tabs or the application from closing

`MainWindow::flushAutosave()` returns `void` and silently returns when the destination cannot be opened (`src/mainwindow.cpp:393-407`). Both tab close and application close call it and then continue destroying the editor or accepting the close event (`src/mainwindow.cpp:353-378`, `src/mainwindow.cpp:409-433`).

This loses edits whenever a path-backed document becomes unwritable, its directory disappears, a removable/network volume disconnects, or another I/O failure occurs. The UI only prompts for untitled documents, so a failed save of a named document gives the user no warning and no chance to recover the buffer.

Recommendation: make saving return a checked result; refuse close and show an error when a dirty buffer cannot be persisted.

### 2. High: saves are non-atomic and write failures are marked as successful

Both autosave and explicit save open the real document with `QIODevice::WriteOnly` and stream directly into it (`src/mainwindow.cpp:400-406`, `src/mainwindow.cpp:702-715`). Opening this way truncates the existing file before the replacement content is safely written. Neither path checks `QTextStream::status()`, `QFile::error()`, flush, or close results, yet both clear `document()->isModified()` and present the document as saved.

A disk-full, quota, device, or short-write failure can therefore leave a partial/empty file while the only in-memory copy is marked clean. Combined with finding 1, closing afterward can silently discard the recoverable editor contents.

Recommendation: write through `QSaveFile`, check stream status, and only commit/clear the modified flag after every operation succeeds.

### 3. High: the persisted preview preference violates the no-WebView-at-launch contract

During `MainWindow` construction, `alwaysOpenWebview=true` opens the right shutter (`src/mainwindow.cpp:239-245`). Its `opened` handler immediately calls `PreviewWidget::openPreview()` (`src/mainwindow.cpp:194-208`), which constructs a `QWebEngineView` (`src/preview_widget.cpp:44-71`). This happens as part of initial window construction, before the event loop starts.

That directly conflicts with `contracts/HARD_CONTRACT.md`, which says a WebView must never be created or initialized during initial launch. It also means users who once enabled the setting permanently pay WebEngine startup cost on later launches.

Recommendation: remove the launch-time preference or reinterpret it as an after-launch explicit action without weakening the documented contract.

### 4. Medium: WebEngine builds report PDF export success even when export fails

The `MDRAFT_HAVE_WEBENGINE` branch calls `Exporter::exportTo()` but discards its return value and error, then unconditionally displays `PDF via Pandoc` success (`src/mainwindow.cpp:751-757`). Missing `pandoc`/`pdflatex`, permission failures, invalid output paths, and conversion errors are all reported as successful even if no PDF exists.

The branch does not actually use WebEngine despite its comment; it invokes the same Pandoc path as the non-WebEngine branch.

Recommendation: handle the return/error exactly as the non-WebEngine branch does, or implement the claimed WebEngine print path.

### 5. Medium: export can use stale on-disk content and freezes the entire UI

All export actions pass `m_currentFile` to Pandoc without first requiring a successful save of the active buffer (`src/mainwindow.cpp:721-782`). Autosave is delayed by the editor's 180 ms content debounce plus a 2 second autosave timer (`src/editor.cpp:22-30`, `src/mainwindow.cpp:318-329`). An export initiated before that write, or after any silent autosave failure, reads older disk content rather than what the editor shows.

Separately, `Exporter::runProcess()` uses blocking `waitForStarted()`/`waitForFinished()` calls for up to 60 seconds on the GUI thread (`src/exporter.cpp:8-32`). The window cannot repaint, accept typing, or cancel during that interval, contrary to the typing-latency contract.

Recommendation: export an explicitly captured current buffer (or require a checked save), and run Pandoc asynchronously with cancellation/progress handling.

### 6. Medium: preview conversion can settle on stale Markdown after a quick revert

`PreviewWidget::setMarkdown()` only queues conversion when the new text differs from `m_lastRenderedSource` (`src/preview_widget.cpp:75-83`). Consider rendered source A, an in-flight conversion for B, then an edit back to A before B completes. The final `setMarkdown(A)` does not set `m_conversionPending`, because A equals the old rendered source. When B finishes it replaces the preview, and no follow-up conversion restores A (`src/preview_widget.cpp:86-109`). The preview remains inconsistent with the editor until another edit or reopen.

Recommendation: compare incoming text with the in-flight source as well, and always schedule the latest pending source when an in-flight result is no longer current.

### 7. Medium: file identity is raw-string based, allowing two tabs to overwrite one file

Duplicate detection compares path strings verbatim (`src/mainwindow.cpp:451-460`). A relative CLI path, absolute DIR-view path, symlink, `..` path, or Save As onto an already-open file can therefore create two editors for the same underlying file. Their independent autosaves then overwrite each other, potentially replacing newer content with stale content merely by switching or closing tabs.

Recommendation: normalize file identity using an absolute canonical path where available, and reject/merge Save As targets already open in another tab.

### 8. Low: the outline treats headings inside fenced code blocks as document headings

`OutlineModel::setMarkdown()` scans each line independently for leading `#` and does not track fenced-code state (`src/outline_model.cpp:10-38`). Markdown such as a shell snippet containing `# comment` inside triple backticks appears in the outline and jumps to a non-heading line.

Recommendation: track backtick/tilde fence state during the scan, or use the same Markdown parser semantics as rendering.

## Verification and coverage

- A clean out-of-tree build with GCC warnings enabled succeeded with WebEngine disabled.
- `core_smoke_test`: 5 passed, 0 failed.
- `preview_lifecycle_test`: 6 passed, 0 failed with WebEngine available.
- Compiler diagnostics were limited to member initialization order and deprecated Qt `QMenu::addAction` overloads; these are not included as behavioral findings.
- Existing tests cover outline nesting, editor debounce/navigation, and the basic preview/shutter lifecycle. They do not cover save failures, short writes, close-after-failure, export errors/staleness, path aliasing, the launch preference, or preview conversion races. Those are the main regression-test gaps.

## Resolution — 2026-08-22

All eight findings were resolved, together with the overlapping Opus audit.

1. Failed saves now return checked results, keep the document dirty, surface an
   error, and prevent tab or application close.
2. `DocumentFile` writes through `QSaveFile`, validates the stream and commit,
   fingerprints disk contents, and refuses external-change overwrites. Tests
   cover atomic replacement, an unwritable destination, external edits, and a
   real `EFBIG` mid-write failure.
3. The persisted always-open WebView behavior was removed. The preview remains
   lazy, and startup is covered at `MainWindow` level and by an empirical
   `QWebEngineView` constructor breakpoint check.
4. PDF, LaTeX, and HTML exports share one checked asynchronous path. Converter
   or publication failures are reported instead of showing false success.
5. Export captures the current buffer, never depends on autosave timing, uses
   GFM for every format, has a timeout, and does not block the event loop.
6. Preview conversion tracks pending and in-flight source independently, so a
   quick A-to-B-to-A edit cannot leave B rendered.
7. Paths are normalized and canonicalized for duplicate detection and Save As,
   preventing aliases or symlinks from creating competing document tabs.
8. The outline tracks fenced blocks and excludes code comments from headings;
   it also gained setext/closing-ATX support and stale-index bounds checks.

The final security verification found that `--from=gfm-raw_html` did not
actually disable raw HTML. That flag was removed. JavaScript remains disabled,
and the on-demand WebEngine profile now installs an allowlist request
interceptor that blocks remote subresources. A live localhost listener test
confirms an HTML `<img src="http://...">` cannot phone home.

The original GFM LaTeX test was also replaced: a pipe table was vacuous because
Pandoc's default reader emitted the same `longtable`. The new bare-URL case
requires GFM's `\\href{http://www.example.com}{www.example.com}` output.

Verification completed with 5/5 CTest executables passing in WebEngine-enabled,
WebEngine-disabled, and ASan/UBSan builds. Targeted preview, exporter, and
durability tests passed three consecutive runs; `git diff --check` was clean.
