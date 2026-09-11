#include "atlasboxitem.h"
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>
#include <algorithm>
#include <cmath>

AtlasBoxItem::AtlasBoxItem(int index, const QRect &rect, const QRect &atlasBounds, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_index(index)
    , m_rect(rect)
    , m_atlasBounds(atlasBounds)
{
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setZValue(10.0); // Draw on top of the atlas pixmap
}

void AtlasBoxItem::setIndex(int idx)
{
    if (m_index != idx) {
        m_index = idx;
        update();
    }
}

void AtlasBoxItem::setBoxRect(const QRect &rect)
{
    QRectF newRect(rect);
    if (m_rect != newRect) {
        prepareGeometryChange();
        m_rect = newRect;
        update();
    }
}

void AtlasBoxItem::setSelectedBox(bool sel)
{
    if (m_selected != sel) {
        m_selected = sel;
        update();
    }
}

QRectF AtlasBoxItem::boundingRect() const
{
    // Margin for handles and border
    const double margin = 16.0;
    return m_rect.adjusted(-margin, -margin, margin, margin);
}

QPainterPath AtlasBoxItem::shape() const
{
    QPainterPath path;
    path.addRect(m_rect);
    if (m_selected) {
        const double handleSize = 8.0;
        for (int h = TopLeft; h <= Left; ++h) {
            path.addRect(getHandleRect(static_cast<Handle>(h), handleSize));
        }
    }
    return path;
}

QRectF AtlasBoxItem::getHandleRect(Handle handle, double handleSize) const
{
    const double half = handleSize / 2.0;
    double x = 0.0;
    double y = 0.0;

    switch (handle) {
    case TopLeft:
        x = m_rect.left();
        y = m_rect.top();
        break;
    case Top:
        x = m_rect.center().x();
        y = m_rect.top();
        break;
    case TopRight:
        x = m_rect.right();
        y = m_rect.top();
        break;
    case Right:
        x = m_rect.right();
        y = m_rect.center().y();
        break;
    case BottomRight:
        x = m_rect.right();
        y = m_rect.bottom();
        break;
    case Bottom:
        x = m_rect.center().x();
        y = m_rect.bottom();
        break;
    case BottomLeft:
        x = m_rect.left();
        y = m_rect.bottom();
        break;
    case Left:
        x = m_rect.left();
        y = m_rect.center().y();
        break;
    default:
        return QRectF();
    }

    return QRectF(x - half, y - half, handleSize, handleSize);
}

AtlasBoxItem::Handle AtlasBoxItem::handleAt(const QPointF &pos, double handleSize) const
{
    if (m_selected) {
        // Test corners first
        const Handle corners[] = { TopLeft, TopRight, BottomRight, BottomLeft };
        for (Handle h : corners) {
            if (getHandleRect(h, handleSize).contains(pos)) {
                return h;
            }
        }
        // Then test edges
        const Handle edges[] = { Top, Right, Bottom, Left };
        for (Handle h : edges) {
            if (getHandleRect(h, handleSize).contains(pos)) {
                return h;
            }
        }
    }

    if (m_rect.contains(pos)) {
        return Move;
    }

    return None;
}

void AtlasBoxItem::updateCursor(Handle handle)
{
    switch (handle) {
    case TopLeft:
    case BottomRight:
        setCursor(Qt::SizeFDiagCursor);
        break;
    case TopRight:
    case BottomLeft:
        setCursor(Qt::SizeBDiagCursor);
        break;
    case Top:
    case Bottom:
        setCursor(Qt::SizeVerCursor);
        break;
    case Left:
    case Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Move:
        setCursor(m_selected ? Qt::SizeAllCursor : Qt::PointingHandCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

void AtlasBoxItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    m_hovered = true;
    double scale = 1.0;
    if (scene() && !scene()->views().isEmpty()) {
        scale = scene()->views().first()->transform().m11();
    }
    double handleSize = (scale > 0.0) ? std::max(6.0 / scale, 6.0) : 8.0;

    Handle h = handleAt(event->pos(), handleSize);
    updateCursor(h);
    update();
}

void AtlasBoxItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    m_hovered = false;
    unsetCursor();
    update();
}

void AtlasBoxItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        double scale = 1.0;
        if (scene() && !scene()->views().isEmpty()) {
            scale = scene()->views().first()->transform().m11();
        }
        double handleSize = (scale > 0.0) ? std::max(6.0 / scale, 6.0) : 8.0;

        m_activeHandle = handleAt(event->pos(), handleSize);
        m_pressScenePos = event->scenePos();
        m_initialRect = m_rect;
        m_hasMoved = false;

        emit boxSelected(m_index, true, event->modifiers());
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

void AtlasBoxItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_activeHandle == None) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    QPointF delta = event->scenePos() - m_pressScenePos;
    QRectF newRect = m_initialRect;
    const double minSize = 3.0;

    switch (m_activeHandle) {
    case Move: {
        newRect.translate(delta.x(), delta.y());
        // Clamp to atlas bounds if valid
        if (!m_atlasBounds.isEmpty()) {
            if (newRect.left() < m_atlasBounds.left()) {
                newRect.moveLeft(m_atlasBounds.left());
            }
            if (newRect.right() > m_atlasBounds.right()) {
                newRect.moveRight(m_atlasBounds.right());
            }
            if (newRect.top() < m_atlasBounds.top()) {
                newRect.moveTop(m_atlasBounds.top());
            }
            if (newRect.bottom() > m_atlasBounds.bottom()) {
                newRect.moveBottom(m_atlasBounds.bottom());
            }
        }
        break;
    }
    case TopLeft: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setLeft(newLeft);
        newRect.setTop(newTop);
        break;
    }
    case Top: {
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setTop(newTop);
        break;
    }
    case TopRight: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        double newTop = std::min(m_initialRect.bottom() - minSize, m_initialRect.top() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
            newTop = std::max<double>(m_atlasBounds.top(), newTop);
        }
        newRect.setRight(newRight);
        newRect.setTop(newTop);
        break;
    }
    case Right: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
        }
        newRect.setRight(newRight);
        break;
    }
    case BottomRight: {
        double newRight = std::max(m_initialRect.left() + minSize, m_initialRect.right() + delta.x());
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newRight = std::min<double>(m_atlasBounds.right(), newRight);
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setRight(newRight);
        newRect.setBottom(newBottom);
        break;
    }
    case Bottom: {
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setBottom(newBottom);
        break;
    }
    case BottomLeft: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        double newBottom = std::max(m_initialRect.top() + minSize, m_initialRect.bottom() + delta.y());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
            newBottom = std::min<double>(m_atlasBounds.bottom(), newBottom);
        }
        newRect.setLeft(newLeft);
        newRect.setBottom(newBottom);
        break;
    }
    case Left: {
        double newLeft = std::min(m_initialRect.right() - minSize, m_initialRect.left() + delta.x());
        if (!m_atlasBounds.isEmpty()) {
            newLeft = std::max<double>(m_atlasBounds.left(), newLeft);
        }
        newRect.setLeft(newLeft);
        break;
    }
    default:
        break;
    }

    if (newRect != m_rect) {
        prepareGeometryChange();
        m_rect = newRect;
        m_hasMoved = true;
        update();
    }

    event->accept();
}

void AtlasBoxItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_activeHandle != None) {
        if (m_hasMoved) {
            QRect oldRect = m_initialRect.toRect();
            QRect newRect = m_rect.toRect();
            if (oldRect != newRect) {
                emit boxGeometryChanged(m_index, newRect, oldRect);
            }
        }
        m_activeHandle = None;
        m_hasMoved = false;
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void AtlasBoxItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    emit boxContextMenuRequested(m_index, event->screenPos());
    event->accept();
}

void AtlasBoxItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Compute cosmetic handle size based on view zoom
    double scale = 1.0;
    if (scene() && !scene()->views().isEmpty()) {
        scale = scene()->views().first()->transform().m11();
    }
    double handleSize = (scale > 0.0) ? std::max(6.0 / scale, 6.0) : 8.0;

    // 1. Fill
    if (m_selected) {
        painter->fillRect(m_rect, QColor(0, 160, 255, 60));
    } else if (m_hovered) {
        painter->fillRect(m_rect, QColor(0, 180, 255, 30));
    }

    // 2. Outline (cosmetic)
    QPen borderPen;
    borderPen.setCosmetic(true);
    if (m_selected) {
        borderPen.setColor(QColor(255, 200, 0));
        borderPen.setWidth(2);
        borderPen.setStyle(Qt::SolidLine);
    } else {
        borderPen.setColor(QColor(0, 180, 255, 180));
        borderPen.setWidth(1);
        borderPen.setStyle(Qt::SolidLine);
    }
    painter->setPen(borderPen);
    painter->drawRect(m_rect);

    // 3. Index Badge in top-left corner
    QString labelText = QString::number(m_index + 1);
    QFont badgeFont("Arial", 8, QFont::Bold);
    painter->setFont(badgeFont);

    QFontMetrics fm(badgeFont);
    int textW = fm.horizontalAdvance(labelText);
    int textH = fm.height();
    double badgeW = (textW + 6.0) / (scale > 0.0 ? scale : 1.0);
    double badgeH = (textH + 2.0) / (scale > 0.0 ? scale : 1.0);

    QRectF badgeRect(m_rect.left(), m_rect.top(), badgeW, badgeH);
    painter->fillRect(badgeRect, m_selected ? QColor(255, 200, 0, 220) : QColor(0, 50, 90, 200));

    painter->setPen(m_selected ? Qt::black : Qt::white);
    painter->drawText(badgeRect, Qt::AlignCenter, labelText);

    // 4. Resize Handles (only when selected)
    if (m_selected) {
        QPen handlePen(QColor(40, 40, 40));
        handlePen.setCosmetic(true);
        handlePen.setWidth(1);
        painter->setPen(handlePen);
        painter->setBrush(QColor(255, 255, 255));

        for (int h = TopLeft; h <= Left; ++h) {
            QRectF hr = getHandleRect(static_cast<Handle>(h), handleSize);
            painter->drawRect(hr);
        }
    }

    painter->restore();
}
