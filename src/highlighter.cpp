#include "highlighter.h"
#include "theme.h"

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_darkMode(false)
{
    configureFormats();
}

void MarkdownHighlighter::setDarkMode(bool dark)
{
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    configureFormats();
    rehighlight();
}

void MarkdownHighlighter::configureFormats()
{
    m_codeblockFormatOld = {};
    m_codeFormat = {};
    m_emphasisFormat = {};
    m_boldFormat = {};
    m_linkFormat = {};
    m_headingFormat = {};

    const QColor codeBackground = m_darkMode ? QColor(Theme::CodeDarkBackground) : QColor(240, 240, 240);
    const QColor codeForeground = m_darkMode ? QColor(Theme::CodeDarkForeground) : QColor(136, 32, 0);
    const QColor emphasis = m_darkMode ? QColor(Theme::EmphasisDark) : QColor(164, 84, 0);
    const QColor accent = m_darkMode ? QColor(Theme::AccentBright) : QColor(0, 90, 150);

    // --- Code block (fenced) ---
    m_codeblockFormatOld.setBackground(codeBackground);
    m_codeblockFormatOld.setForeground(codeForeground);

    // --- Inline code ---
    m_codeFormat.setBackground(codeBackground);
    m_codeFormat.setForeground(codeForeground);

    // --- Emphasis / italic ---
    m_emphasisFormat.setFontItalic(true);
    m_emphasisFormat.setForeground(emphasis);

    // --- Bold ---
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(accent);

    // --- Link ---
    m_linkFormat.setForeground(accent);
    m_linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

    // --- Headings are handled in highlightBlock directly (line-based) ---

    m_headingFormat.setFontWeight(QFont::Bold);
    m_headingFormat.setForeground(accent);

    m_rules.clear();
    Rule rule;

    // Inline code: `...`
    rule.pattern = QRegularExpression(R"(`[^`]+`)");
    rule.format = m_codeFormat;
    m_rules.append(rule);

    // Italic: *...*
    rule.pattern = QRegularExpression(R"(\*[^*]+\*)");
    rule.format = m_emphasisFormat;
    m_rules.append(rule);

    // Bold follows italic so the more specific span wins where they overlap.
    rule.pattern = QRegularExpression(R"(\*\*[^*]+\*\*)");
    rule.format = m_boldFormat;
    m_rules.append(rule);

    // Links: [text](url)
    rule.pattern = QRegularExpression(R"(\[[^\]]*\]\([^)]*\))");
    rule.format = m_linkFormat;
    m_rules.append(rule);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    static const QRegularExpression fenceRe(R"(^ {0,3}(`{3,}|~{3,}))");
    const QRegularExpressionMatch fence = fenceRe.match(text);
    const int previousFence = previousBlockState();

    if (previousFence == 1 || previousFence == 2) {
        setCurrentBlockState(previousFence);
        if (fence.hasMatch()) {
            const QChar marker = fence.captured(1).at(0);
            if (((previousFence == 1 && marker == '`') || (previousFence == 2 && marker == '~'))
                && text.mid(fence.capturedEnd(1)).trimmed().isEmpty())
                setCurrentBlockState(0);
        }
        setFormat(0, static_cast<int>(text.length()), m_codeblockFormatOld);
        return;
    }

    if (fence.hasMatch()) {
        setCurrentBlockState(fence.captured(1).at(0) == '`' ? 1 : 2);
        setFormat(0, static_cast<int>(text.length()), m_codeblockFormatOld);
        return;
    }
    setCurrentBlockState(0);

    // Heading detection (line starts with #)
    static const QRegularExpression headingRe(R"(^(#{1,6})\s+(.*)$)");
    QRegularExpressionMatch m = headingRe.match(text);
    if (m.hasMatch()) {
        const int level = static_cast<int>(m.capturedLength(1));
        int size = qBound(9, 12 - level, 18);
        m_headingFormat.setFontPointSize(size);
        setFormat(0, static_cast<int>(text.length()), m_headingFormat);
        return;
    }

    // Apply the inline rules
    for (const Rule &r : m_rules) {
        auto it = r.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()), r.format);
        }
    }
}
