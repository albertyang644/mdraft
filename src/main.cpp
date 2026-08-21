#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
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

    QCommandLineParser parser;
    parser.setApplicationDescription("mdraft - a fast native Markdown editor");
    parser.addHelpOption();
    QCommandLineOption fileOpt(QStringList() << "f" << "file",
                               "Open the given Markdown file.", "file");
    parser.addOption(fileOpt);
    parser.process(app);

    MainWindow w;
    w.show();

    // Optionally open a file passed on the command line.
    if (parser.isSet(fileOpt)) {
        QMetaObject::invokeMethod(&w, [&w, path = parser.value(fileOpt)]() {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&f);
                w.openFileAt(path, in.readAll());
            }
        }, Qt::QueuedConnection);
    }

    return app.exec();
}
