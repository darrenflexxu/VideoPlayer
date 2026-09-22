#include "TimelineWidget.h"
#include <QGraphicsScene>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QCursor>
#include <QFileInfo>
#include <algorithm>

//轨道与场景尺寸
static const double kRulerHeight = 26.0;
static const double kTrackTop = 34.0;
static const double kTrackHeight = 180.0;
static const double kSceneWidth = 12000.0;
static const double kSceneHeight = 260.0;
static const double kMinPixelPerSecond = 1e-6;   //无限缩小: 仅保留极小下限防除零
static const double kMaxPixelPerSecond = 200.0;

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

//按缩放比例选择"漂亮"的刻度间隔(毫秒), 使相邻刻度间距 >= 90px;
//系数按 1/2/5 十进制递增, 支持任意小的缩放比例(刻度间隔可无限增大)
static long long rulerStepMs(double pps)
{
    const double pxPerMs = pps / 1000.0;
    if (pxPerMs <= 0)
        return 3600000LL;
    const long long nice[] = { 1, 2, 5 };
    long long magnitudes = 1;
    for (;;) {
        for (long long d : nice) {
            long long step = d * magnitudes;
            if ((double)step * pxPerMs >= 90.0)
                return step;
        }
        magnitudes *= 10;
    }
}

//顶部时间标尺: 用场景坐标绘制时间刻度与标签
class TimelineRuler : public QGraphicsItem
{
public:
    explicit TimelineRuler(double pps) : pps_(pps) {}

    void setPixelPerSecond(double pps)
    {
        pps_ = pps;
        prepareGeometryChange();
        update();
    }

    QRectF boundingRect() const override
    {
        if (!scene()) return QRectF(0, 0, 1, kRulerHeight);
        return QRectF(0, 0, std::max(1.0, scene()->sceneRect().right()), kRulerHeight);
    }

    void paint(QPainter* p, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        if (!scene()) return;
        const double right = scene()->sceneRect().right();
        const double pxPerMs = pps_ / 1000.0;
        if (pxPerMs <= 0) return;

        QFont f = p->font();
        f.setPixelSize(10);
        p->setFont(f);

        //标尺底色
        p->fillRect(QRectF(0, 0, right, kRulerHeight), QColor(58, 58, 58));

        const long long step = rulerStepMs(pps_);
        const long long minor = step / 5;   //次要刻度
        bool drawMinor = (double)minor * pxPerMs >= 5.0;
        const long long maxMs = (long long)(right / pxPerMs) + 1;

        //超长时间轴(高倍缩放下刻度过多)时加大主刻度间隔, 避免单帧绘制开销过大
        long long majorStep = step;
        while (majorStep > 0 && maxMs / majorStep > 2000)
            majorStep *= 2;
        if ((double)minor * pxPerMs < 5.0 || maxMs / (majorStep / 5) > 4000)
            drawMinor = false;

        p->setPen(QColor(90, 90, 90));
        if (drawMinor) {
            for (long long ms = 0; ms <= maxMs; ms += minor) {
                double x = (double)ms * pxPerMs;
                bool isMajor = (ms % majorStep) == 0;
                if (isMajor)
                    p->drawLine(QPointF(x, 0), QPointF(x, kRulerHeight));
                else
                    p->drawLine(QPointF(x, kRulerHeight - 8), QPointF(x, kRulerHeight));
            }
        } else {
            for (long long ms = 0; ms <= maxMs; ms += majorStep) {
                double x = (double)ms * pxPerMs;
                p->drawLine(QPointF(x, 0), QPointF(x, kRulerHeight));
            }
        }

        //主刻度标签
        p->setPen(QColor(210, 210, 210));
        for (long long ms = 0; ms <= maxMs; ms += majorStep) {
            double x = (double)ms * pxPerMs;
            p->drawText(QPointF(x + 3, kRulerHeight - 7), msToText(ms));
        }
    }

private:
    double pps_;
};

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
    //用户拖拽底部滑块时取消自动跟随, 避免与播放头回中打架
    connect(horizontalScrollBar(), &QScrollBar::sliderPressed, this,
            [this]() { follow_playhead_ = false; });
    refresh();
}

void TimelineWidget::setPixelPerSecond(double pps)
{
    pixelPerSecond_ = std::max(kMinPixelPerSecond, pps);
    refresh();
}

void TimelineWidget::refresh()
{
    if (!scene_) return;
    scene_->clear();
    playhead_ = nullptr;
    ruler_ = nullptr;
    for (int i = 0; i < model_->clipCount(); ++i) {
        ClipItem* it = new ClipItem(i, model_, pixelPerSecond_);
        it->changed = [this]() { emit timelineChanged(); };
        scene_->addItem(it);
    }
    //时间标尺
    ruler_ = new TimelineRuler(pixelPerSecond_);
    ruler_->setZValue(50);
    scene_->addItem(ruler_);
    //播放头
    playhead_ = scene_->addLine(QLineF(0, 0, 0, kSceneHeight), QPen(Qt::red, 2));
    playhead_->setZValue(100);
    setPlayheadMs(playhead_ms_);
    //重新延展场景宽度
    long long total = model_->total_ms();
    double w = msToX(total) + 200;
    w = std::max(w, kSceneWidth);
    scene_->setSceneRect(0, 0, w, kSceneHeight);
    if (ruler_) ruler_->update();
}

void TimelineWidget::zoomBy(double factor, const QPointF& anchor)
{
    double nf = pixelPerSecond_ * factor;
    if (nf < kMinPixelPerSecond) nf = kMinPixelPerSecond;
    if (nf > kMaxPixelPerSecond) nf = kMaxPixelPerSecond;
    if (nf == pixelPerSecond_)
        return;
    pixelPerSecond_ = nf;
    refresh();          // 按新比例重建片段与标尺
    centerOn(anchor);   // 保持缩放位置不跳动
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
    //仅在自动跟随模式下随播放头居中(用户手动滚动/编辑时保持不动)
    if (follow_playhead_)
        centerOn(x, kSceneHeight / 2);
}

void TimelineWidget::mousePressEvent(QMouseEvent* ev)
{
    QGraphicsView::mousePressEvent(ev);
    if (ev->button() == Qt::LeftButton) {
        //空白处点击 -> 定位播放头, 并恢复跟随
        if (!clipItemAt(ev->pos())) {
            follow_playhead_ = true;
            emit seekRequested(xToMs(mapToScene(ev->pos()).x()));
        } else {
            //开始拖动/裁剪片段: 取消自动跟随, 避免视角被拽走
            follow_playhead_ = false;
        }
    }
}

void TimelineWidget::wheelEvent(QWheelEvent* ev)
{
    if (ev->modifiers() & Qt::ControlModifier) {
        //Ctrl+滚轮: 以光标处为锚点缩放时间轴
        double factor = ev->angleDelta().y() > 0 ? 1.25 : 1.0 / 1.25;
        follow_playhead_ = false;
        zoomBy(factor, mapToScene(ev->position().toPoint()));
        ev->accept();
        return;
    }
    follow_playhead_ = false;   //普通滚动等同手动浏览
    QGraphicsView::wheelEvent(ev);
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