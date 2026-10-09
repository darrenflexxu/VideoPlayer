#include "TimelinePlayer.h"
#include "x_qt_opengl_widget.h"
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

#include <QPainter>
#include <QFont>

void TimelinePlayer::setView(XQtOpenGLWidget* w)
{
    view_ = w;
    if (player_)
        player_->set_view(w);
}

void TimelinePlayer::pauseAll()
{
    if (player_)
        player_->Pause(true);
    for (auto& o : overlays_)
        if (o.player)
            o.player->Pause(true);
}

void TimelinePlayer::releaseBase()
{
    if (!player_)
        return;
    player_->Stop();
    delete player_;
    player_ = nullptr;
}

XPlayer* TimelinePlayer::buildBasePlayer(const Clip& c)
{
    auto* p = new XPlayer();
    p->set_view(view_);
    p->set_gpu_decode(false);
    p->set_gpu_direct_render_(false);
    p->set_owns_audio(true);   //主轨负责SDL音频设备
    p->set_audio_source(0);    //混音源0

    if (!p->Open(c.url.toLocal8Bit().constData(), nullptr)) {
        delete p;
        return nullptr;
    }
    return p;
}

bool TimelinePlayer::switchToBase(int idx)
{
    if (!model_ || idx < 0 || idx >= model_->clipCount())
        return false;
    const Clip& c = model_->clip(idx);

    XPlayer* p = buildBasePlayer(c);
    if (!p) {
        paused_ = true;
        timeline_pos_ms_ = c.pos_ms;
        return false;
    }
    // 新实例打开成功后再释放旧的, 打开失败时保留旧播放器状态
    releaseBase();
    player_ = p;
    active_idx_ = idx;
    if (c.source_start_ms > 0)
        player_->Seek(c.source_start_ms);
    player_->Start();
    player_->Pause(paused_);
    startPrefetch(idx + 1);
    return true;
}

//剪切后基片重同步: 若当前播放位置落入被裁掉的源区间, 重定位到可见窗口起点
void TimelinePlayer::resyncBase()
{
    if (!model_ || active_idx_ < 0 || active_idx_ >= model_->clipCount())
        return;
    if (!player_)
        return;
    const Clip& c = model_->clip(active_idx_);
    long long cur = player_->pos_ms();
    if (cur < c.source_start_ms || cur >= c.source_end_ms) {
        player_->Seek(c.source_start_ms);
        timeline_pos_ms_ = c.pos_ms;
    }
}

//统一创建叠加层播放器: 无view(合成器取帧)、独立混音源、不再打开SDL设备
XPlayer* TimelinePlayer::buildOverlayPlayer(const Clip& c)
{
    auto* p = new XPlayer();
    p->set_no_view(true);
    p->set_gpu_decode(false);
    p->set_owns_audio(false);       //SDL设备由主轨打开, 此处仅注入混音源
    p->set_audio_source(c.trackIndex);
    p->set_gpu_direct_render_(false);

    if (!p->Open(c.url.toLocal8Bit().constData(), nullptr)) {
        delete p;
        return nullptr;
    }
    return p;
}

void TimelinePlayer::destroyOverlay(int i)
{
    auto& o = overlays_[i];
    if (o.player) {
        o.player->Stop();
        delete o.player;
        o.player = nullptr;
    }
    overlays_.erase(overlays_.begin() + i);
}

void TimelinePlayer::destroyOverlays()
{
    for (int i = (int)overlays_.size() - 1; i >= 0; --i)
        destroyOverlay(i);
}

void TimelinePlayer::startPrefetch(int idx)
{
    if (!model_)
        return;
    //跳到下一个轨道0的视频片段(仅顺序播放主轨, 叠加层走reconcile)
    idx = nextBaseClip(idx);
    if (idx < 0 || idx >= model_->clipCount())
        return;
    joinPrefetch();
    const Clip c = model_->clip(idx);
    int gen = ++prefetch_gen_;
    prefetch_th_ = std::thread([this, c, gen]() {
        XPlayer* p = buildBasePlayer(c);
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
    releaseBase();
    player_ = next_player_;
    next_player_ = nullptr;
    player_->set_preroll_one(false);   // 预热完成, 解除"出帧即停"
    return true;
}

int TimelinePlayer::nextBaseClip(int idx) const
{
    if (!model_)
        return -1;
    for (int i = idx > 0 ? idx : 0; i < model_->clipCount(); ++i)
        if (model_->clip(i).type == ClipType::VIDEO ||
            model_->clip(i).type == ClipType::AUDIO)
            return i;
    return -1;
}

//timeline_ms 处最低轨激活的"可作基片"片段(VIDEO或AUDIO): 按Z序最底层, 无则-1
int TimelinePlayer::activeBaseClipIndex(long long ms) const
{
    if (!model_)
        return -1;
    auto act = model_->getActiveClips(ms);
    for (const Clip* p : act) {
        if (p->type != ClipType::VIDEO && p->type != ClipType::AUDIO)
            continue;
        for (int i = 0; i < model_->clipCount(); ++i)
            if (&model_->clip(i) == p)
                return i;
    }
    return -1;
}

//QImage → RGBA AVFrame: 贴纸/字幕静态层共用
AVFrame* TimelinePlayer::loadRgbaFromQImage(const QImage& img)
{
    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull() || rgba.width() <= 0 || rgba.height() <= 0)
        return nullptr;
    AVFrame* f = av_frame_alloc();
    if (!f)
        return nullptr;
    f->format = AV_PIX_FMT_RGBA;
    f->width = rgba.width();
    f->height = rgba.height();
    if (av_frame_get_buffer(f, 32) < 0) {
        av_frame_free(&f);
        return nullptr;
    }
    for (int y = 0; y < rgba.height(); ++y)
        memcpy(f->data[0] + (size_t)y * f->linesize[0],
               rgba.constScanLine(y), (size_t)rgba.width() * 4);
    return f;
}

//按片段类型生成静态层RGBA帧: 贴纸读图片, 字幕渲文字
AVFrame* TimelinePlayer::staticFrameFor(const Clip& c) const
{
    if (c.type == ClipType::STICKER) {
        QImage img(c.url);
        if (img.isNull())
            return nullptr;
        return loadRgbaFromQImage(img);
    }
    if (c.type == ClipType::SUBTITLE) {
        QImage img(800, 120, QImage::Format_RGBA8888);
        img.fill(Qt::transparent);
        QPainter p(&img);
        QFont font(QStringLiteral("PingFang SC"), 36, QFont::Bold);
        p.setFont(font);
        //半透明描边提升可读性
        QColor stroke(0, 0, 0, 200);
        p.setPen(stroke);
        QRect r = img.rect();
        QRect e = r.adjusted(2, 2, -2, -2);
        p.drawText(e, Qt::AlignCenter | Qt::TextWordWrap, c.text);
        p.setPen(QColor(255, 255, 255, 255));
        p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, c.text);
        p.end();
        return loadRgbaFromQImage(img);
    }
    return nullptr;
}

void TimelinePlayer::reconcileStatics()
{
    if (!model_)
        return;
    auto act = model_->getActiveClips(timeline_pos_ms_);
    std::vector<const Clip*> want;
    for (const Clip* p : act)
        if (p->type == ClipType::STICKER || p->type == ClipType::SUBTITLE)
            want.push_back(p);

    //移除已结束/变更的静态层
    for (int i = (int)static_layers_.size() - 1; i >= 0; --i) {
        bool keep = false;
        for (const Clip* w : want)
            if (static_layers_[i].clip_index >= 0 &&
                static_layers_[i].clip_index < (int)model_->clips().size() &&
                &model_->clip(static_layers_[i].clip_index) == w) {
                keep = true;
                break;
            }
        if (!keep) {
            if (static_layers_[i].rgba_frame)
                av_frame_free(&static_layers_[i].rgba_frame);
            static_layers_.erase(static_layers_.begin() + i);
        }
    }

    //生成缺失的静态层(贴纸载图/字幕渲文)
    for (const Clip* w : want) {
        long long idx = -1;
        for (int i = 0; i < (int)model_->clips().size(); ++i)
            if (&model_->clip(i) == w) { idx = i; break; }
        if (idx < 0)
            continue;
        bool exists = false;
        for (auto& s : static_layers_)
            if (s.clip_index == idx) { exists = true; break; }
        if (exists)
            continue;
        XStaticOverlay st;
        st.clip_index = (int)idx;
        st.track = w->trackIndex;
        st.rgba_frame = staticFrameFor(*w);
        if (!st.rgba_frame)
            continue;
        static_layers_.push_back(st);
    }
}

void TimelinePlayer::destroyStatics()
{
    for (auto& s : static_layers_)
        if (s.rgba_frame)
            av_frame_free(&s.rgba_frame);
    static_layers_.clear();
}

TimelinePlayer::~TimelinePlayer()
{
    joinPrefetch();
    destroyOverlays();
    destroyStatics();
    releaseBase();
}

//按播放头收敛叠加层: 打开新出现的叠加片段, 关闭已结束的; 均定位到各自本地位置
void TimelinePlayer::reconcileOverlays()
{
    if (!model_)
        return;
    auto act = model_->getActiveClips(timeline_pos_ms_);
    //基片 = 当前激活的最低轨视频片段; 其余视频/音频均为叠加层(不含基片自身)
    const Clip* base = nullptr;
    if (active_idx_ >= 0 && active_idx_ < (int)model_->clips().size())
        base = &model_->clip(active_idx_);
    std::vector<const Clip*> want;   //叠加层集合(按Z序, 底层在前)
    for (const Clip* p : act) {
        if (p->type != ClipType::VIDEO && p->type != ClipType::AUDIO)
            continue;
        if (base && p == base)
            continue;
        want.push_back(p);
    }

    //移除不再需要/片段已变更的叠加层
    for (int i = (int)overlays_.size() - 1; i >= 0; --i) {
        bool keep = false;
        for (const Clip* w : want)
            if (overlays_[i].clip_index >= 0 &&
                overlays_[i].clip_index < (int)model_->clips().size() &&
                &model_->clip(overlays_[i].clip_index) == w) {
                keep = true;
                break;
            }
        if (!keep) {
            if (view_)
                view_->clearOverlays();
            destroyOverlay(i);
        }
    }

    //打开需要的叠加层(按clip_index在模型中的顺序, 保证底层在前)
    for (const Clip* w : want) {
        long long idx = -1;
        for (int i = 0; i < (int)model_->clips().size(); ++i)
            if (&model_->clip(i) == w) { idx = i; break; }
        if (idx < 0)
            continue;
        int ti = w->trackIndex;
        int found = -1;
        for (int i = 0; i < (int)overlays_.size(); ++i)
            if (overlays_[i].clip_index == idx) { found = i; break; }
        if (found >= 0) {
            // 已有: 漂移过大时重新对位(snapping)
            long long local = timeline_pos_ms_ - w->pos_ms;
            long long cur = overlays_[found].player->pos_ms();
            if (local < 0) local = 0;
            if (llabs(cur - (w->source_start_ms + local)) > 200)
                overlays_[found].player->Seek(w->source_start_ms + local);
            continue;
        }
        XOverlayPlay o;
        o.clip_index = (int)idx;
        o.track = ti;
        o.player = buildOverlayPlayer(*w);
        if (!o.player)
            continue;
        long long local = timeline_pos_ms_ - w->pos_ms;
        if (local < 0) local = 0;
        o.player->Start();
        o.player->Seek(w->source_start_ms + local);
        o.player->Pause(paused_);
        overlays_.push_back(o);
    }
}

bool TimelinePlayer::play()
{
    if (!model_ || model_->empty())
        return false;
    if (active_idx_ < 0) {
        // 尚未打开基片: 从播放头处最低轨的视频/音频开始, 找不到则取第一个可作基片的片段
        int idx = activeBaseClipIndex(timeline_pos_ms_);
        if (idx < 0) {
            for (int i = 0; i < model_->clipCount(); ++i)
                if (model_->clip(i).type == ClipType::VIDEO ||
                    model_->clip(i).type == ClipType::AUDIO) { idx = i; break; }
        }
        if (idx < 0)
            idx = 0;
        paused_ = false;
        if (!switchToBase(idx))
            return false;
        timeline_pos_ms_ = model_->clip(idx).pos_ms;
        return true;
    }
    paused_ = false;
    resyncBase();               //剪切后起点前移时, 重定位避免被裁掉的前段仍被播放
    if (player_)
        player_->Pause(false);
    for (auto& o : overlays_)
        if (o.player)
            o.player->Pause(false);
    return true;
}

bool TimelinePlayer::seekTo(long long timeline_ms)
{
    if (!model_ || model_->empty())
        return false;
    int idx = activeBaseClipIndex(timeline_ms);
    if (idx < 0)
        return false;
    timeline_pos_ms_ = timeline_ms;
    paused_ = false;
    //基片对齐
    if (idx != active_idx_) {
        if (!switchToBase(idx))
            return false;
        if (player_)
            player_->Pause(false);
        else
            return false;
    } else if (player_) {
        player_->Pause(false);
    }
    if (player_) {
        const Clip& c = model_->clip(idx);
        player_->Seek(c.source_start_ms + (timeline_ms - c.pos_ms));
    }
    if (view_)
        view_->clearOverlays();
    destroyOverlays();
    destroyStatics();
    reconcileOverlays();
    reconcileStatics();
    return true;
}

void TimelinePlayer::set_volume(float v)
{
    if (player_)
        player_->SetVolume(v);
    for (auto& o : overlays_)
        if (o.player)
            o.player->SetVolume(v);
}

void TimelinePlayer::set_speed(float s)
{
    if (player_)
        player_->SetSpeed(s);
}

void TimelinePlayer::Update()
{
    if (!model_ || model_->empty() || active_idx_ < 0 || !player_)
        return;

    //按播放头收敛叠加层集合(暂停时也能显示当前播放头的叠加画面)
    reconcileOverlays();
    reconcileStatics();

    player_->Update();          // 主轨渲染一帧 / 推进音频
    if (view_)
        view_->clearOverlays();

    //按(轨, 层序)Z序合并绘制: 视频叠加层(播放器) + 静态层(贴纸/字幕) 依次填入合成器
    struct Layer { int track; int kind; int index; };
    std::vector<Layer> order;
    for (int i = 0; i < (int)overlays_.size(); ++i)
        order.push_back({overlays_[i].track, 0, i});
    for (int i = 0; i < (int)static_layers_.size(); ++i)
        order.push_back({static_layers_[i].track, 1, i});
    std::stable_sort(order.begin(), order.end(),
                     [](const Layer& a, const Layer& b) { return a.track < b.track; });

    int slot = 0;
    for (const Layer& L : order) {
        AVFrame* f = nullptr;
        float alpha = 1.0f;
        XOverlayRect r;
        r.x = 0; r.y = 0; r.w = 1; r.h = 1;
        if (L.kind == 0) {
            XOverlayPlay& o = overlays_[L.index];
            if (!o.player)
                continue;
            o.player->Update();      // 推进该层解码/音频
            if (!view_)
                continue;
            if (!o.player->GetVideoFrameCopy(&f) || !f)
                continue;
            const Clip& c = model_->clip(o.clip_index);
            alpha = (float)c.alpha;
            r.x = c.rect.x; r.y = c.rect.y;
            r.w = c.rect.w; r.h = c.rect.h;
        } else {
            XStaticOverlay& s = static_layers_[L.index];
            if (!s.rgba_frame)
                continue;
            const Clip& c = model_->clip(s.clip_index);
            f = av_frame_alloc();
            if (!f)
                continue;
            if (av_frame_ref(f, s.rgba_frame) < 0) {
                av_frame_free(&f);
                continue;
            }
            alpha = (float)c.alpha;
            r.x = c.rect.x; r.y = c.rect.y;
            r.w = c.rect.w; r.h = c.rect.h;
        }
        if (!view_ || !f)
            continue;
        view_->setOverlayFrame(slot, f, r, alpha);
        av_frame_free(&f);
        ++slot;
    }

    if (paused_)
        return;

    const Clip& c = model_->clip(active_idx_);
    long long local = player_->pos_ms();
    //时间轴位置 = 片段起点 + (当前源位置 - 裁剪起点), 前段被裁时播放头才准
    timeline_pos_ms_ = c.pos_ms + std::max(0LL, local - c.source_start_ms);

    // 播放头到达时间轴末尾 -> 停止(避免末段被当作"切到下一段"而重头循环)
    if (timeline_pos_ms_ >= model_->total_ms() - 60) {
        paused_ = true;
        if (player_)
            player_->Pause(true);
        for (auto& o : overlays_)
            if (o.player)
                o.player->Pause(true);
        timeline_pos_ms_ = model_->total_ms();
        return;
    }

    // 主片段可见窗口用尽(以裁剪终点为界, 带容差)或源已放完 -> 切到下一段
    bool exhausted = local >= c.source_end_ms - 60 || player_->IsFinish();
    if (!exhausted)
        return;
    int nx = nextBaseClip(active_idx_);
    if (nx >= 0) {
        if (takePrefetched()) {
            // 后台已预热好下一段: 直接换装, 边界几乎无卡顿
            const Clip& nxt = model_->clip(nx);
            if (nxt.source_start_ms > 0)
                player_->Seek(nxt.source_start_ms);
            player_->Pause(false);
            active_idx_ = nx;
            timeline_pos_ms_ = nxt.pos_ms;
            startPrefetch(nx + 1);
        } else {
            switchToBase(nx);   // 预热未就绪时退回同步打开(内部会重新预热下一段)
            if (nx < model_->clipCount())
                timeline_pos_ms_ = model_->clip(nx).pos_ms;
        }
    } else {
        paused_ = true;        // 播放到结尾
        if (player_)
            player_->Pause(true);
        for (auto& o : overlays_)
            if (o.player)
                o.player->Pause(true);
        timeline_pos_ms_ = model_->total_ms();
    }
}