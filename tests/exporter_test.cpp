#include <QtTest/QtTest>

#include "exporter.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

class ExporterTest : public QObject
{
    Q_OBJECT

private slots:
    void exportsCurrentBufferAsStandaloneGfmHtml();
    void usesGfmForLatex();
    void exportsCommonUnicodeMathToPdf();
    void conversionDoesNotBlockEventLoop();
    void reportsPandocFailure();
};

void ExporterTest::exportsCurrentBufferAsStandaloneGfmHtml()
{
    if (QStandardPaths::findExecutable("pandoc").isEmpty())
        QSKIP("pandoc is not installed");

    QTemporaryDir dir;
    const QString output = dir.filePath("output.html");
    QEventLoop loop;
    bool ok = false;
    QString error;
    Exporter::exportMarkdown("| A | B |\n|---|---|\n| 1 | 2 |\n", output, "html", this,
        [&](bool succeeded, const QString &message) {
            ok = succeeded;
            error = message;
            loop.quit();
        });
    loop.exec();
    QVERIFY2(ok, qPrintable(error));

    QFile file(output);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray html = file.readAll();
    QVERIFY(html.startsWith("<!DOCTYPE html>"));
    QVERIFY(html.contains("<table>"));
}

void ExporterTest::usesGfmForLatex()
{
    if (QStandardPaths::findExecutable("pandoc").isEmpty())
        QSKIP("pandoc is not installed");

    QTemporaryDir dir;
    const QString output = dir.filePath("output.tex");
    QEventLoop loop;
    bool ok = false;
    QString error;
    Exporter::exportMarkdown("www.example.com\n", output, "latex", this,
        [&](bool succeeded, const QString &message) {
            ok = succeeded;
            error = message;
            loop.quit();
        });
    loop.exec();
    QVERIFY2(ok, qPrintable(error));

    QFile file(output);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QVERIFY(file.readAll().contains("\\href{http://www.example.com}{www.example.com}"));
}

void ExporterTest::exportsCommonUnicodeMathToPdf()
{
    if (QStandardPaths::findExecutable("pandoc").isEmpty()
        || QStandardPaths::findExecutable("pdflatex").isEmpty()) {
        QSKIP("pandoc and pdflatex are required");
    }

    QTemporaryDir dir;
    const QString output = dir.filePath("unicode-math.pdf");
    QEventLoop loop;
    bool ok = false;
    QString error;
    const QString markdown = QString::fromUtf8(
        "Tyler comp: 794.9k ÷ 6,126 sq ft ≈ 129.76\n"
        "Common comparisons: ≠ ≤ ≥\n");

    Exporter::exportMarkdown(markdown, output, "pdf", this,
        [&](bool succeeded, const QString &message) {
            ok = succeeded;
            error = message;
            loop.quit();
        });
    loop.exec();

    QVERIFY2(ok, qPrintable(error));
    QFileInfo pdf(output);
    QVERIFY(pdf.exists());
    QVERIFY(pdf.size() > 0);
}

void ExporterTest::conversionDoesNotBlockEventLoop()
{
    QTemporaryDir dir;
    const QString fakePandoc = dir.filePath("pandoc");
    QFile script(fakePandoc);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nsleep 0.2\nprintf '<p>done</p>'\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));

    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", dir.path().toUtf8() + ':' + oldPath);
    QEventLoop loop;
    bool timerFired = false;
    bool exported = false;
    QTimer::singleShot(20, this, [&]() { timerFired = true; });
    Exporter::exportMarkdown("text", dir.filePath("output.html"), "html", this,
        [&](bool ok, const QString &) {
            exported = ok;
            loop.quit();
        });
    loop.exec();
    qputenv("PATH", oldPath);

    QVERIFY(exported);
    QVERIFY(timerFired);
}

void ExporterTest::reportsPandocFailure()
{
    QTemporaryDir dir;
    const QString fakePandoc = dir.filePath("pandoc");
    QFile script(fakePandoc);
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\necho conversion-failed >&2\nexit 7\n");
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ExeOwner));

    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", dir.path().toUtf8() + ':' + oldPath);
    QEventLoop loop;
    bool exported = true;
    QString error;
    Exporter::exportMarkdown("text", dir.filePath("output.pdf"), "pdf", this,
        [&](bool ok, const QString &message) {
            exported = ok;
            error = message;
            loop.quit();
        });
    loop.exec();
    qputenv("PATH", oldPath);

    QVERIFY(!exported);
    QVERIFY(error.contains("conversion-failed"));
    QVERIFY(!QFileInfo::exists(dir.filePath("output.pdf")));
}

QTEST_GUILESS_MAIN(ExporterTest)
#include "exporter_test.moc"
