#include "editor.h"
#include "highlighter.h"

#include <QTextBlock>
#include <QTimer>
#include <QFontDatabase>

MarkdownEditor::MarkdownEditor(QWidget *parent)
    : QPlainTextEdit(parent), m_changeTimer(nullptr), m_highlighter(nullptr)
{
    // Monospaced, readable default font
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPointSize(11);
    setFont(mono);
    // Map the default tab width to 4 spaces
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);

    // Markdown syntax highlighting (native, cheap)
    m_highlighter = new MarkdownHighlighter(document());

    // Debounce the content-changed signal so secondary work
    // never runs on the typing hot path.
    m_changeTimer = new QTimer(this);
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(180);
    connect(m_changeTimer, &QTimer::timeout, this, &MarkdownEditor::contentChanged);
    connect(this, &QPlainTextEdit::textChanged, this, [this]() {
        m_changeTimer->start();
    });

    setLineWrapMode(QPlainTextEdit::WidgetWidth);
}

void MarkdownEditor::setDarkMode(bool dark)
{
    m_highlighter->setDarkMode(dark);
}

void MarkdownEditor::goToLine(int blockNumber)
{
    if (blockNumber < 0 || blockNumber >= document()->blockCount())
        return;
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    QTextCursor cursor(block);
    setTextCursor(cursor);
    centerCursor();
    setFocus();
}
