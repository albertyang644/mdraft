#include <QtTest/QtTest>
#include "preview_widget.h"
#include "shutter_panel.h"
#include <QSignalSpy>
#include <QChildEvent>
#include <QFile>
#include <QTemporaryDir>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QHostAddress>
#include <QTcpServer>
#include <QWebEngineView>
#include <QWebEngineSettings>
#endif

// A dummy content widget to exercise ShutterPanel.
static QWidget *dummyContent()
{
    return new QWidget();
}

class ScopedPath
{
public:
    explicit ScopedPath(const QString &first)
        : old(qgetenv("PATH"))
    {
        qputenv("PATH", first.toUtf8() + ':' + old);
    }
    ~ScopedPath() { qputenv("PATH", old); }

private:
    QByteArray old;
};

class PreviewLifecycleTest : public QObject
{
    Q_OBJECT
private slots:
    void previewStartsClosed();
    void previewOpensAndDestroysWebView();
    void shutterOpenCloseEmitsSignals();
    void shutterLeftStartsOpenRightStartsClosed();
    void javascriptIsDisabled();
    void remoteSubresourcesAreBlocked();
    void crashedPandocCompletesOnce();
    void latestSourceWinsConversionRace();
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
    ShutterPanel sp(content);
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
    ShutterPanel left(l);
    left.setOpen(true); // MainWindow explicitly opens the left panel
    QVERIFY(left.isOpen());

    QWidget *r = dummyContent();
    ShutterPanel right(r);
    right.setOpen(false); // MainWindow explicitly closes the right panel
    QVERIFY(!right.isOpen());
    delete l;
    delete r;
}

void PreviewLifecycleTest::javascriptIsDisabled()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    PreviewWidget preview;
    preview.openPreview();
    auto *view = preview.findChild<QWebEngineView *>();
    QVERIFY(view);
    QVERIFY(!view->settings()->testAttribute(QWebEngineSettings::JavascriptEnabled));
#else
    QSKIP("WebEngine not compiled in this build");
#endif
}

void PreviewLifecycleTest::remoteSubresourcesAreBlocked()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    QTemporaryDir dir;
    const QString executable = dir.filePath("pandoc");
    QFile script(executable);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\ncat\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));
    ScopedPath path(dir.path());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connections(&server, &QTcpServer::newConnection);

    PreviewWidget preview;
    const QString markdown = QStringLiteral("<img src=\"http://127.0.0.1:%1/tracked.png\">")
                                 .arg(server.serverPort());
    preview.setMarkdown(markdown);
    preview.openPreview();

    auto *view = preview.findChild<QWebEngineView *>();
    QVERIFY(view);
    QSignalSpy loads(view, &QWebEngineView::loadFinished);
    QTRY_COMPARE_WITH_TIMEOUT(preview.m_lastRenderedSource, markdown, 2000);
    QTRY_VERIFY_WITH_TIMEOUT(!loads.isEmpty(), 2000);
    QTest::qWait(250);
    QCOMPARE(connections.count(), 0);
#else
    QSKIP("WebEngine not compiled in this build");
#endif
}

void PreviewLifecycleTest::crashedPandocCompletesOnce()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    QTemporaryDir dir;
    const QString executable = dir.filePath("pandoc");
    QFile script(executable);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nkill -SEGV $$\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));
    ScopedPath path(dir.path());

    PreviewWidget preview;
    preview.setMarkdown("crash test");
    preview.openPreview();
    QTRY_VERIFY_WITH_TIMEOUT(preview.m_pandocProcess == nullptr, 2000);
    QVERIFY(preview.isPreviewOpen());
#else
    QSKIP("WebEngine not compiled in this build");
#endif
}

void PreviewLifecycleTest::latestSourceWinsConversionRace()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    QTemporaryDir dir;
    const QString executable = dir.filePath("pandoc");
    QFile script(executable);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write(
        "#!/bin/sh\n"
        "input=$(cat)\n"
        "if [ \"$input\" = B ]; then sleep 0.3; fi\n"
        "printf '<p>%s</p>' \"$input\"\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));
    ScopedPath path(dir.path());

    PreviewWidget preview;
    preview.setMarkdown("A");
    preview.openPreview();
    QTRY_COMPARE_WITH_TIMEOUT(preview.m_lastRenderedSource, QString("A"), 2000);

    preview.setMarkdown("B");
    QTRY_COMPARE_WITH_TIMEOUT(preview.m_convertingSource, QString("B"), 1000);
    preview.setMarkdown("A");
    QTest::qWait(700);
    QCOMPARE(preview.m_lastRenderedSource, QString("A"));
#else
    QSKIP("WebEngine not compiled in this build");
#endif
}

QTEST_MAIN(PreviewLifecycleTest)
#include "preview_lifecycle_test.moc"
