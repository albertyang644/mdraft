#include <QtTest/QtTest>

#include "mainwindow.h"
#include "editor.h"
#include "preview_widget.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEngineView>
#endif

class MainWindowTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void closeLastTabCreatesSafeReplacement();
    void startupNeverCreatesWebView();
    void canonicalPathsReuseTab();
    void failedSaveDoesNotAdoptPath();

private:
    QTemporaryDir m_settingsDir;
};

void MainWindowTest::initTestCase()
{
    QVERIFY(m_settingsDir.isValid());
    QCoreApplication::setOrganizationName("mdraft-tests");
    QCoreApplication::setApplicationName("mainwindow-test");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
}

void MainWindowTest::closeLastTabCreatesSafeReplacement()
{
    MainWindow window;
    QCOMPARE(window.m_editorTabs->count(), 1);

    window.onTabCloseRequested(0);

    QCOMPARE(window.m_editorTabs->count(), 1);
    QVERIFY(window.m_editor);
    QCOMPARE(window.m_editor.data(), window.m_editorTabs->currentWidget());
}

void MainWindowTest::startupNeverCreatesWebView()
{
    QSettings().setValue("alwaysOpenWebview", true); // Legacy setting must be ignored.
    QElapsedTimer elapsed;
    elapsed.start();
    MainWindow window;
    QVERIFY2(elapsed.elapsed() < 1500, "Native MainWindow construction exceeded 1.5 seconds");
    QVERIFY(!window.m_preview->isPreviewOpen());
#ifdef MDRAFT_HAVE_WEBENGINE
    QCOMPARE(window.findChildren<QWebEngineView *>().size(), 0);
#endif
}

void MainWindowTest::canonicalPathsReuseTab()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString realPath = dir.filePath("document.md");
    QFile file(realPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("# One\n"), 6);
    file.close();

    const QString aliasPath = dir.filePath("alias.md");
    QVERIFY(QFile::link(realPath, aliasPath));

    MainWindow window;
    window.openFileAt(realPath, "# One\n");
    window.openFileAt(aliasPath, "# One\n");
    QCOMPARE(window.m_editorTabs->count(), 1);
}

void MainWindowTest::failedSaveDoesNotAdoptPath()
{
    MainWindow window;
    window.m_editor->setPlainText("important");
    window.m_editor->document()->setModified(true);

    QVERIFY(!window.saveEditorToPath(window.m_editor, "/proc/mdraft-cannot-write.md",
                                     false, false));
    QVERIFY(window.filePathOfEditor(window.m_editor).isEmpty());
    QVERIFY(window.m_editor->document()->isModified());
}

QTEST_MAIN(MainWindowTest)
#include "mainwindow_test.moc"
