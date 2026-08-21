#ifndef OUTLINE_MODEL_H
#define OUTLINE_MODEL_H

#include <QAbstractItemModel>
#include <QVector>
#include <QString>

// A single generated outline entry.
struct OutlineEntry {
    QString text;
    int level;                 // 1..6
    int blockNumber;           // 0-based paragraph/block index in the editor
};

class OutlineModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit OutlineModel(QObject *parent = nullptr);

    // Rebuild the outline from raw markdown source text.
    void setMarkdown(const QString &markdown);

    // Model interface (flat list of headings).
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    int blockNumberAt(int row) const;

private:
    QVector<OutlineEntry> m_entries;
};

#endif // OUTLINE_MODEL_H
