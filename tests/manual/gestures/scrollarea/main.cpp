// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#undef QT_NO_FOREACH // this file contains unported legacy Q_FOREACH uses

#include <QApplication>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGestureEvent>
#include <QPanGesture>
#include <QDebug>

#include "mousepangesturerecognizer.h"

class ScrollArea : public QScrollArea
{
    Q_OBJECT
public:
    ScrollArea(QWidget *parent = nullptr)
        : QScrollArea(parent), outside(false)
    {
        viewport()->grabGesture(Qt::PanGesture, Qt::ReceivePartialGestures);
    }

protected:
    bool viewportEvent(QEvent *event)
    {
        if (event->type() == QEvent::Gesture) {
            gestureEvent(static_cast<QGestureEvent *>(event));
            return true;
        } else if (event->type() == QEvent::GestureOverride) {
            QGestureEvent *ge = static_cast<QGestureEvent *>(event);
            if (QPanGesture *pan = static_cast<QPanGesture *>(ge->gesture(Qt::PanGesture)))
                if (pan->state() == Qt::GestureStarted) {
                    outside = false;
                }
        }
        return QScrollArea::viewportEvent(event);
    }
    void gestureEvent(QGestureEvent *event)
    {
        QPanGesture *pan = static_cast<QPanGesture *>(event->gesture(Qt::PanGesture));
        if (pan) {
            switch(pan->state()) {
            case Qt::GestureStarted: qDebug() << this << "Pan: started"; break;
            case Qt::GestureFinished: qDebug() << this << "Pan: finished"; break;
            case Qt::GestureCanceled: qDebug() << this << "Pan: canceled"; break;
            case Qt::GestureUpdated: break;
            default: qDebug() << this << "Pan: <unknown state>"; break;
            }

            if (pan->state() == Qt::GestureStarted)
                outside = false;
            event->ignore();
            event->ignore(pan);
            if (outside)
                return;

            const QPointF delta = pan->delta();
            const QPointF totalOffset = pan->offset();
            QScrollBar *vbar = verticalScrollBar();
            QScrollBar *hbar = horizontalScrollBar();

            if ((vbar->value() == vbar->minimum() && totalOffset.y() > 10) ||
                (vbar->value() == vbar->maximum() && totalOffset.y() < -10)) {
                outside = true;
                return;
            }
            if ((hbar->value() == hbar->minimum() && totalOffset.x() > 10) ||
                (hbar->value() == hbar->maximum() && totalOffset.x() < -10)) {
                outside = true;
                return;
            }
            vbar->setValue(vbar->value() - delta.y());
            hbar->setValue(hbar->value() - delta.x());
            event->accept(pan);
        }
    }

private:
    bool outside;
};

class Slider : public QSlider
{
public:
    Slider(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent)
    {
        grabGesture(Qt::PanGesture);
    }
protected:
    bool event(QEvent *event)
    {
        if (event->type() == QEvent::Gesture) {
            gestureEvent(static_cast<QGestureEvent *>(event));
            return true;
        }
        return QSlider::event(event);
    }
    void gestureEvent(QGestureEvent *event)
    {
        QPanGesture *pan = static_cast<QPanGesture *>(event->gesture(Qt::PanGesture));
        if (pan) {
            switch (pan->state()) {
            case Qt::GestureStarted: qDebug() << this << "Pan: started"; break;
            case Qt::GestureFinished: qDebug() << this << "Pan: finished"; break;
            case Qt::GestureCanceled: qDebug() << this << "Pan: canceled"; break;
            case Qt::GestureUpdated: break;
            default: qDebug() << this << "Pan: <unknown state>"; break;
            }

            if (pan->state() == Qt::GestureStarted)
                outside = false;
            event->ignore();
            event->ignore(pan);
            if (outside)
                return;
            const QPointF delta = pan->delta();
            const QPointF totalOffset = pan->offset();
            if (orientation() == Qt::Horizontal) {
                if ((value() == minimum() && totalOffset.x() < -10) ||
                    (value() == maximum() && totalOffset.x() > 10)) {
                    outside = true;
                    return;
                }
                if (totalOffset.y() < 40 && totalOffset.y() > -40) {
                    setValue(value() + delta.x());
                    event->accept(pan);
                } else {
                    outside = true;
                }
            } else if (orientation() == Qt::Vertical) {
                if ((value() == maximum() && totalOffset.y() < -10) ||
                    (value() == minimum() && totalOffset.y() > 10)) {
                    outside = true;
                    return;
                }
                if (totalOffset.x() < 40 && totalOffset.x() > -40) {
                    setValue(value() - delta.y());
                    event->accept(pan);
                } else {
                    outside = true;
                }
            }
        }
    }
private:
    bool outside;
};

class MainWindow : public QMainWindow
{
public:
    MainWindow()
    {
        rootScrollArea = new ScrollArea;
        rootScrollArea->setObjectName(QLatin1String("rootScrollArea"));
        setCentralWidget(rootScrollArea);

        QWidget *root = new QWidget;
        root->setFixedSize(3000, 3000);
        rootScrollArea->setWidget(root);

        Slider *verticalSlider = new Slider(Qt::Vertical, root);
        verticalSlider->setObjectName(QLatin1String("verticalSlider"));
        verticalSlider ->move(650, 1100);
        Slider *horizontalSlider = new Slider(Qt::Horizontal, root);
        horizontalSlider->setObjectName(QLatin1String("horizontalSlider"));
        horizontalSlider ->move(600, 1000);

        childScrollArea = new ScrollArea(root);
        childScrollArea->setObjectName(QLatin1String("childScrollArea"));
        childScrollArea->move(500, 500);
        QWidget *w = new QWidget;
        w->setMinimumWidth(700);
        QVBoxLayout *l = new QVBoxLayout(w);
        l->setContentsMargins(20, 20, 20, 20);
        for (int i = 0; i < 100; ++i) {
            QWidget *w = new QWidget;
            QHBoxLayout *ll = new QHBoxLayout(w);
            ll->addWidget(new QLabel(QString("Label %1").arg(i)));
            ll->addWidget(new QPushButton(QString("Button %1").arg(i)));
            l->addWidget(w);
        }
        childScrollArea->setWidget(w);
#if defined(Q_OS_WIN)
        // Windows can force Qt to create a native window handle for an
        // intermediate widget and that will block gesture to get touch events.
        // So this hack to make sure gestures get all touch events they need.
        foreach (QObject *w, children())
            if (w->isWidgetType())
                static_cast<QWidget *>(w)->setAttribute(Qt::WA_AcceptTouchEvents);
#endif
    }
private:
    ScrollArea *rootScrollArea;
    ScrollArea *childScrollArea;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QGestureRecognizer::registerRecognizer(new MousePanGestureRecognizer);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"
