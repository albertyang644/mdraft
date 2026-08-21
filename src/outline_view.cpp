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

    // Clicking an outline entry navigates the editor to that heading.
    connect(this, &QTreeView::clicked, this, [this](const QModelIndex &idx) {
        auto *model = qobject_cast<OutlineModel *>(this->model());
        if (!model)
            return;
        int block = model->blockNumberAt(idx.row());
        if (block >= 0)
            emit goToBlock(block);
    });
}
