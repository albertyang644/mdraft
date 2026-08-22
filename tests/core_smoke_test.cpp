#include <QtTest/QtTest>

#include "outline_model.h"
#include "editor.h"

class CoreSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void outlineParsesHeadings();
    void editorStatsSignalDebounced();
    void editorGoToLine();
};

void CoreSmokeTest::outlineParsesHeadings()
{
    OutlineModel model;
    const QString md =
        QStringLiteral("# Title\n"
                       "some text\n"
                       "## Sub A\n"
                       "### Sub Sub\n"
                       "plain line\n"
                       "# Another\n");
    model.setMarkdown(md);

    // Headings nest by level: Title > Sub A > Sub Sub, with Another as a
    // second top-level heading (see OutlineModel's tree-building rule).
    QCOMPARE(model.rowCount(), 2); // two top-level headings: Title, Another

    QModelIndex title = model.index(0, 0);
    QModelIndex another = model.index(1, 0);
    QCOMPARE(model.data(title, Qt::DisplayRole).toString(), QString("Title"));
    QCOMPARE(model.data(another, Qt::DisplayRole).toString(), QString("Another"));
    QCOMPARE(model.blockNumberForIndex(title), 0);
    QCOMPARE(model.blockNumberForIndex(another), 5);
    QCOMPARE(model.rowCount(another), 0);

    QCOMPARE(model.rowCount(title), 1);
    QModelIndex subA = model.index(0, 0, title);
    QCOMPARE(model.data(subA, Qt::DisplayRole).toString(), QString("Sub A"));
    QCOMPARE(model.blockNumberForIndex(subA), 2);

    QCOMPARE(model.rowCount(subA), 1);
    QModelIndex subSub = model.index(0, 0, subA);
    QCOMPARE(model.data(subSub, Qt::DisplayRole).toString(), QString("Sub Sub"));
    QCOMPARE(model.blockNumberForIndex(subSub), 3);
    QCOMPARE(model.rowCount(subSub), 0);
}

void CoreSmokeTest::editorStatsSignalDebounced()
{
    MarkdownEditor editor;
    int fired = 0;
    QObject::connect(&editor, &MarkdownEditor::contentChanged, [&]() { ++fired; });
    editor.setPlainText(QStringLiteral("one two three\nfour five"));
    // Debounce is 180 ms, so immediately after edit the signal should not have fired.
    QCOMPARE(fired, 0);
    // After the debounce window, exactly one signal should fire.
    QTest::qWait(350);
    QCOMPARE(fired, 1);
}

void CoreSmokeTest::editorGoToLine()
{
    MarkdownEditor editor;
    editor.setPlainText(QStringLiteral("line0\nline1\nline2\nline3"));
    editor.goToLine(2);
    // Cursor should now be on block 2 (0-based).
    QCOMPARE(editor.textCursor().blockNumber(), 2);
}

QTEST_MAIN(CoreSmokeTest)
#include "core_smoke_test.moc"
