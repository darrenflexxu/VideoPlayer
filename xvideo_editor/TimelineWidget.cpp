#include "TimelineWidget.h"
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QCursor>
#include <QFileInfo>
#include <algorithm>

//轨道与场景尺寸
static const double kTrackTop = 20.0;
static const double kTrackHeight = 180.0;
static const double kSceneWidth = 12000.0;
static const double kSceneHeight = 260.0;

//毫秒 -> 时间字符串 (mm:ss / h:mm:ss)
static QString msToText(long long ms)
{
    if (ms < 0) ms = 0;
    long long sec = ms / 1000;
    long long h = sec / 3600;
    long long m = (sec % 3600) / 60;
    long long s = sec % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

//------------------ ClipItem ------------------

ClipItem::ClipItem(int index, TimelineModel* model, double pps, QGraphicsItem* parent)
    : QGraphicsRectItem(parent), index_(index), model_(model), pps_(pps)
{
    setAcceptHoverEvents(true);
    syncFromModel();
}

void ClipItem::syncFromModel()
{
    const Clip& c = model_->clip(index_);
    //始终从 0 时长起可见(宽度1px便于抓取)
    double w = std::max(msToX(c.duration_ms()), 1.0);
    setRect(QRectF(msToX(c.pos_ms), kTrackTop, w, kTrackHeight));
    update();
}

void ClipItem::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    double x = ev->scenePos().x();
    const QRectF r = rect();
    if (x - r.left() <= 8.0)
        mode_ = LeftTrim;
    else if (r.right() - x <= 8.0)
        mode_ = RightTrim;
    else
        mode_ = MoveMode;
    press_scene_ = ev->scenePos();
    const Clip& c = model_->clip(index_);
    orig_pos_ms_ = c.pos_ms;
    orig_start_ms_ = c.source_start_ms;
    orig_end_ms_ = c.source_end_ms;
    orig_dur_ms_ = c.duration_ms();
    ev->accept();
}

void ClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
    if (mode_ == None) {
        ev->ignore();
        return;
    }
    double dpx = ev->scenePos().x() - press_scene_.x();
    long long dms = (long long)(dpx * 1000.0 / pps_);

    Clip& c = model_->clip(index_);
    switch (mode_) {
    case MoveMode:
        //整体移动, 限制不小于0
        c.pos_ms = std::max(0LL, orig_pos_ms_ + dms);
        break;
    case LeftTrim: {
        //左边缘: 时间轴位置与源起点同步移动, 右边缘固定
        long long dclamp = std::max(-orig_start_ms_,
                                    std::min(dms, orig_dur_ms_));
        c.pos_ms = orig_pos_ms_ + dclamp;
        c.source_start_ms = orig_start_ms_ + dclamp;
        break;
    }
    case RightTrim: {
        //右边缘: 只改片段终点/源终点, 左边缘固定
        long long nd = std::max(0LL, orig_dur_ms_ + dms);
        c.source_end_ms = orig_start_ms_ + nd;
        break;
    }
    default:
        break;
    }
    syncFromModel();
    ev->accept();
}

void ClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
    if (mode_ == None) {
        ev->ignore();
        return;
    }
    mode_ = None;
    if (changed) changed();
    ev->accept();
}

void ClipItem::hoverMoveEvent(QGraphicsSceneHoverEvent* ev)
{
    double x = ev->scenePos().x();
    const QRectF r = rect();
    if (x - r.left() <= 8.0 || r.right() - x <= 8.0)
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::OpenHandCursor);
    ev->accept();
}

void ClipItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* ev)
{
    unsetCursor();
    ev->accept();
}

void ClipItem::paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w)
{
    Q_UNUSED(opt);
    Q_UNUSED(w);
    const Clip& c = model_->clip(index_);
    QRectF r = rect();

    //片段底色
    QColor fill = QColor::fromHsv((index_ * 47) % 360, 120, 150);
    p->setPen(QPen(QColor(30, 30, 30), 1.5));
    p->setBrush(fill);
    p->drawRoundedRect(r, 6, 6);

    //左右边缘裁剪把手
    double hw = 6.0;
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 220));
    p->drawRect(QRectF(r.left(), r.top(), hw, r.height()));
    p->drawRect(QRectF(r.right() - hw, r.top(), hw, r.height()));
    p->setPen(QPen(QColor(90, 90, 90), 1));
    double cx = r.left() + hw / 2;
    p->drawLine(QPointF(cx, r.top() + 12), QPointF(cx, r.bottom() - 12));
    cx = r.right() - hw / 2;
    p->drawLine(QPointF(cx, r.top() + 12), QPointF(cx, r.bottom() - 12));

    //文本
    QString txt = QString("片段%1\n时间轴 %2 ~ %3\n源 %4 ~ %5\n%6")
                      .arg(index_ + 1)
                      .arg(msToText(c.pos_ms))
                      .arg(msToText(c.pos_ms + c.duration_ms()))
                      .arg(msToText(c.source_start_ms))
                      .arg(msToText(c.source_end_ms))
                      .arg(QFileInfo(c.url).fileName());
    p->setPen(Qt::white);
    p->drawText(r.adjusted(12, 10, -12, -10),
                Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, txt);
}

//------------------ TimelineWidget ------------------

TimelineWidget::TimelineWidget(TimelineModel* model, QWidget* parent)
    : QGraphicsView(parent), model_(model)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setBackgroundBrush(QColor(50, 50, 50));
    setFrameShape(QFrame::NoFrame);
    scene_->setSceneRect(0, 0, kSceneWidth, kSceneHeight);
    refresh();
}

void TimelineWidget::setPixelPerSecond(double pps)
{
    pixelPerSecond_ = std::max(1.0, pps);
    refresh();
}

void TimelineWidget::refresh()
{
    if (!scene_) return;
    scene_->clear();
    playhead_ = nullptr;
    for (int i = 0; i < model_->clipCount(); ++i) {
        ClipItem* it = new ClipItem(i, model_, pixelPerSecond_);
        it->changed = [this]() { emit timelineChanged(); };
        scene_->addItem(it);
    }
    //播放头
    playhead_ = scene_->addLine(QLineF(0, 0, 0, kSceneHeight), QPen(Qt::red, 2));
    playhead_->setZValue(100);
    setPlayheadMs(playhead_ms_);
    //重新延展场景宽度
    long long total = model_->total_ms();
    double w = msToX(total) + 200;
    w = std::max(w, kSceneWidth);
    scene_->setSceneRect(0, 0, w, kSceneHeight);
}

void TimelineWidget::setPlayheadMs(long long ms)
{
    playhead_ms_ = std::max(0LL, ms);
    if (!playhead_) return;
    double x = msToX(playhead_ms_);
    playhead_->setLine(x, 0, x, kSceneHeight);
    //播放头超出可视范围时扩展场景
    QRectF sr = scene_->sceneRect();
    if (x > sr.right() - 30)
        scene_->setSceneRect(0, 0, x + 100, kSceneHeight);
    centerOn(x, kSceneHeight / 2);
}

void TimelineWidget::mousePressEvent(QMouseEvent* ev)
{
    QGraphicsView::mousePressEvent(ev);
    if (ev->button() == Qt::LeftButton) {
        //空白处点击 -> 定位播放头
        if (!clipItemAt(ev->pos()))
            emit seekRequested(xToMs(mapToScene(ev->pos()).x()));
    }
}

ClipItem* TimelineWidget::clipItemAt(const QPoint& view_pos) const
{
    const QList<QGraphicsItem*> items = scene_->items(mapToScene(view_pos));
    for (QGraphicsItem* it : items) {
        ClipItem* c = dynamic_cast<ClipItem*>(it);
        if (c) return c;
    }
    return nullptr;
}