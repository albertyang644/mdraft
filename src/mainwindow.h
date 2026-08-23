#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QPointer>
#include <QSet>

#include "document_file.h"

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
class QFileSystemWatcher;
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

protected:
    void closeEvent(QCloseEvent *event) override;

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
    void applyStatusBarTheme();
    void applyDarkMode(bool dark, bool persist);
    void createStatusBar();
    void refreshOutline();
    void updateStats();
    void updatePreview();
    void updateEditActions();
    void exportDocument(const QString &format, const QString &title,
                        const QString &suffix, const QString &filter);

    bool saveToPath(const QString &path);
    bool saveEditorToPath(MarkdownEditor *editor, const QString &path,
                          bool reportError, bool checkExternalChanges);
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
    MarkdownEditor *editorForPath(const QString &path) const;

    // Autosave: a few seconds after you stop typing, a tab that already has
    // a file path is saved silently. Tabs with no path (Untitled) can't be
    // autosaved anywhere, so they prompt before being discarded. Named tabs
    // also refuse to close if their checked save fails.
    void updateTabModifiedIndicator(MarkdownEditor *ed);
    bool flushAutosave(MarkdownEditor *ed, bool reportError = false);

    // External-change handling (Notepad++ semantics): open documents are
    // watched, and when one changes underneath you the editor offers to
    // reload it. Declining keeps your buffer and re-baselines, so your next
    // save is allowed to overwrite rather than being refused forever.
    void watchDocument(MarkdownEditor *ed);
    void unwatchDocument(MarkdownEditor *ed);
    void onWatchedFileChanged(const QString &path);
    void promptReload(MarkdownEditor *ed);
    bool reloadEditorFromDisk(MarkdownEditor *ed);
    void reloadFromDisk(); // F5

    QSplitter *m_splitter;
    ShutterPanel *m_leftShutter;
    ShutterPanel *m_rightShutter;
    LeftPanel *m_leftPanel;
    OutlineModel *m_outlineModel;
    QTabWidget *m_editorTabs;
    QPointer<MarkdownEditor> m_editor; // == active tab's editor
    QHash<MarkdownEditor *, DocumentFile> m_documents;
    PreviewWidget *m_preview;

    QLabel *m_wordLabel;
    QLabel *m_charLabel;
    QLabel *m_fileLabel;
    QLabel *m_sunLabel;
    QLabel *m_moonLabel;
    ToggleSwitch *m_modeToggle;

    QWidget *m_topBar;
    QPushButton *m_leftToggleBtn;
    QPushButton *m_rightToggleBtn;
    QLabel *m_topFileLabel;
    QTimer *m_previewDebounce;
    QFileSystemWatcher *m_docWatcher;
    QSet<MarkdownEditor *> m_reloadPromptOpen;

    QString m_currentFile;
    bool m_darkMode;

    QAction *m_undoAction;
    QAction *m_redoAction;
    QAction *m_cutAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_selectAllAction;

    friend class MainWindowTest;
};

#endif // MAINWINDOW_H
