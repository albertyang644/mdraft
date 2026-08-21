#include "outline_model.h"

#include <QIcon>

OutlineModel::OutlineModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void OutlineModel::setMarkdown(const QString &markdown)
{
    beginResetModel();
    m_entries.clear();

    const QStringList lines = markdown.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        int hashes = 0;
        while (hashes < line.size() && line.at(hashes) == '#')
            hashes++;
        if (hashes > 0 && hashes <= 6) {
            // A heading is only valid if a space follows the hashes (or it's the whole line).
            if (hashes < line.size() && line.at(hashes) != ' ')
                continue;
            QString text = line.mid(hashes).trimmed();
            if (text.isEmpty())
                continue;
            OutlineEntry e;
            e.text = text;
            e.level = hashes;
            e.blockNumber = i;
            m_entries.append(e);
        }
    }
    endResetModel();
}

QModelIndex OutlineModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || row < 0 || row >= m_entries.size() || column != 0)
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex OutlineModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex(); // flat list
}

int OutlineModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int OutlineModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant OutlineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();

    const OutlineEntry &e = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return e.text;
    case Qt::UserRole:
        return e.level;
    default:
        return QVariant();
    }
}

int OutlineModel::blockNumberAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return -1;
    return m_entries.at(row).blockNumber;
}
