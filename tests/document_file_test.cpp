#include <QtTest/QtTest>

#include "document_file.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <csignal>
#include <cstdio>
#include <sys/resource.h>
#endif

namespace {
bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
}

class DocumentFileTest : public QObject
{
    Q_OBJECT

private slots:
    void savesAtomically();
    void preservesOriginalWhenReplacementCannotStart();
    void preservesDeviceErrorOnMidWriteFailure();
    void refusesExternalChanges();
    void canonicalizesAliases();
};

void DocumentFileTest::savesAtomically()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("document.md");
    QVERIFY(writeBytes(path, "old"));
    DocumentFile document(path);

    const DocumentSaveResult result = document.save("new contents", path, true);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(readBytes(path), QByteArray("new contents"));
}

void DocumentFileTest::preservesOriginalWhenReplacementCannotStart()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("document.md");
    QVERIFY(writeBytes(path, "original"));
    DocumentFile document(path);

    const QFileDevice::Permissions originalPermissions = QFile::permissions(dir.path());
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const DocumentSaveResult result = document.save("replacement", path, false);
    QVERIFY(QFile::setPermissions(dir.path(), originalPermissions));

    QVERIFY(!result.ok);
    QCOMPARE(readBytes(path), QByteArray("original"));
}

void DocumentFileTest::preservesDeviceErrorOnMidWriteFailure()
{
#ifdef Q_OS_UNIX
    QTemporaryDir dir;
    const QString path = dir.filePath("document.md");
    QVERIFY(writeBytes(path, "original"));

    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--efbig-child"), path});
    QVERIFY(child.waitForFinished(5000));
    QCOMPARE(child.exitStatus(), QProcess::NormalExit);
    QCOMPARE(child.exitCode(), 0);

    const QString error = QString::fromUtf8(child.readAllStandardOutput());
    QVERIFY2(error.contains(QStringLiteral("File too large"), Qt::CaseInsensitive), qPrintable(error));
    QVERIFY2(!error.contains(QStringLiteral("canceled"), Qt::CaseInsensitive), qPrintable(error));
    QCOMPARE(readBytes(path), QByteArray("original"));
#else
    QSKIP("RLIMIT_FSIZE is unavailable on this platform");
#endif
}

void DocumentFileTest::refusesExternalChanges()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("document.md");
    QVERIFY(writeBytes(path, "opened version"));
    DocumentFile document(path);
    QVERIFY(writeBytes(path, "external version"));

    const DocumentSaveResult result = document.save("editor version", path, true);
    QVERIFY(!result.ok);
    QVERIFY(result.externalConflict);
    QCOMPARE(readBytes(path), QByteArray("external version"));
}

void DocumentFileTest::canonicalizesAliases()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("document.md");
    const QString alias = dir.filePath("alias.md");
    QVERIFY(writeBytes(path, "text"));
    QVERIFY(QFile::link(path, alias));
    QCOMPARE(DocumentFile::normalizedPath(path), DocumentFile::normalizedPath(alias));
}

int main(int argc, char **argv)
{
#ifdef Q_OS_UNIX
    if (argc == 3 && QByteArray(argv[1]) == QByteArrayLiteral("--efbig-child")) {
        std::signal(SIGXFSZ, SIG_IGN);
        const rlimit limit{1024, 1024};
        if (setrlimit(RLIMIT_FSIZE, &limit) != 0)
            return 2;

        const QString path = QFile::decodeName(argv[2]);
        DocumentFile document(path);
        const DocumentSaveResult result = document.save(QString(1024 * 1024, QLatin1Char('x')),
                                                        path, false);
        const QByteArray error = result.error.toUtf8();
        std::fwrite(error.constData(), 1, static_cast<size_t>(error.size()), stdout);
        return result.ok ? 3 : 0;
    }
#endif

    QCoreApplication app(argc, argv);
    DocumentFileTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "document_file_test.moc"
