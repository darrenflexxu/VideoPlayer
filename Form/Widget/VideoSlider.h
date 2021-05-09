#ifndef VIDEOSLIDER_H
#define VIDEOSLIDER_H

#include <QWidget>
#include <QSlider>

namespace Ui {
class VideoSlider;
}

class VideoSlider : public QSlider {
    Q_OBJECT

public:
    explicit VideoSlider(QWidget *parent = 0);
    ~VideoSlider();
    void setValue(int value);

signals:
    void sig_clicked(qint64 mSec);
    void sig_setStart(qint64 mSec);
    void sig_setEnd(qint64 mSec);
    void sig_valueChanged(int);

private slots:
    void slotMousemoveTimerTimeOut();

protected:
    void resizeEvent(QResizeEvent *);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void leaveEvent(QEvent *);

private:
    QTimer * m_timer_mousemove;
    int m_posX;
    bool isSliderMoving;    
};

#endif // VIDEOSLIDERVIEW_H