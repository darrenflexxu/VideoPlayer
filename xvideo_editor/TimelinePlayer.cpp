#include "TimelinePlayer.h"
#include "x_qt_opengl_widget.h"
#include <chrono>
#include <thread>

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

XPlayer* TimelinePlayer::buildPlayer(const Clip& c)
{
    auto* p = new XPlayer();
    p->set_view(view_);
    p->set_gpu_decode(false);
    p->set_gpu_direct_render_(false);

    if (!p->Open(c.url.toLocal8Bit().constData(), nullptr)) {
        delete p;
        return nullptr;
    }
    return p;
}

bool TimelinePlayer::switchTo(int idx)
{
    if (!model_ || idx < 0 || idx >= model_->clipCount())
        return false;
    const Clip& c = model_->clip(idx);

    XPlayer* p = buildPlayer(c);
    if (!p) {
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
    startPrefetch(idx + 1);
    return true;
}

void TimelinePlayer::startPrefetch(int idx)
{
    if (!model_ || idx < 0 || idx >= model_->clipCount())
        return;
    joinPrefetch();   // 回收上一次预热线程, 并作废其可能已发布的播放器
    const Clip c = model_->clip(idx);
    int gen = ++prefetch_gen_;
    prefetch_th_ = std::thread([this, c, gen]() {
        XPlayer* p = buildPlayer(c);
        if (!p)
            return;
        p->set_preroll_one(true);
        p->Start();
        // 轮询等待第一帧解码完成(后台预热); 超时或作废则主动停止
        for (int i = 0; i < 400 && !p->preroll_ready(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        // 预热完成即整体暂停(decode+demux): 防止demux继续把整段灌进缓存,
        // 换装恢复时才能按真实帧率被demux限速逐步解码
        p->Pause(true);
        {
            std::lock_guard<std::mutex> lk(next_mtx_);
            if (gen != prefetch_gen_ || !p->preroll_ready()) {
                // 已作废或预热失败: 在本线程释放(不触碰外部view)
                p->set_view(nullptr);
                p->Stop();
                delete p;
                return;
            }
            if (next_player_)
                delete next_player_;
            next_player_ = p;
        }
    });
}

void TimelinePlayer::joinPrefetch()
{
    prefetch_gen_++;   // 作废在途预热, worker 发现代际不符会自行清理
    {
        std::lock_guard<std::mutex> lk(next_mtx_);
        if (next_player_) {
            delete next_player_;
            next_player_ = nullptr;
        }
    }
    if (prefetch_th_.joinable())
        prefetch_th_.join();
}

bool TimelinePlayer::takePrefetched()
{
    std::lock_guard<std::mutex> lk(next_mtx_);
    if (!next_player_)
        return false;
    releasePlayer();
    player_ = next_player_;
    next_player_ = nullptr;
    player_->set_preroll_one(false);   // 预热完成, 解除"出帧即停"
    return true;
}

TimelinePlayer::~TimelinePlayer()
{
    joinPrefetch();
    releasePlayer();
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
        if (takePrefetched()) {
            // 后台已预热好下一段: 直接换装, 片段边界几乎无卡顿
            active_idx_ = nx;
            const Clip& nxt = model_->clip(nx);
            if (nxt.source_start_ms > 0)
                player_->Seek(nxt.source_start_ms);
            player_->Pause(false);   // 恢复解码(预热后停在第一帧)
            timeline_pos_ms_ = nxt.pos_ms;
        } else {
            switchTo(nx);            // 预热未就绪时退回同步打开
            if (player_)
                player_->Pause(false);  // 跨片段保持播放
        }
        startPrefetch(nx + 1);
    } else {
        paused_ = true;        // 播放到结尾
        if (player_)
            player_->Pause(true);
        timeline_pos_ms_ = model_->total_ms();
    }
}