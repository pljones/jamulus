/******************************************************************************\
 * Copyright (c) 2023
 *
 * Author(s):
 *  Peter L Jones
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
\******************************************************************************/
#pragma once

#include <QGesture>
#include <QPanGesture>
#include <QPinchGesture>
#include <QSwipeGesture>
#include <QTapAndHoldGesture>
#include <QTapGesture>

#include <QWidget>
#include <QGroupBox>
#include <QMessageBox>

class CGGroupBox : public QGroupBox
{

    Q_OBJECT

public:
    CGGroupBox ( QWidget* parent = nullptr ) : QGroupBox ( parent )
    {
        grabGesture ( Qt::PanGesture );
        grabGesture ( Qt::PinchGesture );
        grabGesture ( Qt::SwipeGesture );
        grabGesture ( Qt::TapAndHoldGesture );
        grabGesture ( Qt::TapGesture );
        grabGesture ( Qt::CustomGesture );
    }

    bool event ( QEvent* event )
    {
        if ( event->type() != QEvent::Gesture )
        {
            return QGroupBox::event ( event );
        }

QMessageBox::information(this, "Jamulus", "CGGroupBox gesture detected");

        QGestureEvent* gestureEvent = static_cast<QGestureEvent*> ( event );
        QGesture*      gesture      = nullptr;

        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::PanGesture ) ) != nullptr )
        {
            emit panGestureSignal ( gestureEvent, static_cast<QPanGesture*> ( gesture ) );
        }
        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::PinchGesture ) ) != nullptr )
        {
            emit pinchGestureSignal ( gestureEvent, static_cast<QPinchGesture*> ( gesture ) );
        }
        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::SwipeGesture ) ) != nullptr )
        {
            emit swipeGestureSignal ( gestureEvent, static_cast<QSwipeGesture*> ( gesture ) );
        }
        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::TapAndHoldGesture ) ) != nullptr )
        {
            emit tapAndHoldGestureSignal ( gestureEvent, static_cast<QTapAndHoldGesture*> ( gesture ) );
        }
        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::TapGesture ) ) != nullptr )
        {
            emit tapGestureSignal ( gestureEvent, static_cast<QTapGesture*> ( gesture ) );
        }
        if ( !event->isAccepted() && ( gesture = gestureEvent->gesture ( Qt::CustomGesture ) ) != nullptr )
        {
            emit customGestureSignal ( gestureEvent, gesture );
        }

        return event->isAccepted() ? true : QGroupBox::event ( event );
    }

signals:
    void panGestureSignal ( QGestureEvent* gestureEvent, QPanGesture* gesture );
    void pinchGestureSignal ( QGestureEvent* gestureEvent, QPinchGesture* gesture );
    void swipeGestureSignal ( QGestureEvent* gestureEvent, QSwipeGesture* gesture );
    void tapAndHoldGestureSignal ( QGestureEvent* gestureEvent, QTapAndHoldGesture* gesture );
    void tapGestureSignal ( QGestureEvent* gestureEvent, QTapGesture* gesture );
    void customGestureSignal ( QGestureEvent* gestureEvent, QGesture* gesture );
};
