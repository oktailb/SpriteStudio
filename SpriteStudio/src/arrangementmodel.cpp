#include "arrangementmodel.h"
#include <QDebug>
#include <QApplication>

ArrangementModel::ArrangementModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

bool ArrangementModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                                const QModelIndex &destinationParent, int destinationChild)
{
    bool success = beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild);

    if (success) {
        QStandardItemModel::moveRows(sourceParent, sourceRow, count, destinationParent, destinationChild);

        endMoveRows();

        qDebug() << "ArrangementModel: Mouvement de lignes effectué. Forcing layout reset.";

        beginResetModel();
        endResetModel();

        return true;
    }

    return false;
}

bool ArrangementModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
    return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}
