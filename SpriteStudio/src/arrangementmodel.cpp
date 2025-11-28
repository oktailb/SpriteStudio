#include "arrangementmodel.h"
#include <QDebug>
#include <QIODevice>

ArrangementModel::ArrangementModel(QObject *parent)
    : QStandardItemModel(parent)
{
}

Qt::ItemFlags ArrangementModel::flags(const QModelIndex &index) const
{
  // Retrieve the default flags for the item
  Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);

  if (index.isValid()) {
      // For a valid item, enable dragging (source) and dropping (target for merge)
      return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    }

  // If the index is invalid (dropping into empty space/list end), still allow dropping
  // for standard reordering functionality (handled by the base class).
  return defaultFlags | Qt::ItemIsDropEnabled;
}

QMimeData *ArrangementModel::mimeData(const QModelIndexList &indexes) const
{
  // Use the default implementation for standard data
  QMimeData *mimeData = QStandardItemModel::mimeData(indexes);

         // Add custom data to easily identify the source row
  if (!indexes.isEmpty()) {
      QByteArray encodedData;
      QDataStream stream(&encodedData, QIODevice::WriteOnly);

      // We only care about the first selected row for the drag source
      stream << indexes.first().row();

      // Custom mime type used to transport the source row index
      mimeData->setData("application/x-sprite-row", encodedData);
    }
  return mimeData;
}

bool ArrangementModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                int row, int column, const QModelIndex &parent)
{
  // 1. Check for MERGE operation (Drop ONTO an existing item)
  // 'parent.isValid()' is true when an item is dropped directly onto another item.
  if (parent.isValid() && data->hasFormat("application/x-sprite-row")) {

      // Extract the source row from the custom mime data
      QByteArray encodedData = data->data("application/x-sprite-row");
      QDataStream stream(&encodedData, QIODevice::ReadOnly);
      int sourceRow;
      stream >> sourceRow;

      int targetRow = parent.row();

      // Prevent dropping an item onto itself
      if (sourceRow != targetRow) {
          // Emit a signal to MainWindow to execute the heavy-lifting merge logic
          emit mergeRequested(sourceRow, targetRow);
          return true; // Drop successfully accepted and handled (no standard reordering needed)
        }
    }

  // 2. Otherwise, perform STANDARD REORDERING (Drop BETWEEN items or into empty space)
  return QStandardItemModel::dropMimeData(data, action, row, column, parent);
}
