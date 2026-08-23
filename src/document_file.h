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

    // True when the file's bytes differ from what was last read or written by
    // us — i.e. somebody else changed it. Our own saves re-baseline, so an
    // autosave never reports itself as an external change.
    bool changedOnDisk() const;

    // Accept the current bytes on disk as the new baseline. Used when the user
    // keeps their own version ("no" to a reload prompt), so the next save is
    // allowed to overwrite instead of being refused forever.
    void acceptDiskState();

    bool exists() const;

    static QString normalizedPath(const QString &path);

private:
    void captureDiskSnapshot();
    bool diskChanged() const;

    QString m_path;
    QByteArray m_diskFingerprint;
    bool m_hasDiskSnapshot = false;
};

#endif // DOCUMENT_FILE_H
