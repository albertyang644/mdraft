#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class QTimer;
class MarkdownHighlighter;

class MarkdownEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget *parent = nullptr);

    // Jump to a line (used by outline navigation)
    void goToLine(int line);
    void setDarkMode(bool dark);

signals:
    // Emitted (debounced) when the user modifies the document.
    void contentChanged();

private:
    QTimer *m_changeTimer;
    MarkdownHighlighter *m_highlighter;
};

#endif // EDITOR_H
