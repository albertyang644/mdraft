#include "exporter.h"
#include "markdown_html.h"

#include <QProcess>
#include <QFile>
#include <QTextStream>

QString Exporter::runProcess(const QString &program, const QStringList &args, bool *ok, QString *err)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForStarted(3000)) {
        if (ok) *ok = false;
        if (err) *err = QString("Failed to start '%1'. Is it installed?").arg(program);
        return QString();
    }
    if (!p.waitForFinished(60000)) {
        p.kill();
        if (ok) *ok = false;
        if (err) *err = QString("'%1' timed out.").arg(program);
        return QString();
    }
    QByteArray out = p.readAllStandardOutput();
    QByteArray cerr = p.readAllStandardError();
    if (p.exitCode() != 0) {
        if (ok) *ok = false;
        if (err) *err = QString("'%1' failed: %2").arg(program, QString::fromUtf8(cerr));
        return QString();
    }
    if (ok) *ok = true;
    if (err) err->clear();
    return QString::fromUtf8(out);
}

bool Exporter::exportTo(const QString &srcPath, const QString &dstPath, const QString &format, QString &error)
{
    bool ok = false;
    QStringList args;
    args << srcPath << "-o" << dstPath;

    if (format == "html") {
        // Convert to a bare HTML fragment ourselves, then wrap it with the
        // same CSS the live preview uses, so the exported file isn't just
        // unstyled pandoc paragraphs dumped in a browser.
        const QString body = runProcess("pandoc", {"--from=gfm", "--to=html", srcPath}, &ok, &error);
        if (!ok)
            return false;

        QFile f(dstPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            error = QString("Cannot write:\n%1").arg(dstPath);
            return false;
        }
        QTextStream out(&f);
        out << wrapMarkdownHtml(body, /*dark=*/false);
        return true;
    } else if (format == "pdf") {
        // Let pandoc pick a LaTeX engine (or try pdflatex).
        args.prepend("--pdf-engine=pdflatex");
        runProcess("pandoc", args, &ok, &error);
        return ok;
    } else if (format == "latex") {
        runProcess("pandoc", args, &ok, &error);
        return ok;
    }

    error = QString("Unsupported export format '%1'").arg(format);
    return false;
}

bool Exporter::htmlToPdf(const QString &html, const QString &dstPath, QString &error)
{
    Q_UNUSED(html);
    Q_UNUSED(dstPath);
    Q_UNUSED(error);
    // Full WebEngine-based HTML->PDF is implemented in the MainWindow when
    // WebEngine is available (QWebEnginePage::printToPdf). This stub exists so
    // the call path is uniform; it is replaced by the WebEngine implementation.
    error = "WebEngine PDF path not configured.";
    return false;
}
