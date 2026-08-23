#include "outline_model.h"

#include <QRegularExpression>

namespace {
QVector<OutlineEntry> parseHeadings(const QString &markdown)
{
    static const QRegularExpression fenceRe(R"(^ {0,3}(`{3,}|~{3,}))");
    static const QRegularExpression atxRe(R"(^ {0,3}(#{1,6})(?:[ \t]+(.*)|[ \t]*)$)");
    static const QRegularExpression closingHashesRe(R"([ \t]+#+[ \t]*$)");
    static const QRegularExpression setextRe(R"(^ {0,3}(=+|-+)[ \t]*$)");

    QVector<OutlineEntry> entries;
    const QStringList lines = markdown.split('\n');
    QChar fenceMarker;
    int fenceLength = 0;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        const QRegularExpressionMatch fence = fenceRe.match(line);
        if (!fenceMarker.isNull()) {
            if (fence.hasMatch() && fence.captured(1).at(0) == fenceMarker
                && fence.capturedLength(1) >= fenceLength
                && line.mid(fence.capturedEnd(1)).trimmed().isEmpty()) {
                fenceMarker = {};
                fenceLength = 0;
            }
            continue;
        }
        if (fence.hasMatch()) {
            fenceMarker = fence.captured(1).at(0);
            fenceLength = static_cast<int>(fence.capturedLength(1));
            continue;
        }

        const QRegularExpressionMatch setext = setextRe.match(line);
        if (setext.hasMatch() && i > 0) {
            const QString text = lines.at(i - 1).trimmed();
            if (!text.isEmpty() && !atxRe.match(lines.at(i - 1)).hasMatch())
                entries.append({text, setext.captured(1).at(0) == '=' ? 1 : 2, i - 1});
            continue;
        }

        const QRegularExpressionMatch atx = atxRe.match(line);
        if (!atx.hasMatch())
            continue;

        QString text = atx.captured(2).trimmed();
        text.remove(closingHashesRe);
        text = text.trimmed();
        if (!text.isEmpty())
            entries.append({text, static_cast<int>(atx.capturedLength(1)), i});
    }
    return entries;
}

bool sameEntries(const QVector<OutlineEntry> &left, const QVector<OutlineEntry> &right)
{
    if (left.size() != right.size())
        return false;
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i).text != right.at(i).text
            || left.at(i).level != right.at(i).level
            || left.at(i).blockNumber != right.at(i).blockNumber) {
            return false;
        }
    }
    return true;
}
}

OutlineModel::OutlineModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void OutlineModel::setMarkdown(const QString &markdown)
{
    const QVector<OutlineEntry> parsed = parseHeadings(markdown);
    if (sameEntries(parsed, m_entries))
        return;

    beginResetModel();
    m_entries = parsed;
    m_parentOf.clear();
    m_children.clear();
    m_rootChildren.clear();
    m_rowInParent.clear();

    const int n = static_cast<int>(m_entries.size());
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
            m_rowInParent[i] = static_cast<int>(m_rootChildren.size());
            m_rootChildren.append(i);
        } else {
            const int p = stack.last();
            m_parentOf[i] = p;
            m_rowInParent[i] = static_cast<int>(m_children[p].size());
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

    if (parent.model() != this)
        return QModelIndex();
    const int parentEntry = static_cast<int>(parent.internalId());
    if (parentEntry < 0 || parentEntry >= m_children.size())
        return QModelIndex();
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
    if (child.model() != this || entry < 0 || entry >= m_parentOf.size())
        return QModelIndex();
    const int parentEntry = m_parentOf.at(entry);
    if (parentEntry < 0)
        return QModelIndex();
    return createIndex(m_rowInParent.at(parentEntry), 0, static_cast<quintptr>(parentEntry));
}

int OutlineModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return static_cast<int>(m_rootChildren.size());
    if (parent.column() != 0)
        return 0;
    const int entry = static_cast<int>(parent.internalId());
    if (parent.model() != this || entry < 0 || entry >= m_children.size())
        return 0;
    return static_cast<int>(m_children.at(entry).size());
}

int OutlineModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant OutlineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this)
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
    if (index.model() != this || entry < 0 || entry >= m_entries.size())
        return -1;
    return m_entries.at(entry).blockNumber;
}
