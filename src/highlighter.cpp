#include "highlighter.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // --- Code block (fenced) ---
    m_codeblockFormatOld.setBackground(QColor(240, 240, 240));
    m_codeblockFormatOld.setForeground(QColor(136, 32, 0));

    // --- Inline code ---
    m_codeFormat.setBackground(QColor(240, 240, 240));
    m_codeFormat.setForeground(QColor(136, 32, 0));

    // --- Emphasis / italic ---
    m_emphasisFormat.setFontItalic(true);
    m_emphasisFormat.setForeground(QColor(164, 84, 0));

    // --- Bold ---
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(QColor(0, 90, 150));

    // --- Link ---
    m_linkFormat.setForeground(QColor(0, 90, 150));
    m_linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    // --- Headings are handled in highlightBlock directly (line-based) ---

    Rule rule;

    // Inline code: `...`
    rule.pattern = QRegularExpression(R"(`[^`]+`)");
    rule.format = m_codeFormat;
    m_rules.append(rule);

    // Bold: **...**
    rule.pattern = QRegularExpression(R"(\*\*[^*]+\*\*)");
    rule.format = m_boldFormat;
    m_rules.append(rule);

    // Italic: *...*
    rule.pattern = QRegularExpression(R"(\*[^*]+\*)");
    rule.format = m_emphasisFormat;
    m_rules.append(rule);

    // Links: [text](url)
    rule.pattern = QRegularExpression(R"(\[[^\]]*\]\([^)]*\))");
    rule.format = m_linkFormat;
    m_rules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    // Heading detection (line starts with #)
    QRegularExpression headingRe(R"(^(#{1,6})\s+(.*)$)");
    QRegularExpressionMatch m = headingRe.match(text);
    if (m.hasMatch()) {
        m_headingFormat.setFontWeight(QFont::Bold);
        // Level-based size hint
        int level = m.captured(1).length();
        QFont f;
        f.setPointSize(10 + (4 - level)); // crude size scale
        m_headingFormat.setFontWeight(QFont::Bold);
        int size = qBound(9, 12 - level, 18);
        m_headingFormat.setFontPointSize(size);
        m_headingFormat.setForeground(QColor(0, 90, 150));
        setFormat(0, text.length(), m_headingFormat);
        return;
    }

    // Fenced code block detection (``` or ~~~~)
    QRegularExpression fenceRe(R"(^\s*(```|~~~))");
    if (fenceRe.match(text).hasMatch()) {
        setFormat(0, text.length(), m_codeblockFormatOld);
        return;
    }

    // Apply the inline rules
    for (const Rule &r : m_rules) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), r.format);
        }
    }
}
