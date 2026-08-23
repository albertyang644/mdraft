#include "theme.h"

QString Theme::darkApplicationStyleSheet()
{
    return QStringLiteral(
        "QWidget { background-color:#2b2b2b; color:#e0e0e0; }"
        "QMenuBar { background-color:#333; }"
        "QMenuBar::item:selected { background-color:#444; }"
        "QMenu { background-color:#2b2b2b; color:#e0e0e0; }"
        "QStatusBar { background-color:#222; }"
        "QTreeView,QListWidget { background-color:#2b2b2b; color:#e0e0e0; }"
        "QPlainTextEdit { background-color:#1e1e1e; color:#e0e0e0; }"
        "QTabWidget::pane { background-color:#2b2b2b; border-color:#444; }"
        "QTabBar::tab { background-color:#333; color:#e0e0e0; padding:4px 10px; }"
        "QTabBar::tab:selected { background-color:#1e1e1e; }");
}

QString Theme::markdownStyleSheet(bool dark)
{
    if (dark) {
        return QStringLiteral(
            "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;"
            "max-width:840px;margin:24px auto;padding:0 16px;line-height:1.55;color:#e0e0e0;background:#1e1e1e;}"
            "h1,h2,h3,h4{line-height:1.25;margin-top:1.4em;color:#f0f0f0;}"
            "h1{border-bottom:1px solid #3a3a3a;padding-bottom:.3em;}"
            "h2{border-bottom:1px solid #333;padding-bottom:.2em;}"
            "code{background:#2b2b2b;padding:.15em .4em;border-radius:3px;"
            "font-family:Menlo,Consolas,monospace;font-size:.9em;color:#e0e0e0;}"
            "pre{background:#252525;padding:12px;border-radius:6px;overflow-x:auto;}"
            "pre code{background:none;padding:0;}"
            "blockquote{border-left:4px solid #444;margin:0;padding:0 1em;color:#aaa;}"
            "table{border-collapse:collapse;}"
            "th,td{border:1px solid #444;padding:6px 10px;}"
            "img{max-width:100%;}"
            "a{color:#6fb2ff;}");
    }
    return QStringLiteral(
        "body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;"
        "max-width:840px;margin:24px auto;padding:0 16px;line-height:1.55;color:#222;background:#fff;}"
        "h1,h2,h3,h4{line-height:1.25;margin-top:1.4em;}"
        "h1{border-bottom:1px solid #ddd;padding-bottom:.3em;}"
        "h2{border-bottom:1px solid #eee;padding-bottom:.2em;}"
        "code{background:#f2f2f2;padding:.15em .4em;border-radius:3px;"
        "font-family:Menlo,Consolas,monospace;font-size:.9em;}"
        "pre{background:#f6f8fa;padding:12px;border-radius:6px;overflow-x:auto;}"
        "pre code{background:none;padding:0;}"
        "blockquote{border-left:4px solid #ddd;margin:0;padding:0 1em;color:#555;}"
        "table{border-collapse:collapse;}"
        "th,td{border:1px solid #ddd;padding:6px 10px;}"
        "img{max-width:100%;}"
        "a{color:#2a6ebb;}");
}
