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

#ifndef AUDACIOUS_KIRIGAMI_NATIVE_WIDGET_ITEM_H
#define AUDACIOUS_KIRIGAMI_NATIVE_WIDGET_ITEM_H

#include <QImage>
#include <QMutex>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>

class QWidget;

class NativeWidgetItem : public QQuickPaintedItem
{
public:
    explicit NativeWidgetItem(QQuickItem * parent = nullptr);
    ~NativeWidgetItem() override;

    void setWidget(QWidget * widget);

    void paint(QPainter * painter) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

protected:
    void geometryChange(const QRectF & new_geometry,
                        const QRectF & old_geometry) override;
    void itemChange(ItemChange change, const ItemChangeData & data) override;
    void keyPressEvent(QKeyEvent * event) override;
    void keyReleaseEvent(QKeyEvent * event) override;
    void inputMethodEvent(QInputMethodEvent * event) override;
    void focusInEvent(QFocusEvent * event) override;
    void focusOutEvent(QFocusEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;
    void mouseReleaseEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;
    void mouseUngrabEvent() override;
    void wheelEvent(QWheelEvent * event) override;
    void hoverEnterEvent(QHoverEvent * event) override;
    void hoverMoveEvent(QHoverEvent * event) override;
    void hoverLeaveEvent(QHoverEvent * event) override;

private:
    void syncWidgetGeometry();
    void captureWidget();
    QWidget * widgetAt(const QPointF & position) const;
    QWidget * focusWidget() const;
    QPointF mapToWidget(QWidget * widget, const QPointF & position) const;
    void sendMouseEvent(QMouseEvent * event, QEvent::Type type,
                        QWidget * target = nullptr);
    void updateHover(QWidget * target, const QPointF & position,
                     const QPointF & old_position,
                     const QPointF & global_position,
                     Qt::KeyboardModifiers modifiers);

    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_mouse_target;
    QPointer<QWidget> m_hover_target;
    QTimer m_refresh_timer;
    mutable QMutex m_frame_mutex;
    QImage m_frame;
};

#endif
