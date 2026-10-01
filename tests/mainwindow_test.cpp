#include <QtTest/QtTest>

#include "mainwindow.h"
#include "editor.h"
#include "preview_widget.h"
#include "theme.h"
#include "toggle_switch.h"

#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
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
    void reloadPullsExternalChanges();
    void keepingMyVersionUnblocksSavingAndClosing();
    void themeControlIsMonochrome();
    void findAndReplaceUpdatesMatchCount();

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

void MainWindowTest::themeControlIsMonochrome()
{
    MainWindow window;

    const auto verifyColor = [&window](const QColor &expected) {
        QCOMPARE(window.m_modeToggle->foregroundColor(), expected);
        QCOMPARE(window.m_wordLabel->palette().color(QPalette::WindowText), expected);
        QCOMPARE(window.m_sunLabel->pixmap(Qt::ReturnByValue).toImage().pixelColor(7, 7), expected);
        QCOMPARE(window.m_moonLabel->pixmap(Qt::ReturnByValue).toImage().pixelColor(3, 7), expected);
    };

    window.applyDarkMode(false, false);
    verifyColor(QColor(Theme::LightText));
    window.applyDarkMode(true, false);
    verifyColor(QColor(Theme::DarkText));
}

void MainWindowTest::findAndReplaceUpdatesMatchCount()
{
    MainWindow window;
    window.m_editor->setPlainText("alpha beta alpha");
    window.showFindBar();
    window.m_findEdit->setText("alpha");

    QCOMPARE(window.m_findCountLabel->text(), QStringLiteral("2 matches"));
    QVERIFY(window.m_editor->textCursor().hasSelection());
    QCOMPARE(window.m_editor->textCursor().selectedText(), QStringLiteral("alpha"));

    window.m_replaceEdit->setText("gamma");
    window.replaceCurrent();
    QCOMPARE(window.m_findCountLabel->text(), QStringLiteral("1 match"));
    window.replaceAll();
    QCOMPARE(window.m_editor->toPlainText(), QStringLiteral("gamma beta gamma"));
    QCOMPARE(window.m_findCountLabel->text(), QStringLiteral("0 matches"));
}

void MainWindowTest::reloadPullsExternalChanges()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("shared.md");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original\n");
    }

    MainWindow window;
    window.openFileAt(path, QStringLiteral("original\n"));
    MarkdownEditor *editor = window.m_editor;
    QVERIFY(editor);

    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("their version\n");
    }

    QVERIFY(window.reloadEditorFromDisk(editor));
    QCOMPARE(editor->toPlainText(), QStringLiteral("their version\n"));
    QVERIFY(!editor->document()->isModified());
}

void MainWindowTest::keepingMyVersionUnblocksSavingAndClosing()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("contested.md");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original\n");
    }

    MainWindow window;
    window.openFileAt(path, QStringLiteral("original\n"));
    MarkdownEditor *editor = window.m_editor;
    QVERIFY(editor);

    // Type into the buffer the way a user would: setPlainText() resets the
    // document and would clear the modified flag we are trying to set up.
    QTextCursor cursor(editor->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("my version\n"));
    QVERIFY(editor->document()->isModified());
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("their version\n");
    }

    // Until the conflict is resolved the save is refused, which is what used
    // to make the tab (and the whole app) impossible to close.
    QVERIFY(!window.flushAutosave(editor));

    // Choosing "keep mine" re-baselines, so saving and closing work again.
    window.m_documents[editor].acceptDiskState();
    QVERIFY(window.flushAutosave(editor));

    window.onTabCloseRequested(window.m_editorTabs->indexOf(editor));
    QCOMPARE(window.m_editorTabs->count(), 1);
    QVERIFY(window.m_editor);
    QVERIFY(window.filePathOfEditor(window.m_editor).isEmpty()); // fresh Untitled tab
}

QTEST_MAIN(MainWindowTest)
#include "mainwindow_test.moc"
