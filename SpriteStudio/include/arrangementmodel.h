#ifndef ARRANGEMENTMODEL_H
#define ARRANGEMENTMODEL_H

#include <QStandardItemModel>
#include <QObject>
#include <QMimeData>

class ArrangementModel : public QStandardItemModel
{
    Q_OBJECT

public:
    explicit ArrangementModel(QObject *parent = nullptr);

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;
};

#endif // ARRANGEMENTMODEL_H
