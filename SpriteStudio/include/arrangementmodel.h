#ifndef ARRANGEMENTMODEL_H
#define ARRANGEMENTMODEL_H

#include <QStandardItemModel>
#include <QObject>
#include <QMimeData>

/**
 * @file arrangementmodel.h
 * @brief Custom QStandardItemModel subclass to handle specialized drag-and-drop behavior,
 * including drag initialization and detection of merge operations.
 */
class ArrangementModel : public QStandardItemModel
{
  Q_OBJECT

public:
  /**
   * @brief Constructor for the ArrangementModel.
   * @param parent The parent QObject.
   */
  explicit ArrangementModel(QObject *parent = nullptr);

  /**
   * @brief Overrides the standard flags to enable drag and drop functionality.
   *    * Crucially, this method sets both @c Qt::ItemIsDragEnabled and @c Qt::ItemIsDropEnabled
   * for valid indexes, allowing items to be dropped *onto* other items (required for merging).
   *    * @param index The model index.
   * @return The item flags for the given index.
   */
  Qt::ItemFlags flags(const QModelIndex &index) const override;

  /**
   * @brief Prepares the mime data for a drag operation.
   *    * This implementation extends the standard mime data by adding custom data
   * (`"application/x-sprite-row"`) containing the source row number. This allows
   * the target (MainWindow event filter or dropMimeData) to easily identify
   * the frame being dragged.
   *    * @param indexes A list of indexes being dragged (usually one in this context).
   * @return A QMimeData object containing the standard and custom data.
   */
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  /**
   * @brief Handles the final drop action.
   *    * This method is responsible for detecting if the drop occurred *onto* an existing
   * item (`parent.isValid()`), which signifies a **merge operation**. If it is a merge,
   * it emits the @c mergeRequested signal and returns true. Otherwise, it delegates
   * to the standard implementation for reordering (insertion between items).
   *    * @param data The QMimeData being dropped.
   * @param action The requested drop action.
   * @param row The target row index (if dropping between items).
   * @param column The target column index.
   * @param parent The parent index (if dropping onto an item).
   * @return true if the drop was handled successfully, false otherwise.
   */
  bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                     int row, int column, const QModelIndex &parent) override;

signals:
  /**
   * @brief Signal emitted when a merge operation is requested by dropping a frame onto another.
   *    * This signal is connected to a slot in the MainWindow to execute the frame merging logic.
   *    * @param sourceRow The row index of the frame being dragged.
   * @param targetRow The row index of the frame being dropped upon.
   */
  void mergeRequested(int sourceRow, int targetRow);
};

#endif // ARRANGEMENTMODEL_H
