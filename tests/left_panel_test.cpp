#include <QtTest/QtTest>

#include "left_panel.h"

#include <QFile>
#include <QListWidget>
#include <QSettings>
#include <QPushButton>
#include <QTemporaryDir>

namespace {
QPushButton *buttonWithText(LeftPanel *panel, const QString &text)
{
    const auto buttons = panel->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text().trimmed() == text)
            return button;
    }
    return nullptr;
}

// The refresh button is the only flat, textless button in the panel.
QPushButton *refreshButton(LeftPanel *panel)
{
    const auto buttons = panel->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text().isEmpty())
            return button;
    }
    return nullptr;
}

QStringList listedNames(LeftPanel *panel)
{
    auto *view = panel->findChild<QListWidget *>();
    QStringList names;
    if (!view)
        return names;
    for (int i = 0; i < view->count(); ++i)
        names << view->item(i)->text();
    return names;
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
}

class LeftPanelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void refreshButtonOnlyShowsInDirMode();
    void refreshPicksUpDirectoryChanges();
    void watcherPicksUpNewFiles();

private:
    QTemporaryDir m_settingsDir;
};

void LeftPanelTest::initTestCase()
{
    // LeftPanel restores its Outline/DIR mode from QSettings, so the tests
    // must not read (or write) the developer's real configuration.
    QVERIFY(m_settingsDir.isValid());
    QCoreApplication::setOrganizationName("mdraft-tests");
    QCoreApplication::setApplicationName("left-panel-test");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void LeftPanelTest::refreshButtonOnlyShowsInDirMode()
{
    LeftPanel panel;
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    QPushButton *refresh = refreshButton(&panel);
    QVERIFY(refresh);

    // Outline is the default mode; refreshing a listing you cannot see would
    // be a no-op control.
    QVERIFY(!refresh->isVisible());

    QPushButton *dir = buttonWithText(&panel, QStringLiteral("DIR"));
    QVERIFY(dir);
    dir->click();
    QVERIFY(refresh->isVisible());

    QPushButton *outline = buttonWithText(&panel, QStringLiteral("Outline"));
    QVERIFY(outline);
    outline->click();
    QVERIFY(!refresh->isVisible());
}

void LeftPanelTest::refreshPicksUpDirectoryChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath("one.md"), "# one\n"));

    LeftPanel panel;
    panel.setCurrentFilePath(dir.filePath("one.md"));
    QPushButton *dirButton = buttonWithText(&panel, QStringLiteral("DIR"));
    QVERIFY(dirButton);
    dirButton->click();
    QCOMPARE(listedNames(&panel), QStringList{QStringLiteral("one.md")});

    QVERIFY(writeFile(dir.filePath("two.md"), "# two\n"));
    QVERIFY(QFile::remove(dir.filePath("one.md")));

    QPushButton *refresh = refreshButton(&panel);
    QVERIFY(refresh);
    refresh->click();

    // The explicit refresh must re-read the directory itself, without waiting
    // on the watcher (which silently sees nothing on network mounts and when
    // the inotify limit is exhausted).
    QCOMPARE(listedNames(&panel), QStringList{QStringLiteral("two.md")});
}

void LeftPanelTest::watcherPicksUpNewFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeFile(dir.filePath("one.md"), "# one\n"));

    LeftPanel panel;
    panel.setCurrentFilePath(dir.filePath("one.md"));
    QPushButton *dirButton = buttonWithText(&panel, QStringLiteral("DIR"));
    QVERIFY(dirButton);
    dirButton->click();
    QCOMPARE(listedNames(&panel), QStringList{QStringLiteral("one.md")});

    QVERIFY(writeFile(dir.filePath("two.md"), "# two\n"));
    QTRY_COMPARE_WITH_TIMEOUT(listedNames(&panel),
                              (QStringList{QStringLiteral("one.md"), QStringLiteral("two.md")}),
                              5000);
}

QTEST_MAIN(LeftPanelTest)
#include "left_panel_test.moc"
