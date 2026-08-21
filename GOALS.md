# GOALS.md

Create `mdraft`: a fast, compiled Ghostwriter-style Markdown reader/writer
replacement for Linux-first desktop use.

## Specification Target

Follow GitHub Flavored Markdown as the primary Markdown compatibility target:

https://github.github.com/gfm/

## Technology Decision

Use C++ and Qt for the application shell.

The application must be compiled and optimized for startup speed, editing
latency, and broad Linux desktop compatibility.

## Core Product Goals

- Three-panel desktop layout.
- Left panel: native document index, outline, table of contents, and related
  navigation.
- Middle panel: the actual Markdown editor.
- Right panel: final rendered preview.
- The left and right panels can be hidden.
- Copy the practical menu coverage from Ghostwriter.
- Include font family and font size controls.
- Include light/dark mode toggle.
- Include document statistics.
- Show live word count and character count on the bottom status area.
- Add export engines for HTML and PDF where possible.
- Include LaTeX/Pandoc-style export paths where possible.

## Performance Goals

- The editor must open quickly into an immediately usable writing surface.
- Initial app launch must not create, initialize, hide, warm, or background-load
  a web preview engine.
- The rendered preview is load-on-open only.
- If the right preview panel is closed, the WebView must be destroyed rather
  than kept hidden in the background.
- The left index panel must use native Qt views and parsed Markdown structure,
  not a web-rendered view.
- Typing latency is sacred: editor input must not wait on preview rendering,
  export engines, document statistics, outline updates, spellcheck, or other
  secondary work.
- Preview, outline, statistics, and spellcheck updates must be debounced,
  throttled, incremental, or backgrounded as appropriate.
