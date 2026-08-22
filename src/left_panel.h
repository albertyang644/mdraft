#ifndef LEFT_PANEL_H
#define LEFT_PANEL_H

#include <QWidget>

class QPushButton;
class QStackedWidget;
class QListWidget;
class QListWidgetItem;
class OutlineView;
class OutlineModel;

// Left sidebar content: a small segmented toggle switches between the
// document outline and a listing of sibling Markdown files next to the
// currently open document. Clicking a file in that listing opens it.
class LeftPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LeftPanel(QWidget *parent = nullptr);

    OutlineView *outlineView() const { return m_outlineView; }
    void setOutlineModel(OutlineModel *model);

    // Called whenever the open file changes so the DIR view knows which
    // directory to list; pass an empty path for an unsaved document.
    void setCurrentFilePath(const QString &path);

    // The Outline/DIR toggle bar is styled locally (an ID-selector
    // stylesheet on the bar + explicit per-button stylesheets), which wins
    // over the app-wide dark-mode stylesheet's generic QWidget rule — so it
    // needs to be told about theme changes explicitly rather than relying
    // on the cascade.
    void setDarkMode(bool dark);

signals:
    void goToBlock(int blockNumber);
    void fileActivated(const QString &absolutePath);

private:
    void showOutline();
    void showDirListing();
    void refreshDirListing();
    void applyToggleBarStyle();

    QWidget *m_toggleBar;
    QPushButton *m_outlineBtn;
    QPushButton *m_dirBtn;
    QStackedWidget *m_stack;
    OutlineView *m_outlineView;
    QListWidget *m_dirView;
    QString m_currentDir;
    bool m_dark;
};

#endif // LEFT_PANEL_H
