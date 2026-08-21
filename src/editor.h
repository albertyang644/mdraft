#ifndef EDITOR_H
#define EDITOR_H

#include <QPlainTextEdit>

class QTimer;

class MarkdownEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget *parent = nullptr);

    // Jump to a line (used by outline navigation)
    void goToLine(int line);

signals:
    // Emitted (debounced) when the user modifies the document.
    void contentChanged();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void updateStatsNow();

    QTimer *m_changeTimer;
};

#endif // EDITOR_H
