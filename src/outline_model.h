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

// Headings are nested by level (an H2 nests under the nearest preceding H1,
// an H3 under the nearest preceding H2 or H1, etc.), so the outline reads
// like Ghostwriter's — indented by document structure, not a flat list.
class OutlineModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit OutlineModel(QObject *parent = nullptr);

    // Rebuild the outline from raw markdown source text.
    void setMarkdown(const QString &markdown);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    int blockNumberForIndex(const QModelIndex &index) const;

private:
    QVector<OutlineEntry> m_entries;
    QVector<int> m_parentOf;          // entry index -> parent entry index, -1 for root
    QVector<QVector<int>> m_children; // entry index -> child entry indices
    QVector<int> m_rootChildren;      // top-level entry indices
    QVector<int> m_rowInParent;       // entry index -> row among its siblings
};

#endif // OUTLINE_MODEL_H
