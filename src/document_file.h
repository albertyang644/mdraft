#ifndef DOCUMENT_FILE_H
#define DOCUMENT_FILE_H

#include <QByteArray>
#include <QString>

struct DocumentSaveResult
{
    bool ok = false;
    bool externalConflict = false;
    QString error;
};

class DocumentFile
{
public:
    DocumentFile() = default;
    explicit DocumentFile(const QString &path);

    QString path() const;
    void setPath(const QString &path);

    DocumentSaveResult save(const QString &content, const QString &path,
                            bool checkExternalChanges);

    static QString normalizedPath(const QString &path);

private:
    void captureDiskSnapshot();
    bool diskChanged() const;

    QString m_path;
    QByteArray m_diskFingerprint;
    bool m_hasDiskSnapshot = false;
};

#endif // DOCUMENT_FILE_H
