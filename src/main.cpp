#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include "mainwindow.h"

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#endif

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
    parser.addPositionalArgument("file", "Markdown file to open (e.g. from a file manager's \"Open with\").", "[file]");
    parser.process(app);

    // File managers invoke "Open with" as `mdraft /path/to/file.md` (a bare
    // positional argument, per the desktop entry's %F), not via -f/--file.
    QString filePath;
    if (parser.isSet(fileOpt))
        filePath = parser.value(fileOpt);
    else if (!parser.positionalArguments().isEmpty())
        filePath = parser.positionalArguments().first();

    MainWindow w;
    w.show();

    if (!filePath.isEmpty()) {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            w.openFileAt(filePath, in.readAll());
        }
    }

    return app.exec();
}
