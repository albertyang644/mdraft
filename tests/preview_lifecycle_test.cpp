#include <QtTest/QtTest>
#include "preview_widget.h"
#include "shutter_panel.h"
#include <QSignalSpy>
#include <QChildEvent>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEngineView>
#endif

// A dummy content widget to exercise ShutterPanel.
static QWidget *dummyContent()
{
    return new QWidget();
}

class PreviewLifecycleTest : public QObject
{
    Q_OBJECT
private slots:
    void previewStartsClosed();
    void previewOpensAndDestroysWebView();
    void shutterOpenCloseEmitsSignals();
    void shutterLeftStartsOpenRightStartsClosed();
};

void PreviewLifecycleTest::previewStartsClosed()
{
    PreviewWidget pw;
    QVERIFY(!pw.isPreviewOpen()); // HARD_CONTRACT: no WebView before open
}

void PreviewLifecycleTest::previewOpensAndDestroysWebView()
{
    PreviewWidget pw;
    QVERIFY(!pw.isPreviewOpen());

    pw.setMarkdown("# Hello\n\n*some markdown*");
    pw.openPreview();

#ifdef MDRAFT_HAVE_WEBENGINE
    QVERIFY(pw.isPreviewOpen());   // WebView was created
    QCOMPARE(pw.findChildren<QWebEngineView *>().size(), 1);
#else
    QSKIP("WebEngine not compiled in this build");
#endif

    pw.teardownPreview();

#ifdef MDRAFT_HAVE_WEBENGINE
    QVERIFY(!pw.isPreviewOpen());  // WebView destroyed on close (HARD_CONTRACT)
    QCOMPARE(pw.findChildren<QWebEngineView *>().size(), 0);
#endif
    QVERIFY(!pw.isPreviewOpen());
}

void PreviewLifecycleTest::shutterOpenCloseEmitsSignals()
{
    QWidget *content = dummyContent();
    ShutterPanel sp(content, 1); // right side
    sp.setOpen(false);           // start closed
    QSignalSpy opened(&sp, &ShutterPanel::opened);
    QSignalSpy closed(&sp, &ShutterPanel::closed);

    sp.setOpen(true);
    QCOMPARE(opened.count(), 1);
    QCOMPARE(sp.isOpen(), true);

    sp.setOpen(false);
    QCOMPARE(closed.count(), 1);
    QCOMPARE(sp.isOpen(), false);
    delete content;
}

void PreviewLifecycleTest::shutterLeftStartsOpenRightStartsClosed()
{
    // Left panel default should be open; right panel should be closed on launch.
    QWidget *l = dummyContent();
    ShutterPanel left(l, 0);
    left.setOpen(true); // MainWindow explicitly opens the left panel
    QVERIFY(left.isOpen());

    QWidget *r = dummyContent();
    ShutterPanel right(r, 1);
    right.setOpen(false); // MainWindow explicitly closes the right panel
    QVERIFY(!right.isOpen());
    delete l;
    delete r;
}

QTEST_MAIN(PreviewLifecycleTest)
#include "preview_lifecycle_test.moc"
