#ifndef ATLASBOXITEM_H
#define ATLASBOXITEM_H

#include <QGraphicsObject>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QFont>

/**
 * @brief Interactive QGraphicsObject representing a sprite bounding box on the atlas.
 *
 * Provides 8 resize handles when selected, drag-to-move, context menu, and emits
 * signals for selection and geometry modifications with undo/redo integration.
 */
class AtlasBoxItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum Handle {
        None,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Move
    };

    explicit AtlasBoxItem(int index, const QRect &rect, const QRect &atlasBounds, QGraphicsItem *parent = nullptr);
    ~AtlasBoxItem() override = default;

    int index() const { return m_index; }
    void setIndex(int idx);

    QRect boxRect() const { return m_rect.toRect(); }
    void setBoxRect(const QRect &rect);

    bool isSelectedBox() const { return m_selected; }
    void setSelectedBox(bool sel);

    void setAtlasBounds(const QRect &bounds) { m_atlasBounds = bounds; }

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

signals:
    void boxSelected(int index, bool selected, Qt::KeyboardModifiers modifiers);
    void boxGeometryChanged(int index, const QRect &newRect, const QRect &oldRect);
    void boxContextMenuRequested(int index, const QPoint &screenPos);

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    Handle handleAt(const QPointF &pos, double handleSize) const;
    QRectF getHandleRect(Handle handle, double handleSize) const;
    void updateCursor(Handle handle);

    int      m_index = 0;
    QRectF   m_rect;
    QRect    m_atlasBounds;
    bool     m_selected = false;
    bool     m_hovered = false;

    Handle   m_activeHandle = None;
    QPointF  m_pressScenePos;
    QRectF   m_initialRect;
    bool     m_hasMoved = false;
};

#endif // ATLASBOXITEM_H
