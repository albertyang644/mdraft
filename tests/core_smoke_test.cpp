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

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(), QString("Title"));
    QCOMPARE(model.data(model.index(1, 0), Qt::DisplayRole).toString(), QString("Sub A"));
    QCOMPARE(model.data(model.index(2, 0), Qt::DisplayRole).toString(), QString("Sub Sub"));
    QCOMPARE(model.data(model.index(3, 0), Qt::DisplayRole).toString(), QString("Another"));

    // Block numbers map to source lines.
    QCOMPARE(model.blockNumberAt(0), 0);
    QCOMPARE(model.blockNumberAt(1), 2);
    QCOMPARE(model.blockNumberAt(2), 3);
    QCOMPARE(model.blockNumberAt(3), 5);
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
