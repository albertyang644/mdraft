#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSplitter;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class MarkdownEditor;
class OutlineModel;
class OutlineView;
class PreviewWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public:
    // Programmatically open a file (used by the CLI).
    void openFileAt(const QString &path, const QString &content);

private slots:
    // File
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void exportHtml();
    void exportPdf();
    void exportLatex();
    // View
    void toggleLeftPanel();
    void toggleRightPanel();
    void toggleDarkMode();
    // Tools / format
    void increaseFont();
    void decreaseFont();
    void chooseFont();
    void boldSelection();
    void italicSelection();
    void insertHeading(int level);

private:
    void createMenus();
    void createStatusBar();
    void refreshOutline();
    void updateStats();
    void updatePreview();

    void saveToPath(const QString &path);
    void setCurrentFile(const QString &path);
    QString currentMarkdown() const;
    void setEditorText(const QString &text);

    QSplitter *m_splitter;
    OutlineView *m_outlineView;
    OutlineModel *m_outlineModel;
    MarkdownEditor *m_editor;
    PreviewWidget *m_preview;

    QLabel *m_wordLabel;
    QLabel *m_charLabel;
    QLabel *m_fileLabel;
    QPushButton *m_modeToggle;

    QString m_currentFile;
    bool m_darkMode;

    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_cutAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_selectAllAction;
};

#endif // MAINWINDOW_H
