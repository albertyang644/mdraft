#include "left_panel.h"
#include "outline_view.h"
#include "outline_model.h"
#include "theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QFileInfo>
#include <QDir>
#include <QFileSystemWatcher>
#include <QPainter>
#include <QSettings>

namespace {
// Small nested-outline glyph: three lines at increasing indent.
QIcon makeOutlineIcon(bool dark)
{
    QPixmap pm(14, 14);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(dark ? Theme::OutlineDark : Theme::OutlineLight), 1.4));
    p.drawLine(QPointF(1, 3), QPointF(13, 3));
    p.drawLine(QPointF(4, 7), QPointF(13, 7));
    p.drawLine(QPointF(7, 11), QPointF(13, 11));
    p.end();
    return QIcon(pm);
}
}

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , m_dirWatcher(new QFileSystemWatcher(this))
    , m_dark(false)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- Segmented toggle: Outline | DIR ---
    m_toggleBar = new QWidget(this);
    m_toggleBar->setObjectName("leftToggleBar");
    auto *toggleLayout = new QHBoxLayout(m_toggleBar);
    toggleLayout->setContentsMargins(4, 3, 4, 3);
    toggleLayout->setSpacing(0);

    m_outlineBtn = new QPushButton(tr(" Outline"), m_toggleBar);
    m_dirBtn = new QPushButton(tr("DIR"), m_toggleBar);
    m_outlineBtn->setCheckable(true);
    m_dirBtn->setCheckable(true);
    m_outlineBtn->setCursor(Qt::PointingHandCursor);
    m_dirBtn->setCursor(Qt::PointingHandCursor);
    m_outlineBtn->setToolTip(tr("Show the document outline"));
    m_dirBtn->setToolTip(tr("Show Markdown files in this document's directory"));

    auto *group = new QButtonGroup(this);
    group->setExclusive(true);
    group->addButton(m_outlineBtn);
    group->addButton(m_dirBtn);
    m_outlineBtn->setChecked(true);
    applyToggleBarStyle();

    toggleLayout->addWidget(m_outlineBtn);
    toggleLayout->addWidget(m_dirBtn);
    toggleLayout->addStretch(1);

    connect(m_outlineBtn, &QPushButton::clicked, this, &LeftPanel::showOutline);
    connect(m_dirBtn, &QPushButton::clicked, this, &LeftPanel::showDirListing);

    // --- Content stack ---
    m_stack = new QStackedWidget(this);
    m_outlineView = new OutlineView(m_stack);

    // DIR page: a "└─ /path" header above the file list, so the directory
    // you're browsing is visible right where you're clicking into it (the
    // bottom status bar shows the *open file's* directory, which is a
    // different thing once you've clicked into a sibling).
    QWidget *dirPage = new QWidget(m_stack);
    auto *dirLayout = new QVBoxLayout(dirPage);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->setSpacing(0);

    m_dirPathLabel = new QLabel(dirPage);
    m_dirPathLabel->setContentsMargins(8, 4, 8, 4);
    m_dirPathLabel->setWordWrap(true);

    m_dirView = new QListWidget(dirPage);
    m_dirView->setFrameShape(QFrame::NoFrame);
    connect(m_dirView, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            emit fileActivated(path);
    });
    connect(m_dirWatcher, &QFileSystemWatcher::directoryChanged,
            this, [this]() { refreshDirListing(); });

    dirLayout->addWidget(m_dirPathLabel);
    dirLayout->addWidget(m_dirView, 1);
    applyDirHeaderStyle();

    m_stack->addWidget(m_outlineView); // index 0: outline
    m_stack->addWidget(dirPage);       // index 1: dir listing

    outer->addWidget(m_toggleBar);
    outer->addWidget(m_stack, 1);

    // The toggle is semi-permanent: it remembers whichever side you left it
    // on, across restarts too.
    if (QSettings().value("leftPanelDirMode", false).toBool()) {
        m_dirBtn->setChecked(true);
        showDirListing();
    }
}

void LeftPanel::setOutlineModel(OutlineModel *model)
{
    m_outlineView->setModel(model);
    connect(m_outlineView, &OutlineView::goToBlock, this, &LeftPanel::goToBlock);
}

void LeftPanel::setDarkMode(bool dark)
{
    if (m_dark == dark)
        return;
    m_dark = dark;
    applyToggleBarStyle();
    applyDirHeaderStyle();
}

void LeftPanel::applyToggleBarStyle()
{
    const QString barBg = m_dark ? Theme::DarkPanel : Theme::LightPanel;
    const QString barBorder = m_dark ? Theme::DarkBorder : Theme::LightBorder;
    m_toggleBar->setStyleSheet(
        QString("QWidget#leftToggleBar { background:%1; border-bottom:1px solid %2; }")
            .arg(barBg, barBorder));

    const QString btnBg = m_dark ? Theme::DarkWidget : Theme::LightPanelAlt;
    const QString btnText = m_dark ? Theme::DarkText : Theme::LightText;
    const QString checkedBg = m_dark ? Theme::CheckedDark : Theme::Accent;
    const QString segStyle = QString(
        "QPushButton { padding:3px 10px; border:1px solid %1; background:%2; color:%3; }"
        "QPushButton:checked { background:%4; color:white; border-color:%4; }")
        .arg(barBorder, btnBg, btnText, checkedBg);

    m_outlineBtn->setStyleSheet(segStyle + "QPushButton{border-top-right-radius:0;border-bottom-right-radius:0;}");
    m_dirBtn->setStyleSheet(segStyle + "QPushButton{border-top-left-radius:0;border-bottom-left-radius:0;border-left:none;}");
    m_outlineBtn->setIcon(makeOutlineIcon(m_dark));
}

void LeftPanel::applyDirHeaderStyle()
{
    const QString bg = m_dark ? Theme::DarkPanel : Theme::DirLight;
    const QString border = m_dark ? Theme::DarkBorder : Theme::DirLightBorder;
    const QString text = m_dark ? Theme::DirDarkText : Theme::DirLightText;
    m_dirPathLabel->setStyleSheet(
        QString("QLabel { background:%1; border-bottom:1px solid %2; color:%3; font-size:11px; }")
            .arg(bg, border, text));
}

void LeftPanel::setCurrentFilePath(const QString &path)
{
    if (!m_dirWatcher->directories().isEmpty())
        m_dirWatcher->removePaths(m_dirWatcher->directories());
    m_currentDir = path.isEmpty() ? QString() : QFileInfo(path).absolutePath();
    if (!m_currentDir.isEmpty() && QDir(m_currentDir).exists())
        m_dirWatcher->addPath(m_currentDir);
    if (m_stack->currentIndex() == 1)
        refreshDirListing();
}

void LeftPanel::showOutline()
{
    m_outlineBtn->setChecked(true);
    m_stack->setCurrentIndex(0);
    QSettings().setValue("leftPanelDirMode", false);
}

void LeftPanel::showDirListing()
{
    m_dirBtn->setChecked(true);
    m_stack->setCurrentIndex(1);
    QSettings().setValue("leftPanelDirMode", true);
    refreshDirListing();
}

void LeftPanel::refreshDirListing()
{
    m_dirPathLabel->setText(m_currentDir.isEmpty() ? tr("No directory yet") : "└─ " + m_currentDir);

    m_dirView->clear();
    if (m_currentDir.isEmpty()) {
        auto *item = new QListWidgetItem(tr("Save the document to see its directory."));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        m_dirView->addItem(item);
        return;
    }
    const QStringList names = QDir(m_currentDir).entryList(
        QStringList() << "*.md" << "*.markdown", QDir::Files, QDir::Name);
    if (names.isEmpty()) {
        auto *item = new QListWidgetItem(tr("No Markdown files here."));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        m_dirView->addItem(item);
        return;
    }
    for (const QString &name : names) {
        auto *item = new QListWidgetItem(name, m_dirView);
        item->setData(Qt::UserRole, QDir(m_currentDir).filePath(name));
    }
}
