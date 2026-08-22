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
    m_parentOf.clear();
    m_children.clear();
    m_rootChildren.clear();
    m_rowInParent.clear();

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

    const int n = m_entries.size();
    m_parentOf.fill(-1, n);
    m_children = QVector<QVector<int>>(n);
    m_rowInParent.fill(0, n);

    // Build the nesting with a stack of ancestors: each heading nests under
    // the nearest preceding heading with a strictly lower level.
    QVector<int> stack;
    for (int i = 0; i < n; ++i) {
        const int level = m_entries.at(i).level;
        while (!stack.isEmpty() && m_entries.at(stack.last()).level >= level)
            stack.removeLast();

        if (stack.isEmpty()) {
            m_rowInParent[i] = m_rootChildren.size();
            m_rootChildren.append(i);
        } else {
            const int p = stack.last();
            m_parentOf[i] = p;
            m_rowInParent[i] = m_children[p].size();
            m_children[p].append(i);
        }
        stack.append(i);
    }

    endResetModel();
}

QModelIndex OutlineModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row >= m_rootChildren.size())
            return QModelIndex();
        return createIndex(row, column, static_cast<quintptr>(m_rootChildren.at(row)));
    }

    const int parentEntry = static_cast<int>(parent.internalId());
    const auto &kids = m_children.at(parentEntry);
    if (row >= kids.size())
        return QModelIndex();
    return createIndex(row, column, static_cast<quintptr>(kids.at(row)));
}

QModelIndex OutlineModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    const int entry = static_cast<int>(child.internalId());
    const int parentEntry = m_parentOf.at(entry);
    if (parentEntry < 0)
        return QModelIndex();
    return createIndex(m_rowInParent.at(parentEntry), 0, static_cast<quintptr>(parentEntry));
}

int OutlineModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_rootChildren.size();
    if (parent.column() != 0)
        return 0;
    const int entry = static_cast<int>(parent.internalId());
    return m_children.at(entry).size();
}

int OutlineModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant OutlineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    const int entry = static_cast<int>(index.internalId());
    if (entry < 0 || entry >= m_entries.size())
        return QVariant();

    const OutlineEntry &e = m_entries.at(entry);
    switch (role) {
    case Qt::DisplayRole:
        return e.text;
    case Qt::UserRole:
        return e.level;
    default:
        return QVariant();
    }
}

int OutlineModel::blockNumberForIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return -1;
    const int entry = static_cast<int>(index.internalId());
    if (entry < 0 || entry >= m_entries.size())
        return -1;
    return m_entries.at(entry).blockNumber;
}
