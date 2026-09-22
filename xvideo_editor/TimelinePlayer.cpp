#include "TimelinePlayer.h"
#include "x_qt_opengl_widget.h"
#include <algorithm>

void TimelinePlayer::setView(XQtOpenGLWidget* w)
{
    view_ = w;
    if (player_)
        player_->set_view(w);
}

void TimelinePlayer::releasePlayer()
{
    if (!player_)
        return;
    player_->Stop();
    delete player_;
    player_ = nullptr;
}

bool TimelinePlayer::switchTo(int idx)
{
    if (!model_ || idx < 0 || idx >= model_->clipCount())
        return false;
    const Clip& c = model_->clip(idx);

    auto* p = new XPlayer();
    p->set_view(view_);
    p->set_gpu_decode(false);
    p->set_gpu_direct_render_(false);

    if (!p->Open(c.url.toLocal8Bit().constData(), nullptr)) {
        delete p;
        paused_ = true;
        timeline_pos_ms_ = c.pos_ms;
        return false;
    }
    // 新实例打开成功后再释放旧的, 打开失败时保留旧播放器状态
    releasePlayer();
    player_ = p;
    active_idx_ = idx;
    timeline_pos_ms_ = c.pos_ms;
    if (c.source_start_ms > 0)
        player_->Seek(c.source_start_ms);
    player_->Start();
    player_->Pause(paused_);
    return true;
}

bool TimelinePlayer::play()
{
    if (!model_ || model_->empty())
        return false;
    if (active_idx_ < 0) {
        // 尚未打开任何片段: 从播放头所在片段开始
        int idx = model_->clipAt(timeline_pos_ms_);
        if (idx < 0) idx = 0;
        paused_ = false;
        return switchTo(idx);
    }
    paused_ = false;
    if (player_)
        player_->Pause(false);
    return true;
}

bool TimelinePlayer::seekTo(long long timeline_ms)
{
    if (!model_ || model_->empty())
        return false;
    long long local = 0;
    int idx = model_->clipAt(timeline_ms, &local);
    if (idx < 0)
        return false;
    paused_ = false;
    if (idx != active_idx_) {
        if (!switchTo(idx))
            return false;
        if (player_)
            player_->Pause(false);
    } else if (player_) {
        player_->Pause(false);
    }
    // 片段内定位到目标位置
    if (player_)
        player_->Seek(local);
    timeline_pos_ms_ = model_->clip(idx).pos_ms + local;
    return true;
}

void TimelinePlayer::Update()
{
    if (!model_ || model_->empty() || active_idx_ < 0 || !player_)
        return;

    player_->Update();          // 渲染一帧 / 推进音频
    if (paused_)
        return;

    const Clip& c = model_->clip(active_idx_);
    long long local = player_->pos_ms();
    timeline_pos_ms_ = c.pos_ms + local;

    // 片段用尽(带容差)或源已放完 -> 切到下一段
    bool exhausted = local >= c.duration_ms() - 60 || player_->IsFinish();
    if (!exhausted)
        return;
    int nx = active_idx_ + 1;
    if (nx < model_->clipCount()) {
        switchTo(nx);
        if (player_)
            player_->Pause(false);  // 跨片段保持播放
    } else {
        paused_ = true;        // 播放到结尾
        if (player_)
            player_->Pause(true);
        timeline_pos_ms_ = model_->total_ms();
    }
}