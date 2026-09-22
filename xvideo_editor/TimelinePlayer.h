#pragma once
#include "xplayer.h"
#include "TimelineModel.h"

class XQtOpenGLWidget;

//时间轴播放控制器(Controller): 负责跨片段切换与预览
class TimelinePlayer
{
public:
    void setModel(TimelineModel* m) { model_ = m; }
    void setView(XQtOpenGLWidget* w);

    //从当前播放头继续播放
    bool play();
    void pause() { paused_ = true; if (player_) player_->Pause(true); }
    bool paused() const { return paused_; }

    //定位到时间轴某个位置并开始播放
    bool seekTo(long long timeline_ms);

    //每帧调用: 渲染一帧并处理片段边界切换
    void Update();

    long long pos_ms() const { return timeline_pos_ms_; }
    long long total_ms() const { return model_ ? model_->total_ms() : 0; }
    int activeIndex() const { return active_idx_; }
    void set_volume(float v) { if (player_) player_->SetVolume(v); }
    void set_speed(float s) { if (player_) player_->SetSpeed(s); }

private:
    bool switchTo(int idx);   //打开并开始第idx段
    void releasePlayer();     //停掉并释放当前播放器

    XPlayer* player_ = nullptr;   //每个片段独立播放器实例(重开不再复用旧解码状态)
    TimelineModel* model_ = nullptr;
    XQtOpenGLWidget* view_ = nullptr;
    int active_idx_ = -1;            //当前打开的片段索引, -1=尚未打开
    bool paused_ = true;
    long long timeline_pos_ms_ = 0;  //时间轴上的位置
};