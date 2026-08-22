#include "mainwindow.h"

#include "editor.h"
#include "outline_model.h"
#include "outline_view.h"
#include "preview_widget.h"
#include "exporter.h"
#include "shutter_panel.h"
#include "left_panel.h"
#include "toggle_switch.h"

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
#include <QSettings>
#include <QPainter>
#include <QHBoxLayout>
#include <QTimer>
#include <QTabWidget>
#include <QTabBar>
#include <QCloseEvent>
#include <cmath>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QTemporaryFile>
#include <QTimer>
#endif

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
    p.fillRect(paneRect, QColor("#879fbd"));

    p.setPen(QPen(QColor("#5a6b82"), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outer, 2, 2);
    p.drawLine(QPointF(dividerX, outer.top()), QPointF(dividerX, outer.bottom()));
    p.end();
    return QIcon(pm);
}

QIcon makeSunIcon()
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(7, 7);
    p.setBrush(QColor("#f5a623"));
    p.setPen(Qt::NoPen);
    p.drawEllipse(c, 3.2, 3.2);
    p.setPen(QPen(QColor("#f5a623"), 1.2));
    for (int i = 0; i < 8; ++i) {
        const qreal angle = i * M_PI / 4.0;
        const QPointF dir(std::cos(angle), std::sin(angle));
        p.drawLine(c + dir * 4.6, c + dir * 6.3);
    }
    p.end();
    return QIcon(pm);
}

QIcon makeMoonIcon()
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#8a97a8"));
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
    , m_modeToggle(nullptr)
    , m_topBar(nullptr)
    , m_leftToggleBtn(nullptr)
    , m_rightToggleBtn(nullptr)
    , m_topFileLabel(nullptr)
    , m_alwaysOpenPreviewAction(nullptr)
    , m_previewDebounce(nullptr)
    , m_autosaveDebounce(nullptr)
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
    m_leftPanel = new LeftPanel();
    m_leftPanel->setOutlineModel(m_outlineModel);

    m_editorTabs = new QTabWidget();
    m_editorTabs->setTabsClosable(true);
    m_editorTabs->setMovable(true);
    m_editorTabs->setDocumentMode(true);
    connect(m_editorTabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_editorTabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    m_preview = new PreviewWidget();

    // Left panel: its own shutter container (opens/closes the outline/DIR view).
    m_leftShutter = new ShutterPanel(m_leftPanel, /*side=*/0, m_splitter);
    m_leftShutter->setOpen(true); // outline starts OPEN

    // Right panel: its own shutter container (opens/closes the preview).
    // Closed by default; the "always open webview" setting can override this.
    m_rightShutter = new ShutterPanel(m_preview, /*side=*/1, m_splitter);
    m_rightShutter->setOpen(false); // preview starts CLOSED -> WebView stays uncreated

    m_splitter->addWidget(m_leftShutter);
    m_splitter->addWidget(m_editorTabs);
    m_splitter->addWidget(m_rightShutter);
    m_splitter->setStretchFactor(0, 0); // left: fixed
    m_splitter->setStretchFactor(1, 1); // editor: grows
    m_splitter->setStretchFactor(2, 0); // right: fixed
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
        m_editor->goToLine(block);
    });

    // --- DIR view: click a sibling .md file to open it in a new tab ---
    connect(m_leftPanel, &LeftPanel::fileActivated, this, [this](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
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

    // --- Debounced secondary work from the editor ---
    // Preview conversion shells out to pandoc, so it's debounced separately
    // from the cheap stats/outline refresh to avoid spawning a process per
    // keystroke.
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(250);
    connect(m_previewDebounce, &QTimer::timeout, this, &MainWindow::updatePreview);

    // Autosave: a couple of seconds after typing stops, silently save
    // whichever tab was being edited (only if it already has a path).
    m_autosaveDebounce = new QTimer(this);
    m_autosaveDebounce->setSingleShot(true);
    m_autosaveDebounce->setInterval(2000);
    connect(m_autosaveDebounce, &QTimer::timeout, this, [this]() {
        flushAutosave(m_autosaveTarget);
    });

    createMenus();
    createStatusBar();

    // First tab. Creating it fires currentChanged() synchronously, which
    // reaches into m_wordLabel/m_charLabel etc. via syncActiveTabUi(), so the
    // status bar (and menus) must already exist by this point.
    m_editor = createEditorTab(QString(), QString());

    // "Always open webview" setting: preview is closed by default, but the
    // user can opt into having it open on launch.
    QSettings settings;
    const bool alwaysOpenPreview = settings.value("alwaysOpenWebview", false).toBool();
    m_alwaysOpenPreviewAction->setChecked(alwaysOpenPreview);
    if (alwaysOpenPreview)
        m_rightShutter->setOpen(true);

    // Light/Dark remembers whichever side was last picked, same as the
    // Outline/DIR toggle.
    if (settings.value("darkMode", false).toBool())
        toggleDarkMode();

    // Keyboard toggles for the panels are the Ctrl+1 / Ctrl+3 shortcuts
    // already attached to the View menu actions above.

    // Seed some content so the UI is usable immediately.
    m_editor->setPlainText(QStringLiteral(
        "# Hello, mdraft\n\n"
        "This is a *Markdown* editor. Click an outline entry on the left to "
        "jump to that heading.\n\n"
        "## Features\n\n- Three-panel layout\n- Native outline\n- Live stats\n"
        "- Light/dark toggle\n\n```cpp\nint main(){return 0;}\n```\n"
    ));
    m_editor->document()->setModified(false); // seed content isn't a user edit
    syncActiveTabUi();
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

void MainWindow::setCurrentFile(const QString &path)
{
    m_currentFile = path;
    setFilePathOfEditor(m_editor, path);

    QString shown = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    setWindowTitle(QString("%1 — mdraft").arg(shown));
    if (m_topFileLabel)
        m_topFileLabel->setText(shown);
    if (m_fileLabel)
        m_fileLabel->setText(path.isEmpty() ? QString() : QFileInfo(path).absolutePath());
    m_leftPanel->setCurrentFilePath(path);
    updateTabModifiedIndicator(m_editor);
}

// ---------------------------------------------------------------------------
// Tabs: one MarkdownEditor per open document
// ---------------------------------------------------------------------------

QString MainWindow::filePathOfEditor(MarkdownEditor *ed) const
{
    return ed->property("mdraftFilePath").toString();
}

void MainWindow::setFilePathOfEditor(MarkdownEditor *ed, const QString &path)
{
    ed->setProperty("mdraftFilePath", path);
}

MarkdownEditor *MainWindow::createEditorTab(const QString &path, const QString &content)
{
    auto *editor = new MarkdownEditor();
    editor->setPlainText(content);
    editor->document()->setModified(false); // loading content isn't a user edit
    setFilePathOfEditor(editor, path);

    // Only the active tab's edits should drive stats/outline/preview, but
    // the modified indicator and autosave scheduling apply to whichever tab
    // was actually typed in, active or not.
    connect(editor, &MarkdownEditor::contentChanged, this, [this, editor]() {
        updateTabModifiedIndicator(editor);
        if (editor->document()->isModified() && !filePathOfEditor(editor).isEmpty()) {
            m_autosaveTarget = editor;
            m_autosaveDebounce->start();
        }
        if (editor != m_editor)
            return;
        updateStats();
        refreshOutline();
        m_previewDebounce->start();
    });

    const QString label = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    const int idx = m_editorTabs->addTab(editor, label);
    m_editorTabs->setTabToolTip(idx, path);
    m_editorTabs->setCurrentIndex(idx); // triggers onTabChanged -> syncActiveTabUi
    return editor;
}

void MainWindow::onTabChanged(int index)
{
    if (index < 0)
        return;
    MarkdownEditor *previous = m_editor;
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
            flushAutosave(ed);
        } else if (ed->document()->isModified() && !ed->toPlainText().isEmpty()) {
            // Untitled documents have nowhere to autosave to; this is the
            // only case where closing can actually lose work.
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

void MainWindow::flushAutosave(MarkdownEditor *ed)
{
    if (!ed || !ed->document()->isModified())
        return;
    const QString path = filePathOfEditor(ed);
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return; // silent: autosave shouldn't interrupt with a dialog
    QTextStream out(&f);
    out << ed->toPlainText();
    ed->document()->setModified(false);
    updateTabModifiedIndicator(ed);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    int untitledDirty = 0;
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *ed = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(i));
        if (!ed)
            continue;
        if (!filePathOfEditor(ed).isEmpty())
            flushAutosave(ed); // has a path: just save it, no need to ask
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
    event->accept();
}

void MainWindow::syncActiveTabUi()
{
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
}

void MainWindow::openFileAt(const QString &path, const QString &content)
{
    // Reuse an already-open tab for this file instead of opening a duplicate.
    for (int i = 0; i < m_editorTabs->count(); ++i) {
        auto *ed = qobject_cast<MarkdownEditor *>(m_editorTabs->widget(i));
        if (ed && filePathOfEditor(ed) == path) {
            m_editorTabs->setCurrentIndex(i);
            statusBar()->showMessage(tr("Opened %1").arg(path), 3000);
            return;
        }
    }

    // Replace the initial empty/untitled tab in place rather than leaving a
    // stray blank tab around, but only if it's actually untouched.
    if (m_editorTabs->count() == 1 && filePathOfEditor(m_editor).isEmpty()
        && m_editor->toPlainText().isEmpty()) {
        m_editor->setPlainText(content);
        m_editor->document()->setModified(false); // loading content isn't a user edit
        setCurrentFile(path);
    } else {
        m_editor = createEditorTab(path, content);
        setCurrentFile(path);
    }
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
    // Bound as lambdas (not directly to m_editor's own slots) so these always
    // act on whichever tab is currently active, not the tab that happened to
    // be active when the menu was built.
    m_undoAction = editMenu->addAction(tr("&Undo"), this, [this]() { m_editor->undo(); }, QKeySequence::Undo);
    m_redoAction = editMenu->addAction(tr("&Redo"), this, [this]() { m_editor->redo(); }, QKeySequence::Redo);
    editMenu->addSeparator();
    m_cutAction = editMenu->addAction(tr("Cu&t"), this, [this]() { m_editor->cut(); }, QKeySequence::Cut);
    m_copyAction = editMenu->addAction(tr("&Copy"), this, [this]() { m_editor->copy(); }, QKeySequence::Copy);
    m_pasteAction = editMenu->addAction(tr("&Paste"), this, [this]() { m_editor->paste(); }, QKeySequence::Paste);
    m_selectAllAction = editMenu->addAction(tr("Select &All"), this, [this]() { m_editor->selectAll(); }, QKeySequence::SelectAll);

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

    // Settings
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    m_alwaysOpenPreviewAction = settingsMenu->addAction(tr("Always Open Preview on Launch"));
    m_alwaysOpenPreviewAction->setCheckable(true);
    connect(m_alwaysOpenPreviewAction, &QAction::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue("alwaysOpenWebview", checked);
    });
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
    const QString bg = m_darkMode ? "#232323" : "#e6ebf2";
    const QString border = m_darkMode ? "#3a3a3a" : "#cdd7e4";
    m_topBar->setStyleSheet(
        QString("QWidget#topBar { background:%1; border-bottom:1px solid %2; }").arg(bg, border));
    m_topFileLabel->setStyleSheet(
        QString("font-weight:600; color:%1;").arg(m_darkMode ? "#e8edf5" : "#33404f"));
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

    QLabel *sunLabel = new QLabel(modeWidget);
    sunLabel->setPixmap(makeSunIcon().pixmap(14, 14));
    QLabel *moonLabel = new QLabel(modeWidget);
    moonLabel->setPixmap(makeMoonIcon().pixmap(14, 14));

    m_modeToggle = new ToggleSwitch(modeWidget);
    m_modeToggle->setChecked(false);
    m_modeToggle->setToolTip(tr("Toggle light/dark mode (Ctrl+D)"));
    connect(m_modeToggle, &QAbstractButton::toggled, this, [this](bool checked) {
        if (checked != m_darkMode)
            toggleDarkMode();
    });

    modeLayout->addWidget(sunLabel);
    modeLayout->addWidget(m_modeToggle);
    modeLayout->addWidget(moonLabel);

    statusBar()->addWidget(m_fileLabel, 1);
    statusBar()->addPermanentWidget(m_wordLabel);
    statusBar()->addPermanentWidget(m_charLabel);
    statusBar()->addPermanentWidget(modeWidget); // far right
    // Word/char counts are populated once the first tab exists (see
    // syncActiveTabUi(), called from createEditorTab() right after this).
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
    m_editor->document()->setModified(false);
    updateTabModifiedIndicator(m_editor);
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
    m_leftShutter->setOpen(!m_leftShutter->isOpen());
}

void MainWindow::toggleRightPanel()
{
    m_rightShutter->setOpen(!m_rightShutter->isOpen());
    // The shutter's opened()/closed() signals drive the WebView lifecycle.
}

void MainWindow::toggleDarkMode()
{
    m_darkMode = !m_darkMode;
    QSettings().setValue("darkMode", m_darkMode);
    if (m_darkMode) {
        setStyleSheet(
            "QWidget { background-color:#2b2b2b; color:#e0e0e0; }"
            "QMenuBar { background-color:#333; }"
            "QMenuBar::item:selected { background-color:#444; }"
            "QMenu { background-color:#2b2b2b; color:#e0e0e0; }"
            "QStatusBar { background-color:#222; }"
            "QTreeView { background-color:#2b2b2b; color:#e0e0e0; }"
            "QListWidget { background-color:#2b2b2b; color:#e0e0e0; }"
            // QPlainTextEdit paints its viewport separately from generic
            // QWidget styling, so both the document editor and the DIR
            // listing need an explicit rule to actually go dark.
            "QPlainTextEdit { background-color:#1e1e1e; color:#e0e0e0; }"
            "QTabWidget::pane { background-color:#2b2b2b; border-color:#444; }"
            "QTabBar::tab { background-color:#333; color:#e0e0e0; padding:4px 10px; }"
            "QTabBar::tab:selected { background-color:#1e1e1e; }"
        );
        m_modeToggle->setChecked(true);
    } else {
        setStyleSheet(QString());
        m_modeToggle->setChecked(false);
    }
    m_preview->setDarkMode(m_darkMode);
    m_leftPanel->setDarkMode(m_darkMode);
    applyTopBarTheme();
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
