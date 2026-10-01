#include "exporter.h"
#include "markdown_html.h"

#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSharedPointer>
#include <QStringConverter>
#include <QTextStream>
#include <QTemporaryFile>
#include <QTimer>

void Exporter::exportMarkdown(const QString &markdown, const QString &dstPath,
                              const QString &format, QObject *context,
                              Completion completion)
{
    if (format != "html" && format != "pdf" && format != "latex"
        && format != "docx") {
        completion(false, QString("Unsupported export format '%1'").arg(format));
        return;
    }

    QString temporaryOutput;
    if (format != "html") {
        const QString extension = format == "pdf" ? ".pdf"
                                : format == "docx" ? ".docx" : ".tex";
        QTemporaryFile temporary(
            QFileInfo(dstPath).dir().filePath(".mdraft-export-XXXXXX" + extension));
        temporary.setAutoRemove(false);
        if (!temporary.open()) {
            completion(false, QStringLiteral("Cannot create a temporary export file next to:\n%1")
                                  .arg(dstPath));
            return;
        }
        temporaryOutput = temporary.fileName();
        temporary.close();
    }

    auto *process = new QProcess(context);
    QObject::connect(process, &QObject::destroyed, [temporaryOutput]() {
        if (!temporaryOutput.isEmpty())
            QFile::remove(temporaryOutput);
    });
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    timeout->setInterval(60000);
    const auto completed = QSharedPointer<bool>::create(false);

    auto finish = [process, completed, completion, temporaryOutput](bool ok, const QString &error) {
        if (*completed)
            return;
        *completed = true;
        process->disconnect();
        if (!temporaryOutput.isEmpty())
            QFile::remove(temporaryOutput);
        process->deleteLater();
        completion(ok, error);
    };

    QObject::connect(timeout, &QTimer::timeout, context, [process, finish]() {
        process->kill();
        finish(false, QStringLiteral("'pandoc' timed out after 60 seconds."));
    });
    QObject::connect(process, &QProcess::errorOccurred, context,
            [finish](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(false, QStringLiteral("Failed to start 'pandoc'. Is it installed?"));
    });
    QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), context,
            [process, dstPath, temporaryOutput, format, finish](int exitCode, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString detail = QString::fromUtf8(process->readAllStandardError()).trimmed();
            finish(false, detail.isEmpty() ? QStringLiteral("'pandoc' failed.")
                                            : QStringLiteral("'pandoc' failed: %1").arg(detail));
            return;
        }

        if (format == "html") {
            QSaveFile file(dstPath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                finish(false, QStringLiteral("Cannot write:\n%1\n\n%2")
                                  .arg(dstPath, file.errorString()));
                return;
            }
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << wrapMarkdownHtml(QString::fromUtf8(process->readAllStandardOutput()), false);
            out.flush();
            if (out.status() != QTextStream::Ok || !file.commit()) {
                finish(false, QStringLiteral("Could not safely write:\n%1\n\n%2")
                                  .arg(dstPath, file.errorString()));
                return;
            }
        } else {
            QFile source(temporaryOutput);
            if (!source.open(QIODevice::ReadOnly)) {
                finish(false, QStringLiteral("Cannot read the completed export:\n%1")
                                  .arg(source.errorString()));
                return;
            }
            const QByteArray bytes = source.readAll();
            QSaveFile destination(dstPath);
            if (!destination.open(QIODevice::WriteOnly)
                || destination.write(bytes) != bytes.size()
                || !destination.commit()) {
                finish(false, QStringLiteral("Could not safely write:\n%1\n\n%2")
                                  .arg(dstPath, destination.errorString()));
                return;
            }
        }
        finish(true, {});
    });

    QStringList args{"--from=gfm"};
    if (format == "html")
        args << "--to=html";
    else if (format == "pdf")
        args << "--pdf-engine=xelatex"
             // DejaVu Sans covers common Unicode symbols and many emoji;
             // XeLaTeX completes the document even when a rare glyph is not
             // available, instead of pdflatex aborting on the code point.
             << "--variable" << "mainfont=DejaVu Sans"
             << "--output" << temporaryOutput;
    else if (format == "docx")
        args << "--to=docx" << "--output" << temporaryOutput;
    else
        args << "--to=latex" << "--output" << temporaryOutput;

    process->start(QStringLiteral("pandoc"), args);
    process->write(markdown.toUtf8());
    process->closeWriteChannel();
    timeout->start();
}
