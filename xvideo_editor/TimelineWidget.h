#pragma once
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <functional>
#include "TimelineModel.h"

class QGraphicsScene;
class QGraphicsLineItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneHoverEvent;
class QMouseEvent;
class QWheelEvent;
class TimelineRuler;

//时间轴上的片段条目: 支持整体拖动、左右边缘裁剪
class ClipItem : public QGraphicsRectItem
{
public:
    enum Mode { None, MoveMode, LeftTrim, RightTrim };

    ClipItem(int index, TimelineModel* model, double pps, QGraphicsItem* parent = nullptr);
    int index() const { return index_; }

    //按模型重设几何
    void syncFromModel();

    // 拖动/裁剪结束后的回调(通知视图发 timelineChanged 信号)
    std::function<void()> changed;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* ev) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* ev) override;
    void paint(QPainter* p, const QStyleOptionGraphicsItem* opt, QWidget* w) override;

private:
    double msToX(long long ms) const { return (double)ms * pps_ / 1000.0; }

    int index_ = -1;
    TimelineModel* model_ = nullptr;
    double pps_ = 50.0;        //每秒占像素
    Mode mode_ = None;
    QPointF press_scene_;
    long long orig_pos_ms_ = 0, orig_start_ms_ = 0, orig_end_ms_ = 0, orig_dur_ms_ = 0;

    //可见窗口几何(场景坐标): 灰色为原片不可见区, 亮色为可见区
    double vis_x_ = 0.0, vis_y_ = 0.0, vis_w_ = 0.0;
};

//时间轴视图(View): 展示片段并驱动播放头
class TimelineWidget : public QGraphicsView
{
    Q_OBJECT
public:
    explicit TimelineWidget(TimelineModel* model, QWidget* parent = nullptr);

    void setPixelPerSecond(double pps);
    void setPlayheadMs(long long ms);
    long long playheadMs() const { return playhead_ms_; }

    //恢复播放头自动跟随浏览(用户手动滚动/编辑后调用)
    void resumeFollow() { follow_playhead_ = true; }

    //模型变化后重建场景
    void refresh();
    double msToX(long long ms) const { return (double)ms * pixelPerSecond_ / 1000.0; }
    long long xToMs(double x) const { return (long long)(std::max(0.0, x) * 1000.0 / pixelPerSecond_); }

    //缩放时间轴(factor>1放大), 保持 anchor(场景坐标)位置不变
    void zoomBy(double factor, const QPointF& anchor);
    double pixelPerSecond() const { return pixelPerSecond_; }

signals:
    void seekRequested(long long timeline_ms);
    void timelineChanged();

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;

private:
    ClipItem* clipItemAt(const QPoint& view_pos) const;

    TimelineModel* model_ = nullptr;
    QGraphicsScene* scene_ = nullptr;
    QGraphicsLineItem* playhead_ = nullptr;
    TimelineRuler* ruler_ = nullptr;
    double pixelPerSecond_ = 50.0;   //每秒占像素
    long long playhead_ms_ = 0;
    double sceneH_ = 106.0;          //当前场景总高度(随轨道数扩展)
    bool follow_playhead_ = true;    //播放时是否自动跟随播放头(用户滚动/编辑时关闭)
};