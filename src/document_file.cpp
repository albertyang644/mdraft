#include "document_file.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringConverter>
#include <QTextStream>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#endif

namespace {
QByteArray fingerprint(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *ok = false;
        return {};
    }
    *ok = true;
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}
}

DocumentFile::DocumentFile(const QString &path)
{
    setPath(path);
}

QString DocumentFile::path() const
{
    return m_path;
}

QString DocumentFile::normalizedPath(const QString &path)
{
    if (path.isEmpty())
        return {};

    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

void DocumentFile::setPath(const QString &path)
{
    m_path = normalizedPath(path);
    captureDiskSnapshot();
}

void DocumentFile::captureDiskSnapshot()
{
    if (m_path.isEmpty()) {
        m_diskFingerprint.clear();
        m_hasDiskSnapshot = false;
        return;
    }

    bool ok = false;
    m_diskFingerprint = fingerprint(m_path, &ok);
    m_hasDiskSnapshot = ok;
}

bool DocumentFile::changedOnDisk() const
{
    return diskChanged();
}

void DocumentFile::acceptDiskState()
{
    captureDiskSnapshot();
}

bool DocumentFile::exists() const
{
    return !m_path.isEmpty() && QFileInfo::exists(m_path);
}

bool DocumentFile::diskChanged() const
{
    if (!m_hasDiskSnapshot || m_path.isEmpty())
        return false;

    bool ok = false;
    const QByteArray current = fingerprint(m_path, &ok);
    return !ok || current != m_diskFingerprint;
}

DocumentSaveResult DocumentFile::save(const QString &content, const QString &path,
                                      bool checkExternalChanges)
{
    const QString target = normalizedPath(path);
    if (target.isEmpty())
        return {false, false, QStringLiteral("No destination path was provided.")};

    if (checkExternalChanges && target == m_path && diskChanged()) {
        return {false, true,
                QStringLiteral("The file changed on disk after it was opened:\n%1\n\n"
                               "The editor did not overwrite those external changes.")
                    .arg(target)};
    }

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return {false, false, QStringLiteral("Cannot write:\n%1\n\n%2").arg(target, file.errorString())};

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
#ifdef Q_OS_UNIX
    errno = 0;
#endif
    out << content;
    out.flush();
#ifdef Q_OS_UNIX
    const int writeErrorNumber = errno;
#endif
    if (out.status() != QTextStream::Ok) {
        QString error = file.errorString();
        bool hasNativeError = false;
#ifdef Q_OS_UNIX
        if (writeErrorNumber != 0) {
            error = QString::fromLocal8Bit(std::strerror(writeErrorNumber));
            hasNativeError = true;
        }
#endif
        if (file.error() == QFileDevice::ResourceError && !hasNativeError)
            error = QStringLiteral("The destination is out of space or does not permit a file of this size.");
        file.cancelWriting();
        return {false, false, QStringLiteral("Writing failed:\n%1\n\n%2").arg(target, error)};
    }
    if (!file.commit())
        return {false, false, QStringLiteral("Could not safely replace:\n%1\n\n%2").arg(target, file.errorString())};

    m_path = normalizedPath(target);
    captureDiskSnapshot();
    return {true, false, {}};
}
