#include "VideoSlider.h"
#include <QDebug>
#include <QTimer>
#include <QResizeEvent>
#include <QStyle>

VideoSlider::VideoSlider(QWidget *parent): QSlider(parent) {
    setMouseTracking(true);
    this->setOrientation(Qt::Horizontal);
    isSliderMoving = false;
    m_timer_mousemove = new QTimer;
    m_timer_mousemove->setInterval(100);
    connect(m_timer_mousemove, SIGNAL(timeout()), this, SLOT(slotMousemoveTimerTimeOut()));
}

VideoSlider::~VideoSlider() {
}

void VideoSlider::resizeEvent(QResizeEvent *event) {
}

void VideoSlider::mousePressEvent(QMouseEvent *event) {
    int posX = event->pos().x();
    int w = this->width();
    int value = QStyle::sliderValueFromPosition(minimum(), maximum(), posX, w);
    setValue(value);
    emit sig_valueChanged(value);
}

void VideoSlider::mouseMoveEvent(QMouseEvent *event) {
    m_posX = event->pos().x();
    m_timer_mousemove->stop();
    m_timer_mousemove->start();
    QSlider::mouseMoveEvent(event);
}

void VideoSlider::leaveEvent(QEvent *) {
    m_timer_mousemove->stop();
}

void VideoSlider::setValue(int value) {
    if (!isSliderMoving) {
        QSlider::setValue(value);
    }
}

void VideoSlider::slotMousemoveTimerTimeOut() {
    m_timer_mousemove->stop();
    int w = this->width();
    QPoint point = this->mapToGlobal(QPoint(0, 0));
    int width = this->width();

    if (parent() != NULL) {
        QWidget *widget = (QWidget *)this->parent();
        point = widget->mapToGlobal(QPoint(0, 0));
        width = widget->width();
    }
}