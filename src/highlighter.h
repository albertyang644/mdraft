#ifndef HIGHIGHTER_H
#define HIGHIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit MarkdownHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<Rule> m_rules;
    QTextCharFormat m_headingFormat;
    QTextCharFormat m_emphasisFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_codeblockFormatOld;
};

#endif // HIGHIGHTER_H
