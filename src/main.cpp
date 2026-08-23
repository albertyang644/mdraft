#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QTextStream>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
#ifdef MDRAFT_HAVE_WEBENGINE
    // HARD_CONTRACT: no WebView may exist at startup. Initializing the profile
    // is not the same as creating a WebView; we only request the features that
    // will be needed later if the preview panel is opened.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("mdraft");
    QCoreApplication::setOrganizationName("mdraft");
    QCoreApplication::setApplicationVersion("0.1.0");
    app.setWindowIcon(QIcon(":/icons/mdraft.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("mdraft - a fast native Markdown editor");
    parser.addHelpOption();
    QCommandLineOption fileOpt(QStringList() << "f" << "file",
                               "Open the given Markdown file.", "file");
    parser.addOption(fileOpt);
    parser.addPositionalArgument("files", "Markdown files to open (e.g. from a file manager's \"Open with\").", "[files...]");
    parser.process(app);

    // File managers invoke "Open with" as `mdraft /path/to/file.md` (a bare
    // positional argument, per the desktop entry's %F), not via -f/--file.
    QStringList filePaths;
    if (parser.isSet(fileOpt))
        filePaths.append(parser.value(fileOpt));
    for (const QString &path : parser.positionalArguments()) {
        if (!filePaths.contains(path))
            filePaths.append(path);
    }

    MainWindow w;
    w.show();

    for (const QString &filePath : filePaths) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            w.openFileAt(filePath, in.readAll());
        } else {
            QMessageBox::warning(&w, QObject::tr("Open"),
                QObject::tr("Cannot open file:\n%1\n\n%2").arg(filePath, f.errorString()));
        }
    }

    return app.exec();
}
