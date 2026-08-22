#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QSplitter;
class QLabel;
class QPushButton;
class QPlainTextEdit;
class QTabWidget;
class MarkdownEditor;
class OutlineModel;
class OutlineView;
class PreviewWidget;
class ShutterPanel;
class QTimer;
class LeftPanel;
class ToggleSwitch;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public:
    // Programmatically open a file (used by the CLI, and by clicking a file
    // in the left panel's DIR view). Reuses an already-open tab if the file
    // is already open; otherwise opens it in a new tab.
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
    void createTopBar();
    void applyTopBarTheme();
    void createStatusBar();
    void refreshOutline();
    void updateStats();
    void updatePreview();

    void saveToPath(const QString &path);
    void setCurrentFile(const QString &path);
    QString currentMarkdown() const;

    // Tabs: each tab owns one MarkdownEditor. m_editor always points at the
    // currently active tab's editor, so the rest of the class (menus, file
    // ops, stats/outline/preview) can keep treating "m_editor" as if there
    // were a single document, unchanged.
    MarkdownEditor *createEditorTab(const QString &path, const QString &content);
    void onTabChanged(int index);
    void onTabCloseRequested(int index);
    void syncActiveTabUi();
    QString filePathOfEditor(MarkdownEditor *ed) const;
    void setFilePathOfEditor(MarkdownEditor *ed, const QString &path);

    QSplitter *m_splitter;
    ShutterPanel *m_leftShutter;
    ShutterPanel *m_rightShutter;
    LeftPanel *m_leftPanel;
    OutlineModel *m_outlineModel;
    QTabWidget *m_editorTabs;
    MarkdownEditor *m_editor; // == active tab's editor
    PreviewWidget *m_preview;

    QLabel *m_wordLabel;
    QLabel *m_charLabel;
    QLabel *m_fileLabel;
    ToggleSwitch *m_modeToggle;

    QWidget *m_topBar;
    QPushButton *m_leftToggleBtn;
    QPushButton *m_rightToggleBtn;
    QLabel *m_topFileLabel;
    QAction *m_alwaysOpenPreviewAction;

    QTimer *m_previewDebounce;

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
