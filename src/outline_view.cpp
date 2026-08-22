#include "outline_view.h"
#include "outline_model.h"

#include <QHeaderView>

OutlineView::OutlineView(QWidget *parent)
    : QTreeView(parent)
{
    header()->hide();
    setUniformRowHeights(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setIndentation(16);
    setRootIsDecorated(false); // no twisty for top-level items...
    // ...and QTreeView still draws them for nested children regardless of
    // setRootIsDecorated(), so suppress the branch decoration outright: the
    // outline is always fully expanded, there's nothing to twist open.
    setStyleSheet("QTreeView::branch { border-image: none; image: none; }");

    // Clicking an outline entry navigates the editor to that heading.
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
        auto *model = qobject_cast<OutlineModel *>(this->model());
        if (!model)
            return;
        int block = model->blockNumberForIndex(idx);
        if (block >= 0)
            emit goToBlock(block);
    });
}

void OutlineView::setModel(QAbstractItemModel *model)
{
    QTreeView::setModel(model);
    // Headings are always nested visually; there's no user-facing collapse,
    // so keep every level expanded whenever the outline is rebuilt.
    if (model)
        connect(model, &QAbstractItemModel::modelReset, this, &QTreeView::expandAll);
}
