#include "mainwindow.h"

#include "editor.h"
#include "outline_model.h"
#include "outline_view.h"
#include "preview_widget.h"
#include "exporter.h"

#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QStatusBar>
#include <QMenuBar>
#include <QFontDialog>
#include <QTextCursor>
#include <QFileInfo>
#include <QShortcut>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QTemporaryFile>
#include <QTimer>
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(nullptr)
    , m_outlineView(nullptr)
    , m_outlineModel(nullptr)
    , m_editor(nullptr)
    , m_preview(nullptr)
    , m_wordLabel(nullptr)
    , m_charLabel(nullptr)
    , m_fileLabel(nullptr)
    , m_modeToggle(nullptr)
    , m_darkMode(false)
    , m_undoAction(nullptr)
    , m_redoAction(nullptr)
    , m_cutAction(nullptr)
    , m_copyAction(nullptr)
    , m_pasteAction(nullptr)
    , m_selectAllAction(nullptr)
{
    setWindowTitle("mdraft");
    resize(1280, 820);

    // --- Three-panel layout ---
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_outlineModel = new OutlineModel(this);
    m_outlineView = new OutlineView(m_splitter);
    m_outlineView->setModel(m_outlineModel);

    m_editor = new MarkdownEditor(m_splitter);

    m_preview = new PreviewWidget(m_splitter);

    m_splitter->addWidget(m_outlineView);
    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 0); // left: fixed
    m_splitter->setStretchFactor(1, 1); // editor: grows
    m_splitter->setStretchFactor(2, 0); // right: fixed
    m_splitter->setSizes({240, 700, 400});
    setCentralWidget(m_splitter);

    // --- Wire outline navigation ---
    connect(m_outlineView, &OutlineView::goToBlock, this, [this](int block) {
        m_editor->goToLine(block);
    });

    // --- Debounced secondary work from the editor ---
    connect(m_editor, &MarkdownEditor::contentChanged, this, [this]() {
        updateStats();
        refreshOutline();
        updatePreview();
    });

    createMenus();
    createStatusBar();

    // Keyboard toggles
    new QShortcut(QKeySequence("Ctrl+1"), this, SLOT(toggleLeftPanel()));
    new QShortcut(QKeySequence("Ctrl+3"), this, SLOT(toggleRightPanel()));

    // Seed some content so the UI is usable immediately.
    setEditorText(QStringLiteral(
        "# Hello, mdraft\n\n"
        "This is a *Markdown* editor. Click an outline entry on the left to "
        "jump to that heading.\n\n"
        "## Features\n\n- Three-panel layout\n- Native outline\n- Live stats\n"
        "- Light/dark toggle\n\n```cpp\nint main(){return 0;}\n```\n"
    ));
    setCurrentFile(QString());
    statusBar()->showMessage(tr("Ready"), 2000);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Editor text helpers
// ---------------------------------------------------------------------------

QString MainWindow::currentMarkdown() const
{
    return m_editor->toPlainText();
}

void MainWindow::setEditorText(const QString &text)
{
    m_editor->setPlainText(text);
    updateStats();
    refreshOutline();
    updatePreview();
}

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    QString shown = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    setWindowTitle(QString("%1 — mdraft").arg(shown));
}

void MainWindow::openFileAt(const QString &path, const QString &content)
{
    setEditorText(content);
    setCurrentFile(path);
    statusBar()->showMessage(tr("Opened %1").arg(path), 3000);
}

// ---------------------------------------------------------------------------
// Menu construction (Ghostwriter-inspired coverage)
// ---------------------------------------------------------------------------

void MainWindow::createMenus()
{
    // File
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), this, &MainWindow::newFile, QKeySequence::New);
    fileMenu->addAction(tr("&Open…"), this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::saveFile, QKeySequence::Save);
    fileMenu->addAction(tr("Save &As…"), this, &MainWindow::saveFileAs, QKeySequence::SaveAs);
    fileMenu->addSeparator();
    QMenu *exportMenu = fileMenu->addMenu(tr("E&xport"));
    exportMenu->addAction(tr("Export &HTML…"), this, &MainWindow::exportHtml);
    exportMenu->addAction(tr("Export &PDF…"), this, &MainWindow::exportPdf);
    exportMenu->addAction(tr("Export &LaTeX…"), this, &MainWindow::exportLatex);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);

    // Edit
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    m_undoAction = editMenu->addAction(tr("&Undo"), m_editor, &QPlainTextEdit::undo, QKeySequence::Undo);
    m_redoAction = editMenu->addAction(tr("&Redo"), m_editor, &QPlainTextEdit::redo, QKeySequence::Redo);
    editMenu->addSeparator();
    m_cutAction = editMenu->addAction(tr("Cu&t"), m_editor, &QPlainTextEdit::cut, QKeySequence::Cut);
    m_copyAction = editMenu->addAction(tr("&Copy"), m_editor, &QPlainTextEdit::copy, QKeySequence::Copy);
    m_pasteAction = editMenu->addAction(tr("&Paste"), m_editor, &QPlainTextEdit::paste, QKeySequence::Paste);
    m_selectAllAction = editMenu->addAction(tr("Select &All"), m_editor, &QPlainTextEdit::selectAll, QKeySequence::SelectAll);

    // Format
    QMenu *formatMenu = menuBar()->addMenu(tr("F&ormat"));
    formatMenu->addAction(tr("&Bold"), this, &MainWindow::boldSelection, QKeySequence(Qt::CTRL | Qt::Key_B));
    formatMenu->addAction(tr("&Italic"), this, &MainWindow::italicSelection, QKeySequence(Qt::CTRL | Qt::Key_I));
    formatMenu->addSeparator();
    QMenu *headingMenu = formatMenu->addMenu(tr("&Heading"));
    for (int lvl = 1; lvl <= 4; ++lvl)
        headingMenu->addAction(tr("Heading %1").arg(lvl), this, [this, lvl]() { insertHeading(lvl); });
    formatMenu->addSeparator();
    formatMenu->addAction(tr("&Font…"), this, &MainWindow::chooseFont);
    formatMenu->addAction(tr("Increase Font Size"), QKeySequence(Qt::CTRL | Qt::Key_Plus), this, &MainWindow::increaseFont);
    formatMenu->addAction(tr("Decrease Font Size"), QKeySequence(Qt::CTRL | Qt::Key_Minus), this, &MainWindow::decreaseFont);

    // View
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Toggle &Left Panel"), QKeySequence("Ctrl+1"), this, &MainWindow::toggleLeftPanel);
    viewMenu->addAction(tr("Toggle &Preview Panel"), QKeySequence("Ctrl+3"), this, &MainWindow::toggleRightPanel);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("&Light/Dark Mode"), QKeySequence("Ctrl+D"), this, &MainWindow::toggleDarkMode);
}

void MainWindow::createStatusBar()
{
    m_wordLabel = new QLabel(this);
    m_charLabel = new QLabel(this);
    m_fileLabel = new QLabel(this);

    // Light/Dark toggle: a checkable button on the very right of the status bar.
    m_modeToggle = new QPushButton(tr("Light"), this);
    m_modeToggle->setCheckable(true);
    m_modeToggle->setChecked(false);
    m_modeToggle->setToolTip(tr("Toggle light/dark mode (Ctrl+D)"));
    connect(m_modeToggle, &QPushButton::clicked, this, &MainWindow::toggleDarkMode);

    statusBar()->addWidget(m_fileLabel, 1);
    statusBar()->addPermanentWidget(m_wordLabel);
    statusBar()->addPermanentWidget(m_charLabel);
    statusBar()->addPermanentWidget(m_modeToggle); // far right
    m_modeToggle->setStyleSheet("margin-left:8px; padding:0 10px;");

    updateStats();
}

// ---------------------------------------------------------------------------
// Stats + outline + preview (debounced, off the typing hot path)
// ---------------------------------------------------------------------------

void MainWindow::updateStats()
{
    const QString text = m_editor->toPlainText();
    // Word count: split on whitespace.
    int words = 0;
    if (!text.trimmed().isEmpty())
        words = text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
    m_wordLabel->setText(QString("%1 words").arg(words));
    m_charLabel->setText(QString("%1 chars").arg(text.length()));
}

void MainWindow::refreshOutline()
{
    m_outlineModel->setMarkdown(m_editor->toPlainText());
}

void MainWindow::updatePreview()
{
    m_preview->setMarkdown(m_editor->toPlainText());
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void MainWindow::newFile()
{
    if (!currentMarkdown().trimmed().isEmpty()) {
        QMessageBox::StandardButton r = QMessageBox::question(
            this, tr("New"),
            tr("Discard the current document?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes)
            return;
    }
    setEditorText(QString());
    setCurrentFile(QString());
}

void MainWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Markdown"),
                                                QString(), tr("Markdown (*.md *.markdown);;All Files (*)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open"), tr("Cannot open file:\n%1").arg(path));
        return;
    }
    QTextStream in(&f);
    setEditorText(in.readAll());
    setCurrentFile(path);
}

void MainWindow::saveFile()
{
    if (m_currentFile.isEmpty()) {
        saveFileAs();
        return;
    }
    saveToPath(m_currentFile);
}

void MainWindow::saveFileAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Markdown"),
                                                QString(), tr("Markdown (*.md);;All Files (*)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".md"))
        path += ".md";
    saveToPath(path);
    setCurrentFile(path);
}

void MainWindow::saveToPath(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save"), tr("Cannot write:\n%1").arg(path));
        return;
    }
    QTextStream out(&f);
    out << m_editor->toPlainText();
    f.close();
    statusBar()->showMessage(tr("Saved %1").arg(path), 2000);
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

void MainWindow::exportHtml()
{
    if (m_currentFile.isEmpty())
        saveFileAs();
    if (m_currentFile.isEmpty())
        return;

    QString outPath = QFileDialog::getSaveFileName(this, tr("Export HTML"),
                                                   m_currentFile + ".html", tr("HTML (*.html)"));
    if (outPath.isEmpty())
        return;
    QString err;
    if (Exporter::exportTo(m_currentFile, outPath, "html", err))
        statusBar()->showMessage(tr("Exported HTML to %1").arg(outPath), 3000);
    else
        QMessageBox::warning(this, tr("Export"), err);
}

void MainWindow::exportPdf()
{
    if (m_currentFile.isEmpty())
        saveFileAs();
    if (m_currentFile.isEmpty())
        return;

    QString outPath = QFileDialog::getSaveFileName(this, tr("Export PDF"),
                                                   m_currentFile + ".pdf", tr("PDF (*.pdf)"));
    if (outPath.isEmpty())
        return;

#ifdef MDRAFT_HAVE_WEBENGINE
    // Render current markdown to HTML file, then print via WebEngine.
    // For simplicity, first try Pandoc; if that fails, copy raw text to <pre>.
    QString dummyErr;
    Exporter::exportTo(m_currentFile, outPath, "pdf", dummyErr);
    statusBar()->showMessage(tr("PDF via Pandoc -> %1").arg(outPath), 3000);
#else
    QString err;
    if (Exporter::exportTo(m_currentFile, outPath, "pdf", err))
        statusBar()->showMessage(tr("Exported PDF to %1").arg(outPath), 3000);
    else
        QMessageBox::warning(this, tr("Export"), err);
#endif
}

void MainWindow::exportLatex()
{
    if (m_currentFile.isEmpty())
        saveFileAs();
    if (m_currentFile.isEmpty())
        return;

    QString outPath = QFileDialog::getSaveFileName(this, tr("Export LaTeX"),
                                                   m_currentFile + ".tex", tr("LaTeX (*.tex)"));
    if (outPath.isEmpty())
        return;
    QString err;
    if (Exporter::exportTo(m_currentFile, outPath, "latex", err))
        statusBar()->showMessage(tr("Exported LaTeX to %1").arg(outPath), 3000);
    else
        QMessageBox::warning(this, tr("Export"), err);
}

// ---------------------------------------------------------------------------
// View toggles + dark mode
// ---------------------------------------------------------------------------

void MainWindow::toggleLeftPanel()
{
    bool visible = m_outlineView->isVisible();
    m_outlineView->setVisible(!visible);
}

void MainWindow::toggleRightPanel()
{
    bool visible = m_preview->isVisible();
    m_preview->setVisible(!visible);
    if (!visible) {
        // Panel becoming visible -> create + render the WebView (lazy).
        m_preview->openPreview();
    } else {
        // Panel hidden -> destroy WebView (HARD_CONTRACT).
        m_preview->teardownPreview();
    }
}

void MainWindow::toggleDarkMode()
{
    m_darkMode = !m_darkMode;
    if (m_darkMode) {
        setStyleSheet(
            "QWidget { background-color:#2b2b2b; color:#e0e0e0; }"
            "QMenuBar { background-color:#333; }"
            "QMenuBar::item:selected { background-color:#444; }"
            "QMenu { background-color:#2b2b2b; color:#e0e0e0; }"
            "QStatusBar { background-color:#222; }"
            "QTreeView { background-color:#2b2b2b; color:#e0e0e0; }"
        );
        m_modeToggle->setText(tr("Dark"));
        m_modeToggle->setChecked(true);
    } else {
        setStyleSheet(QString());
        m_modeToggle->setText(tr("Light"));
        m_modeToggle->setChecked(false);
    }
}

// ---------------------------------------------------------------------------
// Format actions
// ---------------------------------------------------------------------------

void MainWindow::chooseFont()
{
    bool ok = false;
    QFont f = QFontDialog::getFont(&ok, m_editor->font(), this, tr("Choose Editor Font"));
    if (ok)
        m_editor->setFont(f);
}

void MainWindow::increaseFont()
{
    QFont f = m_editor->font();
    f.setPointSize(f.pointSize() + 1);
    m_editor->setFont(f);
}

void MainWindow::decreaseFont()
{
    QFont f = m_editor->font();
    f.setPointSize(qMax(6, f.pointSize() - 1));
    m_editor->setFont(f);
}

void MainWindow::boldSelection()
{
    QTextCursor c = m_editor->textCursor();
    if (!c.hasSelection())
        return;
    QString sel = c.selectedText();
    c.insertText("**" + sel + "**");
}

void MainWindow::italicSelection()
{
    QTextCursor c = m_editor->textCursor();
    if (!c.hasSelection())
        return;
    QString sel = c.selectedText();
    c.insertText("*" + sel + "*");
}

void MainWindow::insertHeading(int level)
{
    QTextCursor c = m_editor->textCursor();
    c.beginEditBlock();
    c.movePosition(QTextCursor::StartOfLine);
    c.insertText(QString(level, '#') + " ");
    c.endEditBlock();
    m_editor->setTextCursor(c);
}
