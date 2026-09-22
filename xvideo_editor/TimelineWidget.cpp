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
static const double kTrackGap = 8.0;            //相邻轨道行间距
static const double kSceneWidth = 12000.0;
static const double kSceneHeight = 260.0;
static const double kMinPixelPerSecond = 1e-6;   //无限缩小: 仅保留极小下限防除零
static const double kMaxPixelPerSecond = 200.0;

//某轨道片段所在的行纵坐标(顶部标尺下方, 每条轨道一行)
static double trackRowY(int trackIndex)
{
    return kTrackTop + trackIndex * (kTrackHeight + kTrackGap);
}

//场景总高度: 标尺 + 所有轨道行 + 底部留白
static double sceneHeightWith(int maxTrackIndex)
{
    return kRulerHeight + kTrackGap +
           (maxTrackIndex + 1) * kTrackHeight + kTrackGap;
}

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

//按片段类型取固定配色: 不同类型颜色不同, 相同类型颜色相同
static QColor clipColor(ClipType t)
{
    switch (t) {
    case ClipType::VIDEO:    return QColor(70, 160, 255);
    case ClipType::AUDIO:    return QColor(90, 200, 120);
    case ClipType::STICKER:  return QColor(230, 110, 250);
    case ClipType::SUBTITLE: return QColor(255, 190, 80);
    }
    return QColor(150, 150, 150);
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
    //可见窗口 = 时间轴上实际展现的片段区段
    vis_x_ = msToX(c.pos_ms);
    vis_y_ = trackRowY(c.trackIndex);
    vis_w_ = std::max(msToX(c.duration_ms()), 1.0);
    //整条 = 原片总长: 左边灰 = 截取起点前被裁掉的部分, 右边灰 = 截取终点后未用的部分
    double fullW = std::max(msToX(c.source_length_ms()), vis_w_);
    setRect(QRectF(vis_x_ - msToX(c.source_start_ms), vis_y_, fullW, kTrackHeight));
    update();
}

void ClipItem::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
    double x = ev->scenePos().x();
    double visRight = vis_x_ + vis_w_;
    if (mode_ == None && x - vis_x_ <= 8.0 && x >= vis_x_ - 8.0)
        mode_ = LeftTrim;
    else if (mode_ == None && visRight - x <= 8.0 && x <= visRight + 8.0)
        mode_ = RightTrim;
    else if (x >= vis_x_ && x <= visRight)
        mode_ = MoveMode;
    else
        mode_ = None;               //灰色不可见区: 原片总长不可调整, 不响应剪切
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
    if (mode_ == MoveMode) {
        //整体移动, 限制不小于0
        c.pos_ms = std::max(0LL, orig_pos_ms_ + dms);
    } else if (mode_ == LeftTrim) {
        //左边缘: 时间轴位置与源起点同步移动, 右边缘固定; 
        //窗口只能在原片总长内滑动(起点不越过0与终点)
        long long dclamp = std::max(-orig_start_ms_,
                                    std::min(dms, orig_dur_ms_));
        c.pos_ms = orig_pos_ms_ + dclamp;
        c.source_start_ms = orig_start_ms_ + dclamp;
    } else if (mode_ == RightTrim) {
        //右边缘: 只改片段终点; 终点不得超过原片总长(原片长度不可调整)
        long long limit = c.source_length_ms() - orig_start_ms_;
        long long nd = std::max(1LL, std::min(orig_dur_ms_ + dms, limit));
        c.source_end_ms = orig_start_ms_ + nd;
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
    double visRight = vis_x_ + vis_w_;
    if ((x - vis_x_ <= 8.0 && x >= vis_x_ - 8.0) ||
        (visRight - x <= 8.0 && x <= visRight + 8.0))
        setCursor(Qt::SizeHorCursor);
    else if (x >= vis_x_ && x <= visRight)
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();
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
    QRectF full = rect();
    QRectF vis(vis_x_, vis_y_, vis_w_, kTrackHeight);

    //1) 灰色底 = 原片全片(不可见区: 截取范围之外的部分), 原片长度固定不可调整
    p->setPen(QPen(QColor(55, 55, 55), 1.5));
    p->setBrush(QColor(105, 105, 105));
    p->drawRoundedRect(full, 6, 6);

    //2) 亮色 = 可见区(类型固定颜色: 同类型同色, 不同类型异色)
    QColor base = clipColor(c.type);
    p->setPen(QPen(QColor(25, 25, 25), 1.5));
    p->setBrush(base);
    p->drawRoundedRect(vis, 4, 4);

    //3) 左右边缘裁剪把手(可见区边界)
    double hw = 6.0;
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(255, 255, 255, 230));
    p->drawRect(QRectF(vis.left(), vis.top(), hw, vis.height()));
    p->drawRect(QRectF(vis.right() - hw, vis.top(), hw, vis.height()));
    p->setPen(QPen(QColor(70, 70, 70), 1));
    double cx = vis.left() + hw / 2;
    p->drawLine(QPointF(cx, vis.top() + 12), QPointF(cx, vis.bottom() - 12));
    cx = vis.right() - hw / 2;
    p->drawLine(QPointF(cx, vis.top() + 12), QPointF(cx, vis.bottom() - 12));

    //4) 文本(画在可见区内)
    QString txt = QString("片段%1\n时间轴 %2 ~ %3\n源 %4 ~ %5 / 全片 %6\n%7")
                      .arg(index_ + 1)
                      .arg(msToText(c.pos_ms))
                      .arg(msToText(c.pos_ms + c.duration_ms()))
                      .arg(msToText(c.source_start_ms))
                      .arg(msToText(c.source_end_ms))
                      .arg(msToText(c.source_length_ms()))
                      .arg(QFileInfo(c.url).fileName());
    p->setPen(Qt::white);
    p->drawText(vis.adjusted(12, 10, -12, -10),
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

    //场景高度按最高轨道动态扩展, 每条轨道一行
    int maxTrack = 0;
    for (int i = 0; i < model_->clipCount(); ++i)
        maxTrack = std::max(maxTrack, model_->clip(i).trackIndex);
    const double sceneH = sceneHeightWith(maxTrack);

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
    playhead_ = scene_->addLine(QLineF(0, 0, 0, sceneH), QPen(Qt::red, 2));
    playhead_->setZValue(100);
    setPlayheadMs(playhead_ms_);
    //重新延展场景尺寸
    long long total = model_->total_ms();
    double w = msToX(total) + 200;
    w = std::max(w, kSceneWidth);
    sceneH_ = sceneH;
    scene_->setSceneRect(0, 0, w, sceneH);
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
    playhead_->setLine(x, 0, x, sceneH_);
    //播放头超出可视范围时扩展场景
    QRectF sr = scene_->sceneRect();
    if (x > sr.right() - 30)
        scene_->setSceneRect(0, 0, x + 100, sceneH_);
    //仅在自动跟随模式下随播放头居中(用户手动滚动/编辑时保持不动)
    if (follow_playhead_)
        centerOn(x, sceneH_ / 2);
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