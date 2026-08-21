# HARD_CONTRACT.md

This file defines non-negotiable architecture rules for `mdraft`.

## No WebView On Initial Load

The application must not create, initialize, hide, warm, preload, or otherwise
start a WebView or browser-backed rendered preview during initial app launch.

The app must open first as a fast native Markdown editor.

## Preview Lifecycle

- The rendered preview is load-on-open only.
- The WebView may be created only when the user opens the right rendered preview
  panel or explicitly requests a rendered preview feature.
- Closing the rendered preview panel must destroy the WebView.
- A hidden rendered preview panel must not keep a WebView alive in the
  background.
- No background WebView process may be kept alive solely to make future preview
  opening faster.

## Left Panel Rule

The left index/outline panel must not use a WebView. It must be implemented
with native Qt views backed by parsed Markdown structure.

## Typing Latency Rule

Typing in the editor must not wait on:

- rendered preview updates
- WebView startup
- export engines
- document statistics
- outline/index refresh
- spellcheck
- external tool detection

Secondary work must be debounced, throttled, incremental, asynchronous, or
deferred so the editor remains responsive.
