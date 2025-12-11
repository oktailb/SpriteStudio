/**
 * @file mainwindow.h
 * @brief Defines the MainWindow class, which manages the application's user interface,
 * sprite extraction, frame arrangement, and animation preview logic.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qgraphicsitem.h>
#include <QLabel>
#include <qstandarditemmodel.h>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTreeWidget>
#include <QInputDialog>
#include <QProgressBar>
#include "extractor/extractor.h"
#include "arrangementmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @brief Custom item delegate for drawing visual feedback (highlights) in the frame list view.
 *
 * This delegate is used to provide visual cues during drag-and-drop operations,
 * specifically indicating whether a drop action will result in a frame merger
 * or an insertion before/after the target item.
 */
class FrameDelegate : public QStyledItemDelegate
{
public:
  /**
   * @brief Defines the possible highlight states for drag-and-drop feedback.
   */
  enum HighlightState {
    None,          /**< No highlight is active. */
    Merge,         /**< Highlighted for a merge operation (drop in the center). */
    InsertLeft,    /**< Highlighted for insertion before the item (drop on the left edge). */
    InsertRight    /**< Highlighted for insertion after the item (drop on the right edge). */
  };

  /**
   * @brief Constructor for FrameDelegate.
   * @param parent The parent QObject.
   */
  explicit FrameDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

  /**
   * @brief Sets the target row and the highlight state to be drawn by the delegate.
   *    * This method configures the delegate's internal state, instructing it which row
   * to highlight and what kind of visual feedback to draw during the next paint event.
   * * @param row The model row index to be highlighted. Use -1 for no highlight.
   * @param state The type of highlight to display (Merge, InsertLeft, InsertRight, or None).
   */
  void setHighlight(int row, HighlightState state) {
    m_targetRow = row;
    m_state = state;
  }

  /**
   * @brief Overrides the standard paint method to draw custom highlights.
   *    * This method calls the base class paint method first to draw the standard item content
   * (the thumbnail and text), and then draws the custom merge or insertion indicators
   * based on the internal m_targetRow and m_state.
   * * @param painter The QPainter used for drawing.
   * @param option The style options for the view item.
   * @param index The model index of the item being drawn.
   */
  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    // Base implementation (draws thumbnail and text)
    QStyledItemDelegate::paint(painter, option, index);

    if (index.row() != m_targetRow || m_state == None) {
        return;
      }

    painter->save();

    if (m_state == Merge) {
        QPen pen(Qt::magenta);
        pen.setWidth(3);
        pen.setStyle(Qt::DotLine);
        QRect rect = option.rect.adjusted(2, 2, -2, -2);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect);
      }
    else if (m_state == InsertLeft || m_state == InsertRight) {
        int xPos = (m_state == InsertLeft) ? option.rect.left() : option.rect.right();

        if (m_state == InsertRight) xPos -= 2;

        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::black);
        painter->drawRect(xPos, option.rect.top(), 3, option.rect.height());
      }

    painter->restore();
  }

private:
  int             m_targetRow = -1; /**< Stores the row index currently targeted for highlight. -1 means no row is targeted. */
  HighlightState  m_state = None;   /**< Stores the type of visual feedback to be drawn for the target row. */
};

/**
 * @brief The main window class for the Sprite Studio application.
 *
 * MainWindow handles user interactions, manages the state of the sprite extraction
 * (via the Extractor hierarchy), controls the frame list display (via ArrangementModel),
 * and provides animation and export functionalities.
 */
class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  /**
   * @brief Constructs the MainWindow.
   *    * Initializes the UI, sets up the model and delegate for the frame list,
   * and connects necessary signals and slots.
   * @param parent The parent widget.
   */
  MainWindow(QWidget *parent = nullptr);

  /**
   * @brief Destructor for the MainWindow.
   */
  ~MainWindow();

protected:
  /**
   * @brief Handles system timer events.
   *    * Used primarily for general application updates or potentially low-frequency,
   * non-critical periodic tasks.
   * @param event The QTimerEvent object.
   */
  void timerEvent(QTimerEvent *event) override;

  /**
   * @brief Handles the window close event.
   *    * Allows custom shutdown logic, such as prompting the user to save unsaved work.
   * @param e The QCloseEvent object.
   */
  void closeEvent(QCloseEvent *e) override;

  /**
   * @brief Handles the window resize event.
   * * When the main window is resized, we must manually force the QGraphicsViews
   * to re-scale their contents to fit the new viewport size.
   * * @param event The resize event object.
   */
  void resizeEvent(QResizeEvent *event) override;

  /**
   * @brief When window is resized of image loaded for  fisrst ttime,  the picture is adjusted to the viw.
   * This method will recalculate the effective value of zoomFFactor and make it visible
   */
  void adjustZoomSliderToWindow();

  /**
   * @brief wheelEvent
   * * Allows to zoom on atlas view
   * @param event The mouse wheel event
   */
  void wheelEvent(QWheelEvent *event) override;

  /**
   * @brief Custom event filter used to monitor events on specific watched objects.
   *    * This is typically used to capture mouse events on the frames list view
   * to provide custom drag-and-drop feedback (e.g., controlling the FrameDelegate).
   * @param watched The object receiving the event.
   * @param event The event being monitored.
   * @return true if the event was handled and should stop, false otherwise.
   */
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  /**
   * @brief Slot triggered by the 'Licence' action.
   *    * Displays the application's licensing information.
   */
  void on_actionLicence_triggered();

  /**
   * @brief Slot triggered by the 'About' action.
   *    * Displays information about the application and the developer.
   */
  void on_actionAbout_triggered();

  /**
   * @brief Slot triggered by the 'Open' action.
   *    * Prompts the user to select a sprite file (PNG, GIF, etc.) for processing.
   */
  void on_actionOpen_triggered();

  /**
   * @brief Slot triggered by the 'Exit' action.
   *    * Closes the application.
   */
  void on_actionExit_triggered();

  /**
   * @brief Slot triggered when the FPS (Frames Per Second) value is changed.
   *    * Updates the animation timer interval and refreshes the related UI elements.
   * @param arg1 The new FPS value.
   */
  void on_fps_valueChanged(int arg1);

  /**
   * @brief Slot triggered when the Alpha Threshold value is changed.
   *    * Re-runs the sprite detection algorithm on the in-memory atlas.
   * @param threshold The new alpha threshold value.
   */
  void on_alphaThreshold_valueChanged(int threshold);

  /**
   * @brief Slot triggered when the Vertical Tolerance value is changed.
   *    * Re-runs the sprite detection algorithm on the in-memory atlas.
   * @param verticalTolerance The new vertical tolerance value.
   */
  void on_verticalTolerance_valueChanged(int verticalTolerance);

  /**
   * @brief Slot triggered when a frame is clicked in the frames list.
   *    * Handles frame selection and updates the bounding box highlight in the atlas view.
   * @param index The model index of the clicked frame.
   */
  void on_framesList_clicked(const QModelIndex &index);

  /**
   * @brief Slot connected to ArrangementModel::mergeRequested.
   *    * Executes the logic to merge two frames (combining the source frame onto the target frame).
   * @param sourceRow The row index of the frame being dragged.
   * @param targetRow The row index of the frame being dropped onto.
   */
  void onMergeFrames(int sourceRow, int targetRow);

  /**
   * @brief Slot triggered by a custom context menu request on the frames list.
   *    * Displays the context menu for frame manipulation (e.g., delete, reverse order).
   * @param pos The position where the context menu was requested (local to the widget).
   */
  void on_framesList_customContextMenuRequested(const QPoint &pos);

  /**
   * @brief Executes the action to delete all currently selected frames.
   */
  void deleteSelectedFrame();

  /**
   * @brief Executes the action to invert the current selection in the frames list.
   */
  void invertSelection();

  /**
   * @brief Advances the animation to the next frame and updates the animation view.
   */
  void updateAnimation();

  /**
   * @brief Reverses the order of the currently selected animation.
   */
  void reverseAnimationOrder();

  /**
   * @brief Slot triggered by the 'Export' action.
   *    * Prompts the user for a filename and triggers the export process (PNG atlas and JSON metadata).
   */
  void on_actionExport_triggered();

  /**
   * @brief Slot triggered by the 'Play' button.
   *    * Starts or resumes the frame animation sequence.
   */
  void on_Play_clicked();

  /**
   * @brief Slot triggered by the 'Pause' button.
   *    * Pauses the frame animation sequence.
   */
  void on_Pause_clicked();

  /**
   * @brief Slot triggered by a custom context menu request on the central atlas image.
   * * Displays the context menu for atlas manipulation (e.g., remove background).
   * @param pos The position where the context menu was requested.
   */
  void onAtlasContextMenuRequested(const QPoint &pos);

  /**
   * @brief Automatically removes the background color from the atlas image.
   *    * This calculates the dominant color and replaces all similar pixels with transparency,
   * then re-runs the sprite extraction.
   */
  void removeAtlasBackground();

  /**
   * @brief Same as removeAtlasBackground plus refresh HMI and sprites ddetection process
   */
  void removeAtlasBackgroundAndRefresh();

  /**
   * @brief createAnimationFromSelection
   */
  void createAnimationFromSelection();

  /**
   * @brief createAnimationFromSelection
   */
  void createAnimation(QString name, QList<int> indexes, int fps);

  /**
   * @brief on_animationList_customContextMenuRequested
   * @param pos
   */
  void on_animationList_customContextMenuRequested(const QPoint &pos);

  /**
   * @brief removeSelectedAnimation
   */
  void removeSelectedAnimation();

  /**
   * @brief on_animationList_itemSelectionChanged
   */
  void on_animationList_itemSelectionChanged();

  /**
   * @brief on_animationList_itemClicked
   * @param item
   * @param column
   */
  void on_animationList_itemClicked(QTreeWidgetItem *item, int column);

  /**
   * @brief zoomSliderChanged
   * @param val
   */
  void zoomSliderChanged(int val);

  /**
   * @brief on_enableSmartCropCheckbox_stateChanged
   * @param state
   */
  void on_enableSmartCropCheckbox_stateChanged(int state);

  /**
   * @brief on_overlapThresholdSpinbox_valueChanged
   * @param threshold
   */
  void on_overlapThresholdSpinbox_valueChanged(double threshold);

  /**
   * @brief deleteSelectedFramesFromAtlas
   */
  void deleteSelectedFramesFromAtlas();

  /**
   * @brief updateAnimationsList
   */
  void updateAnimationsList();

  /**
     * @brief Crée ou met à jour l'animation "current" basée sur la sélection actuelle
     */
  void updateCurrentAnimation();

  /**
     * @brief Supprime l'animation "current" si elle existe
     */
  void removeCurrentAnimation();

  /**
     * @brief Vérifie si l'animation "current" existe
     * @return true si l'animation "current" existe
     */
  bool hasCurrentAnimation() const;

  // Méthodes refactorisées
  void reverseFramesOrder(QList<int> &selectedIndices);
  void invertFrameSelection();
  void deleteFrames(const QList<int> &frameIndices);
  void removeAnimations(const QStringList &animationNames);

  // Méthodes d'animation refactorisées
  bool canStartAnimation() const;
  void setupAnimationParameters();
  void setupAnimationUI();
  void startAnimationTimer();
  void stopAnimationTimer();
  void updateAnimationUI(bool playing);
  void on_spriteCleanButton_clicked();
  void on_spriteAlignButton_clicked();
  void on_mirrorButton_clicked();
  void on_sliderFrom_sliderMoved(int position);


  private:
  Ui::MainWindow *ui;                                     /**< Pointer to the UI generated by Qt Designer. */
  ArrangementModel *frameModel;                           /**< Custom model managing the list of extracted frames, enabling drag-and-drop logic. */
  double timestamp;                                       /**< Not currently used, could be reserved for future timing/debug. */
  int timerId;                                            /**< ID for the QObject::startTimer used for low-frequency updates. */
  bool ready;                                             /**< Flag indicating if the main window initialization is complete. */
  Extractor *extractor;                                   /**< Base pointer to the current file handler (SpriteExtractor, GifExtractor, etc.). */
  QString currentFilePath;                                /**< Path to the currently loaded file. */
  QList<QGraphicsRectItem*> boundingBoxHighlighters;      /**< Graphic item used to highlight the bounding box of a selected frame in the atlas view. */
  QGraphicsRectItem *mergeHighlighter = nullptr;          /**< Graphic item used to highlight the merge target in the frames list. */
  FrameDelegate *listDelegate;                            /**< Custom delegate used to draw drag-and-drop feedback in the frames list. */
  QTimer *animationTimer;                                 /**< Timer controlling the frame rate of the animation preview. */
  int currentAnimationFrameIndex;                         /**< Index of the frame currently displayed in the animation preview. */
  QList<int> selectedFrameRows;                           /**< List of row indices for all currently selected frames. */
  QGraphicsRectItem *selectionRectItem = nullptr;
  QPointF selectionStartPoint;
  bool isSelecting = false;
  Qt::KeyboardModifiers selectionModifiers;
  QList<int> currentSelection;
  QProgressBar *progressBar;
  QLabel * statusLabel;
  QLabel * zoomLabel;
  QSlider* zoomSlider;
  double zoomFactor = 1.0;

  /**
   * @brief Populates the frame list model with frames and metadata from the extractor.
   * @param frameList The list of QPixmap frames.
   * @param boxList The corresponding list of bounding boxes.
   */
  void populateFrameList(const QList<QPixmap> &frameList, const QList<Extractor::Box> &boxList);

  /**
   * @brief Determines the file type, creates the appropriate Extractor, and starts the extraction process.
   * @param fileName The path to the file to be processed.
   */
  void processFile(const QString &fileName);

  /**
   * @brief Sets up the graphics view to display the main sprite atlas.
   * * This method is called after a new image file (sprite sheet or GIF) is loaded.
   * It resets the scene, adds the new atlas pixmap, and ensures the view scales to fit it.
   * * @param pixmap The QPixmap of the full sprite atlas.
   */
  void setupGraphicsView(const QImage &pixmap);

  /**
   * @brief Sets or hides the merge highlight in the central atlas view (used during drag-and-drop).
   * @param index The model index of the item being highlighted.
   * @param show If true, display the highlight; if false, hide it.
   */
  void setMergeHighlight(const QModelIndex &index, bool show);

  /**
   * @brief Clears any active merge highlight.
   */
  void clearMergeHighlight();

  /**
   * @brief Deletes a frame at the specified row index from the model.
   * @param row The row index to delete.
   */
  void deleteFrame(int row);

  /**
   * @brief Initializes and starts the animation preview timer.
   */
  void startAnimation();

  /**
   * @brief Stops the animation preview timer.
   */
  void stopAnimation();

  /**
   * @brief Clean up all bounding box highlighter on atlas view
   */
  void clearBoundingBoxHighlighters();

  /**
   * @brief Update all bounding box highlighter on atlas view
   */
  void setBoundingBoxHighllithers(const QList<int> &selectedIndices);

  /**
   * @brief Set zoom/position on atlas view to fit current selection
   */
  void fitSelectedFramesInView(int padding);

  /**
   * @brief Called when a rectangular selection is made from Atlas view
   * @param scenePos
   */
  void startSelection(const QPointF &scenePos);

  /**
   * @brief Called during muve move on rectagular selection to update the selected frames list in real time
   * @param scenePos
   */
  void updateSelection(const QPointF &scenePos);

  /**
   * @brief Reflect atlas selection to the bottom list selected frames
   * @param frameIndices
   */
  void selectFramesInList(const QList<int> &frameIndices);

  /**
   * @brief Clean up rectangular selection
   */
  void endSelection();

  /**
   * @brief findFramesInSelectionRect
   * @param rect
   * @return
   */
  QList<int> findFramesInSelectionRect(const QRectF &rect);

  /**
   * @brief Give a ccolor from an internal list
   * @param index
   * @param total
   * @return A prredefined  color in internal cycling list
   */
  QColor getHighlightColor(int index, int total);

  /**
   * @brief syncAnimationListWidget
   */
  void syncAnimationListWidget();

  void refreshFrameListDisplay();

  void updateAnimationsAfterFrameRemoval(int removedRow, int mergedRow);

  /**
     * @brief Récupère les indices des frames sélectionnées depuis le modèle Extractor
     * @return Liste des indices avec selected = true
     */
  QList<int> getSelectedFrameIndices() const;

  /**
     * @brief Met à jour la sélection dans le modèle Extractor
     * @param selectedIndices Liste des indices à marquer comme sélectionnés
     */
  void setSelectedFrameIndices(const QList<int> &selectedIndices);

  /**
     * @brief Efface toutes les sélections dans le modèle Extractor
     */
  void clearFrameSelections();

  /**
     * @brief Met à jour l'affichage de la liste des frames basé sur le modèle Extractor
     */
  void updateFrameListSelectionFromModel();

};

#endif // MAINWINDOW_H
