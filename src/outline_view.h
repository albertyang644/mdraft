#ifndef OUTLINE_VIEW_H
#define OUTLINE_VIEW_H

#include <QTreeView>

class OutlineView : public QTreeView
{
    Q_OBJECT
public:
    explicit OutlineView(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model) override;

signals:
    void goToBlock(int blockNumber);
};

#endif // OUTLINE_VIEW_H
