/*
 * Copyright 2026 Audacious developers and others
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#include "native_widget_item.h"

#include <QApplication>
#include <QFocusEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QQuickWindow>
#include <QWheelEvent>
#include <QWidget>

namespace
{

constexpr int refresh_interval_ms = 100;

QPointF fractionalPosition(const QPointF & position)
{
    return {position.x() - qFloor(position.x()),
            position.y() - qFloor(position.y())};
}

} // namespace

NativeWidgetItem::NativeWidgetItem(QQuickItem * parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setActiveFocusOnTab(true);
    setFlag(ItemAcceptsInputMethod, true);
    setFillColor(Qt::transparent);

    m_refresh_timer.setInterval(refresh_interval_ms);
    connect(&m_refresh_timer, &QTimer::timeout, this,
            &NativeWidgetItem::captureWidget);
}

NativeWidgetItem::~NativeWidgetItem()
{
    m_refresh_timer.stop();
    m_widget.clear();
}

void NativeWidgetItem::setWidget(QWidget * widget)
{
    if (m_widget == widget)
        return;

    m_refresh_timer.stop();
    m_mouse_target.clear();
    m_hover_target.clear();
    m_widget = widget;

    if (!m_widget)
    {
        setImplicitSize(0, 0);
        QMutexLocker locker(&m_frame_mutex);
        m_frame = {};
        update();
        return;
    }

    connect(m_widget.data(), &QObject::destroyed, this, [this]() {
        m_refresh_timer.stop();
        m_mouse_target.clear();
        m_hover_target.clear();
        m_widget.clear();
        setImplicitSize(0, 0);
        {
            QMutexLocker locker(&m_frame_mutex);
            m_frame = {};
        }
        update();
    });

    m_widget->setAttribute(Qt::WA_DontShowOnScreen, true);
    m_widget->ensurePolished();
    m_widget->show();
    syncWidgetGeometry();
    captureWidget();
    if (isVisible())
        m_refresh_timer.start();
}

void NativeWidgetItem::syncWidgetGeometry()
{
    if (!m_widget)
        return;

    const QSize size_hint =
        m_widget->sizeHint().expandedTo(m_widget->minimumSizeHint());
    const int widget_width =
        qMax(1, qRound(width() > 0 ? width() : size_hint.width()));

    int widget_height = size_hint.height();
    if (m_widget->layout() && m_widget->layout()->hasHeightForWidth())
        widget_height = m_widget->layout()->totalHeightForWidth(widget_width);
    else if (m_widget->hasHeightForWidth())
        widget_height = m_widget->heightForWidth(widget_width);
    widget_height = qMax(widget_height, m_widget->minimumSizeHint().height());
    widget_height = qMax(1, widget_height);

    setImplicitSize(qMax(1, size_hint.width()), widget_height);
    m_widget->resize(widget_width,
                     qMax(widget_height, qRound(height())));

    if (window())
    {
        const QPointF scene_position = mapToScene(QPointF());
        m_widget->move(window()->mapToGlobal(scene_position.toPoint()));
    }
}

void NativeWidgetItem::captureWidget()
{
    if (!m_widget || !isVisible() || width() <= 0 || height() <= 0)
        return;

    syncWidgetGeometry();

    const qreal ratio = window() ? window()->devicePixelRatio() : 1.0;
    const QSize logical_size = m_widget->size();
    QImage frame((logical_size * ratio).expandedTo(QSize(1, 1)),
                 QImage::Format_ARGB32_Premultiplied);
    frame.setDevicePixelRatio(ratio);
    frame.fill(Qt::transparent);

    QPainter painter(&frame);
    m_widget->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();

    {
        QMutexLocker locker(&m_frame_mutex);
        m_frame = std::move(frame);
    }
    update();
}

void NativeWidgetItem::paint(QPainter * painter)
{
    QImage frame;
    {
        QMutexLocker locker(&m_frame_mutex);
        frame = m_frame;
    }

    if (!frame.isNull())
        painter->drawImage(QPointF(), frame);
}

void NativeWidgetItem::geometryChange(const QRectF & new_geometry,
                                      const QRectF & old_geometry)
{
    QQuickPaintedItem::geometryChange(new_geometry, old_geometry);
    syncWidgetGeometry();
    captureWidget();
}

void NativeWidgetItem::itemChange(ItemChange change,
                                  const ItemChangeData & data)
{
    QQuickPaintedItem::itemChange(change, data);

    if (change == ItemVisibleHasChanged)
    {
        if (data.boolValue && m_widget)
        {
            captureWidget();
            m_refresh_timer.start();
        }
        else
            m_refresh_timer.stop();
    }
    else if (change == ItemSceneChange && data.window)
        captureWidget();
}

QWidget * NativeWidgetItem::widgetAt(const QPointF & position) const
{
    if (!m_widget)
        return nullptr;

    QWidget * child = m_widget->childAt(position.toPoint());
    return child ? child : m_widget.data();
}

QWidget * NativeWidgetItem::focusWidget() const
{
    if (!m_widget)
        return nullptr;

    QWidget * focused = m_widget->focusWidget();
    return focused ? focused : m_widget.data();
}

QPointF NativeWidgetItem::mapToWidget(QWidget * widget,
                                      const QPointF & position) const
{
    if (!m_widget || !widget || widget == m_widget)
        return position;

    return widget->mapFrom(m_widget.data(), position.toPoint()) +
           fractionalPosition(position);
}

void NativeWidgetItem::sendMouseEvent(QMouseEvent * source, QEvent::Type type,
                                      QWidget * target)
{
    if (!m_widget)
        return;

    if (!target)
        target = m_mouse_target ? m_mouse_target.data()
                                : widgetAt(source->position());
    if (!target)
        return;

    QMouseEvent event(type, mapToWidget(target, source->position()),
                      source->position(), source->globalPosition(),
                      source->button(), source->buttons(), source->modifiers(),
                      source->source(), source->pointingDevice());
    QApplication::sendEvent(target, &event);
    source->setAccepted(event.isAccepted());
    captureWidget();
}

void NativeWidgetItem::mousePressEvent(QMouseEvent * event)
{
    forceActiveFocus(Qt::MouseFocusReason);
    setKeepMouseGrab(true);
    m_mouse_target = widgetAt(event->position());
    sendMouseEvent(event, QEvent::MouseButtonPress);
}

void NativeWidgetItem::mouseMoveEvent(QMouseEvent * event)
{
    sendMouseEvent(event, QEvent::MouseMove);
}

void NativeWidgetItem::mouseReleaseEvent(QMouseEvent * event)
{
    sendMouseEvent(event, QEvent::MouseButtonRelease);
    if (event->buttons() == Qt::NoButton)
    {
        m_mouse_target.clear();
        setKeepMouseGrab(false);
    }
}

void NativeWidgetItem::mouseDoubleClickEvent(QMouseEvent * event)
{
    m_mouse_target = widgetAt(event->position());
    sendMouseEvent(event, QEvent::MouseButtonDblClick);
}

void NativeWidgetItem::mouseUngrabEvent()
{
    if (m_mouse_target)
    {
        QEvent ungrab(QEvent::UngrabMouse);
        QApplication::sendEvent(m_mouse_target.data(), &ungrab);
    }
    m_mouse_target.clear();
    setKeepMouseGrab(false);
}

void NativeWidgetItem::wheelEvent(QWheelEvent * event)
{
    QWidget * target = widgetAt(event->position());
    if (!target)
        return;

    QWheelEvent forwarded(mapToWidget(target, event->position()),
                          event->globalPosition(), event->pixelDelta(),
                          event->angleDelta(), event->buttons(),
                          event->modifiers(), event->phase(),
                          event->inverted(), event->source(),
                          event->pointingDevice());
    QApplication::sendEvent(target, &forwarded);
    event->setAccepted(forwarded.isAccepted());
    captureWidget();
}

void NativeWidgetItem::updateHover(QWidget * target, const QPointF & position,
                                   const QPointF & old_position,
                                   const QPointF & global_position,
                                   Qt::KeyboardModifiers modifiers)
{
    if (target != m_hover_target)
    {
        if (m_hover_target)
        {
            QEvent leave(QEvent::Leave);
            QApplication::sendEvent(m_hover_target.data(), &leave);
        }

        m_hover_target = target;
        if (m_hover_target)
        {
            const QPointF local = mapToWidget(m_hover_target, position);
            QEnterEvent enter(local, local, global_position);
            QApplication::sendEvent(m_hover_target.data(), &enter);
        }
    }

    if (m_hover_target)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
        QHoverEvent hover(QEvent::HoverMove,
                          mapToWidget(m_hover_target, position),
                          global_position,
                          mapToWidget(m_hover_target, old_position), modifiers);
#else
        QHoverEvent hover(QEvent::HoverMove,
                          mapToWidget(m_hover_target, position),
                          mapToWidget(m_hover_target, old_position), modifiers);
#endif
        QApplication::sendEvent(m_hover_target.data(), &hover);
    }

    captureWidget();
}

void NativeWidgetItem::hoverEnterEvent(QHoverEvent * event)
{
    updateHover(widgetAt(event->position()), event->position(),
                event->oldPosF(), event->globalPosition(), event->modifiers());
    event->accept();
}

void NativeWidgetItem::hoverMoveEvent(QHoverEvent * event)
{
    updateHover(widgetAt(event->position()), event->position(),
                event->oldPosF(), event->globalPosition(), event->modifiers());
    event->accept();
}

void NativeWidgetItem::hoverLeaveEvent(QHoverEvent * event)
{
    if (m_hover_target)
    {
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(m_hover_target.data(), &leave);
        m_hover_target.clear();
        captureWidget();
    }
    event->accept();
}

void NativeWidgetItem::keyPressEvent(QKeyEvent * event)
{
    if (QWidget * target = focusWidget())
    {
        QApplication::sendEvent(target, event);
        captureWidget();
    }
}

void NativeWidgetItem::keyReleaseEvent(QKeyEvent * event)
{
    if (QWidget * target = focusWidget())
    {
        QApplication::sendEvent(target, event);
        captureWidget();
    }
}

void NativeWidgetItem::inputMethodEvent(QInputMethodEvent * event)
{
    if (QWidget * target = focusWidget())
    {
        QApplication::sendEvent(target, event);
        captureWidget();
    }
}

QVariant NativeWidgetItem::inputMethodQuery(Qt::InputMethodQuery query) const
{
    QWidget * target = focusWidget();
    if (!target)
        return {};

    QVariant result = target->inputMethodQuery(query);
    if (query == Qt::ImCursorRectangle && result.canConvert<QRectF>())
    {
        QRectF rectangle = result.toRectF();
        rectangle.moveTopLeft(
            target->mapTo(m_widget.data(), rectangle.topLeft().toPoint()));
        return rectangle;
    }
    return result;
}

void NativeWidgetItem::focusInEvent(QFocusEvent * event)
{
    if (m_widget)
    {
        QWidget * target = m_widget->focusWidget();
        if (!target || target == m_widget)
            target = m_widget->nextInFocusChain();
        if (target)
            target->setFocus(event->reason());
        captureWidget();
    }
    QQuickPaintedItem::focusInEvent(event);
}

void NativeWidgetItem::focusOutEvent(QFocusEvent * event)
{
    if (QWidget * target = focusWidget())
        target->clearFocus();
    captureWidget();
    QQuickPaintedItem::focusOutEvent(event);
}
