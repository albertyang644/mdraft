#include "mainwindow.h"

#include "editor.h"
#include "outline_model.h"
#include "outline_view.h"
#include "preview_widget.h"
#include "exporter.h"
#include "shutter_panel.h"
#include "left_panel.h"
#include "toggle_switch.h"
#include "theme.h"

#include <QSplitter>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QLabel>
#include <QMimeData>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QStatusBar>
#include <QMenuBar>
#include <QFontDialog>
#include <QTextCursor>
#include <QFileInfo>
#include <QSettings>
#include <QSignalBlocker>
#include <QPainter>
#include <QHBoxLayout>
#include <QTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QCloseEvent>
#include <QDir>
#include <cmath>

namespace {
// A small "toggle sidebar" glyph: an outlined rect with a vertical divider,
// the pane-side filled. `paneOnLeft` picks which side of the divider is
// filled, so the left and right top-bar buttons can mirror each other.
QIcon makeShutterIcon(bool paneOnLeft)
{
    QPixmap pm(20, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF outer(1.0, 1.0, 18.0, 14.0);
    qreal dividerX = paneOnLeft ? outer.left() + outer.width() * 0.32
                                : outer.left() + outer.width() * 0.68;

    QRectF paneRect = paneOnLeft
        ? QRectF(outer.left(), outer.top(), dividerX - outer.left(), outer.height())
        : QRectF(dividerX, outer.top(), outer.right() - dividerX, outer.height());
    p.fillRect(paneRect, QColor(Theme::Accent));

    p.setPen(QPen(QColor(Theme::AccentDark), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outer, 2, 2);
    p.drawLine(QPointF(dividerX, outer.top()), QPointF(dividerX, outer.bottom()));
    p.end();
    return QIcon(pm);
}

QIcon makeSunIcon(const QColor &color)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(7, 7);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(c, 3.2, 3.2);
    p.setPen(QPen(color, 1.2));
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * M_PI / 4.0;
        const QPointF dir(std::cos(angle), std::sin(angle));
        p.drawLine(c + dir * 4.6, c + dir * 6.3);
    }
    p.end();
    return QIcon(pm);
}

QIcon makeMoonIcon(const QColor &color)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(QRectF(2, 2, 10, 10));
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setBrush(Qt::black);
    p.drawEllipse(QRectF(5, 1, 10, 10));
    p.end();
    return QIcon(pm);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_splitter(nullptr)
    , m_leftShutter(nullptr)
    , m_rightShutter(nullptr)
    , m_leftPanel(nullptr)
    , m_outlineModel(nullptr)
    , m_editorTabs(nullptr)
    , m_editor(nullptr)
    , m_preview(nullptr)
    , m_wordLabel(nullptr)
    , m_charLabel(nullptr)
    , m_fileLabel(nullptr)
    , m_sunLabel(nullptr)
    , m_moonLabel(nullptr)
    , m_modeToggle(nullptr)
    , m_topBar(nullptr)
    , m_leftToggleBtn(nullptr)
    , m_rightToggleBtn(nullptr)
    , m_topFileLabel(nullptr)
    , m_previewDebounce(nullptr)
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
    const QByteArray savedGeometry = QSettings().value("windowGeometry").toByteArray();
    if (!savedGeometry.isEmpty())
        restoreGeometry(savedGeometry);

    // --- Three-panel layout ---
    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_outlineModel = new OutlineModel(this);
    m_leftPanel = new LeftPanel();
    m_leftPanel->setOutlineModel(m_outlineModel);

    m_editorTabs = new QTabWidget();
    m_editorTabs->setObjectName("editorTabs");
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_preview = new PreviewWidget();

    // Left panel: its own shutter container (opens/closes the outline/DIR view).
    m_leftShutter = new ShutterPanel(m_leftPanel, m_splitter);
    m_leftShutter->setOpen(true); // outline starts OPEN

    // Right panel: its own shutter container (opens/closes the preview).
    m_rightShutter = new ShutterPanel(m_preview, m_splitter);

    m_splitter->addWidget(m_leftShutter);
    m_splitter->addWidget(m_editorTabs);
    m_splitter->addWidget(m_rightShutter);
    m_splitter->setStretchFactor(0, 0); // left: fixed
    m_splitter->setStretchFactor(1, 1); // editor: grows
    m_splitter->setStretchFactor(2, 0); // right: fixed
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(2, false);
    m_splitter->setSizes({260, 700, 18});

    // --- Central widget: top bar above the three-panel splitter ---
    QWidget *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    createTopBar();
    centralLayout->addWidget(m_topBar);
    centralLayout->addWidget(m_splitter, 1);
    setCentralWidget(central);

    // --- Wire outline navigation ---
    connect(m_leftPanel, &LeftPanel::goToBlock, this, [this](int block) {
        if (m_editor)
            m_editor->goToLine(block);
    });

    // --- DIR view: click a sibling .md file to open it in a new tab ---
    connect(m_leftPanel, &LeftPanel::fileActivated, this, [this](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            statusBar()->showMessage(tr("Cannot open %1: %2").arg(path, f.errorString()), 5000);
            return;
        }
        QTextStream in(&f);
        openFileAt(path, in.readAll());
    });

    // --- Shutter lifecycle: preview WebView only exists while the right
    //     panel is open (HARD_CONTRACT).
    connect(m_rightShutter, &ShutterPanel::opened, this, [this]() {
        m_preview->openPreview();
        updatePreview();

        // The WebView is (re)created fresh every time it's opened, per the
        // HARD_CONTRACT above; give it a fair 50/50 split against the editor
        // each time, rather than whatever sliver it last collapsed to.
        QList<int> sizes = m_splitter->sizes();
        if (sizes.size() == 3) {
            const int total = sizes[1] + sizes[2];
            sizes[1] = total / 2;
            sizes[2] = total - sizes[1];
            m_splitter->setSizes(sizes);
        }
    });
    connect(m_rightShutter, &ShutterPanel::closed, this, [this]() {
        m_preview->teardownPreview();
    });
    // Closing occurs after lifecycle signals are connected. No WebView has
    // been created, and no setting may override this startup invariant.
    m_rightShutter->setOpen(false);

    // --- Debounced secondary work from the editor ---
    // Preview conversion shells out to pandoc, so it's debounced separately
    // from the cheap stats/outline refresh to avoid spawning a process per
    // keystroke.
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(250);
    connect(m_previewDebounce, &QTimer::timeout, this, &MainWindow::updatePreview);

    createMenus();
    createStatusBar();

    // First tab. Creating it fires currentChanged() synchronously, which
    // reaches into m_wordLabel/m_charLabel etc. via syncActiveTabUi(), so the
    // status bar (and menus) must already exist by this point.
    m_editor = createEditorTab(QString(), QString());

    QSettings settings;
    // Light/Dark remembers whichever side was last picked, same as the
    // Outline/DIR toggle.
    applyDarkMode(settings.value("darkMode", false).toBool(), false);

    // Keyboard toggles for the panels are the Ctrl+1 / Ctrl+3 shortcuts
    // already attached to the View menu actions above.

    syncActiveTabUi();
    statusBar()->showMessage(tr("Ready"), 2000);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Editor text helpers
// ---------------------------------------------------------------------------

QString MainWindow::currentMarkdown() const
{
    return m_editor ? m_editor->toPlainText() : QString();
}

void MainWindow::setCurrentFile(const QString &path)
{
    if (!m_editor)
        return;
    setFilePathOfEditor(m_editor, path);
    m_currentFile = filePathOfEditor(m_editor);

    QString shown = m_currentFile.isEmpty() ? tr("Untitled") : QFileInfo(m_currentFile).fileName();
    setWindowTitle(QString("%1 — mdraft").arg(shown));
    if (m_topFileLabel)
        m_topFileLabel->setText(shown);
    if (m_fileLabel)
        m_fileLabel->setText(m_currentFile.isEmpty() ? QString() : QFileInfo(m_currentFile).absolutePath());
    m_leftPanel->setCurrentFilePath(m_currentFile);
    updateTabModifiedIndicator(m_editor);
}

// ---------------------------------------------------------------------------
// Tabs: one MarkdownEditor per open document
// ---------------------------------------------------------------------------

QString MainWindow::filePathOfEditor(MarkdownEditor *ed) const
{
    const auto it = m_documents.constFind(ed);
    return it == m_documents.cend() ? QString() : it->path();
}

void MainWindow::setFilePathOfEditor(MarkdownEditor *ed, const QString &path)
{
    if (ed)
        m_documents[ed].setPath(path);
}

MarkdownEditor *MainWindow::editorForPath(const QString &path) const
{
    const QString normalized = DocumentFile::normalizedPath(path);
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *editor = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(i));
        if (editor && filePathOfEditor(editor) == normalized)
            return editor;
    }
    return nullptr;
}

MarkdownEditor *MainWindow::createEditorTab(const QString &path, const QString &content)
{
    auto *editor = new MarkdownEditor();
    editor->setDarkMode(m_darkMode);
    editor->setPlainText(content);
    editor->document()->setModified(false); // loading content isn't a user edit
    m_documents.insert(editor, DocumentFile(path));

    auto *autosaveTimer = new QTimer(editor);
    autosaveTimer->setSingleShot(true);
    autosaveTimer->setInterval(2000);
    connect(autosaveTimer, &QTimer::timeout, this, [this, editor]() {
        flushAutosave(editor);
    });

    // Only the active tab's edits should drive stats/outline/preview, but
    // the modified indicator and autosave scheduling apply to whichever tab
    // was actually typed in, active or not.
    connect(editor, &MarkdownEditor::contentChanged, this, [this, editor, autosaveTimer]() {
        updateTabModifiedIndicator(editor);
        if (editor->document()->isModified() && !filePathOfEditor(editor).isEmpty())
            autosaveTimer->start();
        if (editor != m_editor)
            return;
        updateStats();
        refreshOutline();
        m_previewDebounce->start();
    });
    connect(editor, &QPlainTextEdit::undoAvailable, this, [this, editor](bool) {
        if (editor == m_editor)
            updateEditActions();
    });
    connect(editor, &QPlainTextEdit::redoAvailable, this, [this, editor](bool) {
        if (editor == m_editor)
            updateEditActions();
    });
    connect(editor, &QPlainTextEdit::copyAvailable, this, [this, editor](bool) {
        if (editor == m_editor)
            updateEditActions();
    });

    const QString label = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    const int idx = m_editorTabs->addTab(editor, label);
    m_editorTabs->setTabToolTip(idx, path);
    m_editorTabs->setCurrentIndex(idx); // triggers onTabChanged -> syncActiveTabUi
    return editor;
}

void MainWindow::onTabChanged(int index)
{
    MarkdownEditor *previous = m_editor;
    if (index < 0) {
        m_editor.clear();
        m_currentFile.clear();
        return;
    }
    m_editor = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(index));
    if (!m_editor)
        return;
    // Flush the tab you're leaving rather than letting it sit dirty on disk
    // until its own autosave timer happens to fire.
    if (previous && previous != m_editor)
        flushAutosave(previous);
    syncActiveTabUi();
}

void MainWindow::onTabCloseRequested(int index)
{
    auto *ed = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(index));
    if (ed) {
        if (!filePathOfEditor(ed).isEmpty()) {
            if (!flushAutosave(ed, true))
                return;
        } else if (ed->document()->isModified() && !ed->toPlainText().isEmpty()) {
            // Untitled documents need an explicit discard confirmation.
            const auto r = QMessageBox::question(this, tr("Close Tab"),
                tr("This untitled document has unsaved changes and can't be "
                   "autosaved. Close it anyway?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (r != QMessageBox::Yes)
                return;
        }
    }

    QWidget *w = m_editorTabs->widget(index);
    m_editorTabs->removeTab(index);
    m_documents.remove(ed);
    delete w;

    // Always keep at least one tab open.
    if (m_editorTabs->count() == 0)
        createEditorTab(QString(), QString());
}

void MainWindow::updateTabModifiedIndicator(MarkdownEditor *ed)
{
    const int idx = m_editorTabs->indexOf(ed);
    if (idx < 0)
        return;
    const QString path = filePathOfEditor(ed);
    QString shown = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    if (ed->document()->isModified())
        shown += " *";
    m_editorTabs->setTabText(idx, shown);
    m_editorTabs->setTabToolTip(idx, path);
}

bool MainWindow::flushAutosave(MarkdownEditor *ed, bool reportError)
{
    if (!ed || !ed->document()->isModified())
        return true;
    const QString path = filePathOfEditor(ed);
    if (path.isEmpty())
        return false;
    return saveEditorToPath(ed, path, reportError, true);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    int untitledDirty = 0;
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *ed = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(i));
        if (!ed)
            continue;
        if (!filePathOfEditor(ed).isEmpty()) {
            if (!flushAutosave(ed, true)) {
                event->ignore();
                return;
            }
        }
        else if (ed->document()->isModified() && !ed->toPlainText().isEmpty())
            ++untitledDirty; // nowhere to autosave to: this is the real risk
    }

    if (untitledDirty > 0) {
        const auto r = QMessageBox::question(this, tr("Quit"),
            tr("%1 untitled document(s) have unsaved changes and can't be "
               "autosaved. Quit anyway?").arg(untitledDirty),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (r != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    QSettings().setValue("windowGeometry", saveGeometry());
    event->accept();
}

void MainWindow::syncActiveTabUi()
{
    if (!m_editor)
        return;
    m_currentFile = filePathOfEditor(m_editor);
    const QString shown = m_currentFile.isEmpty() ? tr("Untitled") : QFileInfo(m_currentFile).fileName();
    setWindowTitle(QString("%1 — mdraft").arg(shown));
    if (m_topFileLabel)
        m_topFileLabel->setText(shown);
    if (m_fileLabel)
        m_fileLabel->setText(m_currentFile.isEmpty() ? QString() : QFileInfo(m_currentFile).absolutePath());
    m_leftPanel->setCurrentFilePath(m_currentFile);

    updateStats();
    refreshOutline();
    updatePreview();
    updateEditActions();
}

void MainWindow::openFileAt(const QString &path, const QString &content)
{
    const QString normalized = DocumentFile::normalizedPath(path);
    // Reuse an already-open tab for this file instead of opening a duplicate.
    if (MarkdownEditor *existing = editorForPath(normalized)) {
        m_editorTabs->setCurrentWidget(existing);
        statusBar()->showMessage(tr("Opened %1").arg(normalized), 3000);
        return;
    }

    // Replace the initial empty/untitled tab in place rather than leaving a
    // stray blank tab around, but only if it's actually untouched.
    if (m_editorTabs->count() == 1 && filePathOfEditor(m_editor).isEmpty()
        && m_editor->toPlainText().isEmpty()) {
        m_editor->setPlainText(content);
        m_editor->document()->setModified(false); // loading content isn't a user edit
        setCurrentFile(normalized);
    } else {
        m_editor = createEditorTab(normalized, content);
        setCurrentFile(normalized);
    }
    statusBar()->showMessage(tr("Opened %1").arg(normalized), 3000);
}

// ---------------------------------------------------------------------------
// Menu construction (Ghostwriter-inspired coverage)
// ---------------------------------------------------------------------------

void MainWindow::createMenus()
{
    // File
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), QKeySequence::New, this, &MainWindow::newFile);
    fileMenu->addAction(tr("&Open…"), QKeySequence::Open, this, &MainWindow::openFile);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, &MainWindow::saveFileAs);
    fileMenu->addSeparator();
    QMenu *exportMenu = fileMenu->addMenu(tr("E&xport"));
    exportMenu->addAction(tr("Export &HTML…"), this, &MainWindow::exportHtml);
    exportMenu->addAction(tr("Export &PDF…"), this, &MainWindow::exportPdf);
    exportMenu->addAction(tr("Export &LaTeX…"), this, &MainWindow::exportLatex);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);

    // Edit
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    // Bound as lambdas (not directly to m_editor's own slots) so these always
    // act on whichever tab is currently active, not the tab that happened to
    // be active when the menu was built.
    m_undoAction = editMenu->addAction(tr("&Undo"), QKeySequence::Undo, this, [this]() { m_editor->undo(); });
    m_redoAction = editMenu->addAction(tr("&Redo"), QKeySequence::Redo, this, [this]() { m_editor->redo(); });
    editMenu->addSeparator();
    m_cutAction = editMenu->addAction(tr("Cu&t"), QKeySequence::Cut, this, [this]() { m_editor->cut(); });
    m_copyAction = editMenu->addAction(tr("&Copy"), QKeySequence::Copy, this, [this]() { m_editor->copy(); });
    m_pasteAction = editMenu->addAction(tr("&Paste"), QKeySequence::Paste, this, [this]() { m_editor->paste(); });
    m_selectAllAction = editMenu->addAction(tr("Select &All"), QKeySequence::SelectAll, this, [this]() { m_editor->selectAll(); });
    connect(QApplication::clipboard(), &QClipboard::dataChanged,
            this, &MainWindow::updateEditActions);

    // Format
    QMenu *formatMenu = menuBar()->addMenu(tr("F&ormat"));
    formatMenu->addAction(tr("&Bold"), QKeySequence(Qt::CTRL | Qt::Key_B), this, &MainWindow::boldSelection);
    formatMenu->addAction(tr("&Italic"), QKeySequence(Qt::CTRL | Qt::Key_I), this, &MainWindow::italicSelection);
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

void MainWindow::createTopBar()
{
    m_topBar = new QWidget(this);
    m_topBar->setObjectName("topBar");
    m_topBar->setFixedHeight(32);

    auto *layout = new QHBoxLayout(m_topBar);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(0);

    m_leftToggleBtn = new QPushButton(m_topBar);
    m_leftToggleBtn->setIcon(makeShutterIcon(/*paneOnLeft=*/true));
    m_leftToggleBtn->setFlat(true);
    m_leftToggleBtn->setFixedSize(28, 24);
    m_leftToggleBtn->setCursor(Qt::PointingHandCursor);
    m_leftToggleBtn->setToolTip(tr("Toggle outline panel (Ctrl+1)"));
    connect(m_leftToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleLeftPanel);

    m_topFileLabel = new QLabel(tr("Untitled"), m_topBar);
    m_topFileLabel->setAlignment(Qt::AlignCenter);

    m_rightToggleBtn = new QPushButton(m_topBar);
    m_rightToggleBtn->setIcon(makeShutterIcon(/*paneOnLeft=*/false));
    m_rightToggleBtn->setFlat(true);
    m_rightToggleBtn->setFixedSize(28, 24);
    m_rightToggleBtn->setCursor(Qt::PointingHandCursor);
    m_rightToggleBtn->setToolTip(tr("Toggle preview panel (Ctrl+3)"));
    connect(m_rightToggleBtn, &QPushButton::clicked, this, &MainWindow::toggleRightPanel);

    layout->addWidget(m_leftToggleBtn, 0, Qt::AlignLeft);
    layout->addWidget(m_topFileLabel, 1);
    layout->addWidget(m_rightToggleBtn, 0, Qt::AlignRight);

    applyTopBarTheme();
}

void MainWindow::applyTopBarTheme()
{
    // The top bar and its file label are locally styled (an ID-selector
    // stylesheet + an explicit label color), which wins over the app-wide
    // dark-mode stylesheet's generic QWidget rule — so they need to be
    // updated explicitly rather than relying on the cascade.
    const QString bg = m_darkMode ? Theme::DarkPanel : Theme::LightPanel;
    const QString border = m_darkMode ? Theme::DarkBorder : Theme::LightBorder;
    m_topBar->setStyleSheet(
        QString("QWidget#topBar { background:%1; border-bottom:1px solid %2; }").arg(bg, border));
    m_topFileLabel->setStyleSheet(
        QString("font-weight:600; color:%1;").arg(m_darkMode ? Theme::DarkTopText : Theme::LightText));
}

void MainWindow::createStatusBar()
{
    m_wordLabel = new QLabel(this);
    m_charLabel = new QLabel(this);
    m_fileLabel = new QLabel(this); // shows the open file's directory, far left
    m_fileLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Light/Dark switch: sun | slider | moon, far right of the status bar.
    QWidget *modeWidget = new QWidget(this);
    auto *modeLayout = new QHBoxLayout(modeWidget);
    modeLayout->setContentsMargins(8, 0, 4, 0);
    modeLayout->setSpacing(6);

    m_sunLabel = new QLabel(modeWidget);
    m_moonLabel = new QLabel(modeWidget);

    m_modeToggle = new ToggleSwitch(modeWidget);
    m_modeToggle->setChecked(false);
    m_modeToggle->setToolTip(tr("Toggle light/dark mode (Ctrl+D)"));
    connect(m_modeToggle, &QAbstractButton::toggled, this, [this](bool checked) {
        if (checked != m_darkMode)
            toggleDarkMode();
    });

    modeLayout->addWidget(m_sunLabel);
    modeLayout->addWidget(m_modeToggle);
    modeLayout->addWidget(m_moonLabel);

    statusBar()->addWidget(m_fileLabel, 1);
    statusBar()->addPermanentWidget(m_wordLabel);
    statusBar()->addPermanentWidget(m_charLabel);
    statusBar()->addPermanentWidget(modeWidget); // far right
    applyStatusBarTheme();
    // Word/char counts are populated once the first tab exists (see
    // syncActiveTabUi(), called from createEditorTab() right after this).
}

void MainWindow::applyStatusBarTheme()
{
    const QColor foreground(m_darkMode ? Theme::DarkText : Theme::LightText);
    const QString labelStyle = QStringLiteral("color:%1;").arg(foreground.name());
    m_fileLabel->setStyleSheet(labelStyle);
    m_wordLabel->setStyleSheet(labelStyle);
    m_charLabel->setStyleSheet(labelStyle);
    m_sunLabel->setPixmap(makeSunIcon(foreground).pixmap(14, 14));
    m_moonLabel->setPixmap(makeMoonIcon(foreground).pixmap(14, 14));
    m_modeToggle->setForegroundColor(foreground);
}

// ---------------------------------------------------------------------------
// Stats + outline + preview (debounced, off the typing hot path)
// ---------------------------------------------------------------------------

void MainWindow::updateStats()
{
    if (!m_editor)
        return;
    const QString text = m_editor->toPlainText();
    // Word count: split on whitespace.
    int words = 0;
    if (!text.trimmed().isEmpty())
        words = static_cast<int>(
            text.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size());
    m_wordLabel->setText(QString("%1 words").arg(words));
    m_charLabel->setText(QString("%1 chars").arg(text.length()));
}

void MainWindow::refreshOutline()
{
    if (m_editor)
        m_outlineModel->setMarkdown(m_editor->toPlainText());
}

void MainWindow::updatePreview()
{
    if (m_editor)
        m_preview->setMarkdown(m_editor->toPlainText());
}

void MainWindow::updateEditActions()
{
    const bool hasEditor = !m_editor.isNull();
    const bool hasSelection = hasEditor && m_editor->textCursor().hasSelection();
    const QMimeData *clipboardData = QApplication::clipboard()->mimeData();
    m_undoAction->setEnabled(hasEditor && m_editor->document()->isUndoAvailable());
    m_redoAction->setEnabled(hasEditor && m_editor->document()->isRedoAvailable());
    m_cutAction->setEnabled(hasSelection);
    m_copyAction->setEnabled(hasSelection);
    m_pasteAction->setEnabled(hasEditor && clipboardData && clipboardData->hasText());
    m_selectAllAction->setEnabled(hasEditor && !m_editor->document()->isEmpty());
}

// ---------------------------------------------------------------------------
// File operations
// ---------------------------------------------------------------------------

void MainWindow::newFile()
{
    // Nothing is discarded now that documents live in tabs; just add a
    // fresh blank one.
    m_editor = createEditorTab(QString(), QString());
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
    openFileAt(path, in.readAll());
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
    if (!path.endsWith(".md", Qt::CaseInsensitive))
        path += ".md";
    path = DocumentFile::normalizedPath(path);

    if (MarkdownEditor *existing = editorForPath(path); existing && existing != m_editor) {
        QMessageBox::warning(this, tr("Save As"),
            tr("That file is already open in another tab:\n%1").arg(path));
        m_editorTabs->setCurrentWidget(existing);
        return;
    }

    if (saveToPath(path))
        setCurrentFile(path);
}

bool MainWindow::saveToPath(const QString &path)
{
    if (!m_editor)
        return false;
    const bool samePath = filePathOfEditor(m_editor) == DocumentFile::normalizedPath(path);
    return saveEditorToPath(m_editor, path, true, samePath);
}

bool MainWindow::saveEditorToPath(MarkdownEditor *editor, const QString &path,
                                  bool reportError, bool checkExternalChanges)
{
    if (!editor)
        return false;

    DocumentSaveResult result = m_documents[editor].save(
        editor->toPlainText(), path, checkExternalChanges);
    if (!result.ok) {
        updateTabModifiedIndicator(editor);
        if (reportError)
            QMessageBox::warning(this, tr("Save"), result.error);
        else
            statusBar()->showMessage(result.error.simplified(), 8000);
        return false;
    }

    editor->document()->setModified(false);
    updateTabModifiedIndicator(editor);
    if (editor == m_editor)
        m_currentFile = filePathOfEditor(editor);
    statusBar()->showMessage(tr("Saved %1").arg(filePathOfEditor(editor)), 2000);
    return true;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

void MainWindow::exportHtml()
{
    exportDocument("html", tr("Export HTML"), ".html", tr("HTML (*.html)"));
}

void MainWindow::exportPdf()
{
    exportDocument("pdf", tr("Export PDF"), ".pdf", tr("PDF (*.pdf)"));
}

void MainWindow::exportLatex()
{
    exportDocument("latex", tr("Export LaTeX"), ".tex", tr("LaTeX (*.tex)"));
}

void MainWindow::exportDocument(const QString &format, const QString &title,
                                const QString &suffix, const QString &filter)
{
    QString suggested;
    if (m_currentFile.isEmpty()) {
        suggested = QDir::home().filePath(tr("Untitled") + suffix);
    } else {
        const QFileInfo source(m_currentFile);
        suggested = source.dir().filePath(source.completeBaseName() + suffix);
    }

    QString outPath = QFileDialog::getSaveFileName(this, title, suggested, filter);
    if (outPath.isEmpty())
        return;
    if (!outPath.endsWith(suffix, Qt::CaseInsensitive))
        outPath += suffix;

    statusBar()->showMessage(tr("Exporting to %1…").arg(outPath));
    Exporter::exportMarkdown(currentMarkdown(), outPath, format, this,
        [this, outPath](bool ok, const QString &error) {
            if (ok)
                statusBar()->showMessage(tr("Exported to %1").arg(outPath), 3000);
            else
                QMessageBox::warning(this, tr("Export"), error);
        });
}

// ---------------------------------------------------------------------------
// View toggles + dark mode
// ---------------------------------------------------------------------------

void MainWindow::toggleLeftPanel()
{
    m_leftShutter->setOpen(!m_leftShutter->isOpen());
}

void MainWindow::toggleRightPanel()
{
    m_rightShutter->setOpen(!m_rightShutter->isOpen());
    // The shutter's opened()/closed() signals drive the WebView lifecycle.
}

void MainWindow::toggleDarkMode()
{
    applyDarkMode(!m_darkMode, true);
}

void MainWindow::applyDarkMode(bool dark, bool persist)
{
    m_darkMode = dark;
    if (persist)
        QSettings().setValue("darkMode", m_darkMode);
    if (m_darkMode) {
        setStyleSheet(Theme::darkApplicationStyleSheet());
    } else {
        setStyleSheet(QString());
    }
    const QSignalBlocker blocker(m_modeToggle);
    m_modeToggle->setChecked(m_darkMode);
    m_preview->setDarkMode(m_darkMode);
    m_leftPanel->setDarkMode(m_darkMode);
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        if (auto *editor = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(i)))
            editor->setDarkMode(m_darkMode);
    }
    applyTopBarTheme();
    applyStatusBarTheme();
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
