#ifndef ATLASVIEWCONTROLLER_H
#define ATLASVIEWCONTROLLER_H

#include <QObject>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QList>
#include <QRect>
#include <QPointF>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>

class SpriteDocument;
class QUndoStack;
class AtlasBoxItem;

/**
 * @brief Controller managing the Atlas QGraphicsView, zoom/pan, tools, and AtlasBoxItem interactions.
 */
class AtlasViewController : public QObject
{
    Q_OBJECT

public:
    enum SliceToolMode {
        ToolSelect,
        ToolAddSlice
    };
    Q_ENUM(SliceToolMode)

    explicit AtlasViewController(QGraphicsView *view,
                                 SpriteDocument *document,
                                 QUndoStack *undoStack = nullptr,
                                 QObject *parent = nullptr);
    ~AtlasViewController() override;

    // Tool mode
    SliceToolMode toolMode() const { return m_sliceToolMode; }
    void setToolMode(SliceToolMode mode);

    // Zoom and Pan
    double zoomFactor() const { return m_zoomFactor; }
    void setZoomFactor(double factor);
    void zoomIn(double step = -1.0);
    void zoomOut(double step = -1.0);
    void zoomAt(const QPointF &viewportPos, double factor);
    void fitInView();
    void adjustZoomToWindow();

    // Scene & Atlas Image
    void setAtlasImage(const QImage &image);
    void clearAtlas();
    QGraphicsScene* scene() const { return m_scene; }
    QGraphicsPixmapItem* atlasPixmapItem() const { return m_atlasPixmapItem; }

    // Box items
    void syncAtlasBoxes();
    void clearAtlasBoxes();
    const QList<AtlasBoxItem*>& boxItems() const { return m_boxItems; }
    int boxCount() const { return m_boxItems.size(); }

    // Selection
    QList<int> selectedBoxIndices() const;
    void setSelectedBoxIndices(const QList<int> &indices);
    void clearSelection();
    void selectAll();
    void invertSelection();
    void startMarqueeSelection(const QPointF &scenePos, Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers());
    void updateMarqueeSelection(const QPointF &scenePos);
    void endMarqueeSelection();

    // Slice Commands
    void trimSelectedSlice(int alphaThreshold = -1);
    void mergeSelectedSlices();
    void deleteSelectedSlices();
    void eraseSelectedSlicesPixels();
    void nudgeSelectedBoxes(int dx, int dy);

    // Context Menu & Views
    void fitSelectedFramesInView(int padding = -1);

signals:
    void selectionChanged(const QList<int> &selectedIndices);
    void zoomChanged(double zoomFactor);
    void toolModeChanged(SliceToolMode mode);
    void boxContextMenuRequested(int index, const QPoint &screenPos);
    void atlasContextMenuRequested(const QPoint &pos);
    void statusMessage(const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onBoxItemSelected(int index, bool selected, Qt::KeyboardModifiers modifiers);
    void onBoxItemGeometryChanged(int index, const QRect &newRect, const QRect &oldRect);
    void onBoxContextMenu(int index, const QPoint &screenPos);

private:
    QList<int> findFramesInSelectionRect(const QRectF &rect);
    void updateBoxSelectionVisuals(const QList<int> &selectedIndices);

    QGraphicsView       *m_view = nullptr;
    QGraphicsScene      *m_scene = nullptr;
    SpriteDocument      *m_document = nullptr;
    QUndoStack          *m_undoStack = nullptr;

    QGraphicsPixmapItem *m_atlasPixmapItem = nullptr;
    QList<AtlasBoxItem*> m_boxItems;

    SliceToolMode        m_sliceToolMode = ToolSelect;
    double               m_zoomFactor = 1.0;

    // Pan state
    bool                 m_isPanning = false;
    QPoint               m_panStartPos;

    // New slice drawing state
    bool                 m_isDrawingNewSlice = false;
    QPointF              m_newSliceStart;
    QGraphicsRectItem   *m_newSlicePreviewItem = nullptr;

    // Marquee selection state
    bool                 m_isSelecting = false;
    QPointF              m_selectionStartPoint;
    QGraphicsRectItem   *m_selectionRectItem = nullptr;
    QList<int>           m_dragBaseSelection;
    QList<int>           m_dragCurrentSelection;
    Qt::KeyboardModifiers m_selectionModifiers = Qt::NoModifier;
};

#endif // ATLASVIEWCONTROLLER_H
