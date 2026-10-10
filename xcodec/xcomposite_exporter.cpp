#include "predefine_header.h"
#include "xcomposite_exporter.h"
#include "xtools.h"
#include <algorithm>
#include <cstring>

//分包器: 转发包给音/视频解码器, 或用空包告知各自EOF
void XCompositeExporter::ClipDecoder::Router::Main() {
    while (!is_exit_) MSleep(10);
}
void XCompositeExporter::ClipDecoder::Router::Do(AVPacket* pkt) {
    if (!pkt) return;
    if (pkt->buf == nullptr) {
        //demux EOF 标记: 按各自流索引合成空包触发解码器冲刷
        AVPacket e = {};
        if (v) { e.stream_index = v_idx; v->Do(&e); }
        if (a) { e.stream_index = a_idx; a->Do(&e); }
        return;
    }
    if (v) v->Do(pkt);
    if (a) a->Do(pkt);
}

XCompositeExporter::XCompositeExporter() = default;

XCompositeExporter::~XCompositeExporter() {
    Stop();
    Wait();
}

void XCompositeExporter::Stop() {
    stop_requested_.store(true);
}

void XCompositeExporter::Wait() {
    if (export_th_.joinable())
        export_th_.join();
}

bool XCompositeExporter::Start(const std::string& out_url)
{
    if (running_.load())
        return false;
    if (model_.clips.empty()) {
        error_ = "empty model";
        return false;
    }
    out_url_ = out_url;
    error_.clear();
    finished_.store(false);
    progress_.store(0.0f);
    stop_requested_.store(false);

    //初始化 FBO(离屏渲染目标); 纯音频模式跳过
    if (!audio_only_ && !fbo_.Init(vopts_.width, vopts_.height)) {
        error_ = "FBO init failed";
        return false;
    }

    if (!InitEncoders()) {
        return false;
    }

    running_.store(true);
    export_th_ = std::thread(&XCompositeExporter::ExportThread, this, out_url);
    return true;
}

bool XCompositeExporter::InitEncoders()
{
    AVCodecParameters* vpara = nullptr;
    AVRational vtb = {1, vopts_.fps};
    if (!audio_only_) {
        //视频编码器参数
        vpara = avcodec_parameters_alloc();
        const AVCodec* venc = avcodec_find_encoder_by_name(vopts_.codec.c_str());
        if (!venc) {
            error_ = "video encoder not found: " + vopts_.codec;
            avcodec_parameters_free(&vpara);
            return false;
        }
        vpara->codec_type = AVMEDIA_TYPE_VIDEO;
        vpara->codec_id = venc->id;
        vpara->width = vopts_.width;
        vpara->height = vopts_.height;
        vpara->format = AV_PIX_FMT_YUV420P;
        vpara->framerate = {vopts_.fps, 1};
    }

    //音频编码器参数
    AVCodecParameters* apara = avcodec_parameters_alloc();
    const AVCodec* aenc = avcodec_find_encoder_by_name(aopts_.codec.c_str());
    if (!aenc) {
        error_ = "audio encoder not found: " + aopts_.codec;
        avcodec_parameters_free(&vpara);
        avcodec_parameters_free(&apara);
        return false;
    }
    apara->codec_type = AVMEDIA_TYPE_AUDIO;
    apara->codec_id = aenc->id;
    apara->sample_rate = aopts_.sample_rate;
    av_channel_layout_default(&apara->ch_layout, aopts_.channels);
    apara->format = AV_SAMPLE_FMT_FLTP;
    apara->bit_rate = aopts_.bitrate;

    AVRational atb = {1, aopts_.sample_rate};

    if (!audio_only_) {
        video_enc_ = std::make_unique<XEncodeTask>();
        video_enc_->set_gpu_encode(false);
        video_enc_->set_time_base(&vtb);
        if (!video_enc_->Open(vpara, vopts_.opts)) {
            error_ = "video encoder open failed";
            avcodec_parameters_free(&vpara);
            avcodec_parameters_free(&apara);
            return false;
        }
        avcodec_parameters_free(&vpara);
        vpara = nullptr;
    }

    audio_enc_ = std::make_unique<XEncodeTask>();
    audio_enc_->set_gpu_encode(false);
    audio_enc_->set_time_base(&atb);
    if (!audio_enc_->Open(apara, aopts_.opts)) {
        error_ = "audio encoder open failed";
        if (vpara) avcodec_parameters_free(&vpara);
        avcodec_parameters_free(&apara);
        return false;
    }
    avcodec_parameters_free(&apara);

    //从编码器上下文复制编码器参数给复用器; 纯音频模式无视频流
    AVCodecParameters* vout = nullptr;
    if (!audio_only_) {
        vout = avcodec_parameters_alloc();
        avcodec_parameters_from_context(vout, video_enc_->GetCodecContext());
    }
    AVCodecParameters* aout = avcodec_parameters_alloc();
    avcodec_parameters_from_context(aout, audio_enc_->GetCodecContext());

    //复用器; 纯音频模式只建音频流(传nullptr视频参数)
    mux_ = std::make_unique<XMuxTask>();
    if (!mux_->Open(out_url_.c_str(), vout, audio_only_ ? nullptr : &vtb, aout, &atb)) {
        error_ = "muxer open failed: " + mux_->error();
        if (vout) avcodec_parameters_free(&vout);
        avcodec_parameters_free(&aout);
        return false;
    }
    if (audio_only_)
        mux_->set_audio_only(true);
    if (vout) avcodec_parameters_free(&vout);
    avcodec_parameters_free(&aout);

    //责任链: 编码器发包 -> 复用器; 纯音频时音频独占流0
    if (!audio_only_) {
        video_enc_->set_stream_index(0);
        audio_enc_->set_stream_index(1);
        video_enc_->set_next(mux_.get());
    } else {
        audio_enc_->set_stream_index(0);
    }
    audio_enc_->set_next(mux_.get());
    mux_->ignoreMaxPkts(true);

    mux_->Start();
    if (!audio_only_)
        video_enc_->Start();
    audio_enc_->Start();
    return true;
}

long long XCompositeExporter::ClipIndex(const XExportClip* clip) const
{
    if (model_.clips.empty()) return -1;
    ptrdiff_t idx = clip - model_.clips.data();
    if (idx < 0 || idx >= (ptrdiff_t)model_.clips.size()) return -1;
    return idx;
}

XCompositeExporter::ClipDecoder* XCompositeExporter::GetDecoder(const XExportClip* clip)
{
    long long idx = ClipIndex(clip);
    if (idx < 0) return nullptr;
    std::lock_guard<std::mutex> lk(decoders_mux_);
    if (idx >= (long long)decoders_.size())
        decoders_.resize(idx + 1);
    if (decoders_[idx])
        return decoders_[idx].get();

    auto nd = std::make_unique<ClipDecoder>();
    nd->clip = clip;
    //截取区间在 Open 前设置, 起点由 Open seek, 终点驱动EOF注入
    nd->demux.set_trim(clip->source_start_ms, clip->source_end_ms);
    if (!nd->demux.Open(clip->url)) {
        fprintf(stderr, "[EXPORT] open demux failed: %s\n", clip->url.c_str());
        return nullptr;
    }
    nd->vp = nd->demux.CopyVideoPara();
    nd->ap = nd->demux.CopyAudioPara();
    decoders_[idx] = std::move(nd);
    ClipDecoder* d = decoders_[idx].get();
    //预先打开音/视频解码器并挂到路由器, 再启动解封装。
    //否则懒打开会丢弃路由器中途的音频包, 造成混音起始偏移;
    //纯音频模式跳过视频解码器
    if (!audio_only_ && d->vp) OpenVideoDecoder(d);
    if (d->ap) OpenAudioDecoder(d);
    if (!d->v_started && !d->a_started) {
        decoders_[idx].reset();
        return nullptr;
    }
    if (!d->demux_started_) {
        d->demux.set_next(&d->router);
        d->demux.Start();
        d->demux_started_ = true;
    }
    return d;
}

//背压: 两轨解码都领先时暂停解封装, 都消费到低位后恢复
void XCompositeExporter::UpdateBackpressure(ClipDecoder* d)
{
    if (!d) return;
    int vc = d->v_started ? d->video_dec.cached_frame_count() : 0;
    int ac = d->a_started ? d->audio_dec.cached_frame_count() : 0;
    bool has_video = d->v_started && d->vp;
    bool has_audio = d->a_started && d->ap;
    bool vm = has_video && vc > 80;
    bool am = has_audio && ac > 80;
    bool pause_now = (vm && !has_audio) || (am && !has_video) || (vm && am);
    bool resume_now = (!has_video || vc < 15) && (!has_audio || ac < 15);
    if (pause_now && !d->demux.is_pause())
        d->demux.Pause(true);
    else if (!pause_now && d->demux.is_pause() && resume_now)
        d->demux.Pause(false);
}

void XCompositeExporter::OpenVideoDecoder(ClipDecoder* d)
{
    if (!d->vp) return;
    d->video_dec.set_gpu_decode(false);
    d->video_dec.set_frame_cache(true);
    d->video_dec.ignoreMaxPkts(true);
    d->video_dec.set_stream_index(d->demux.video_index());
    d->video_dec.set_block_size(4);
    if (!d->video_dec.Open(d->vp->para)) {
        d->vp.reset();
        return;
    }
    d->router.v = &d->video_dec;
    d->router.v_idx = d->demux.video_index();
    if (!d->demux_started_) {   //音频已启动则复用已运行解封装
        d->demux.set_next(&d->router);
        d->demux.Start();
        d->demux_started_ = true;
    }
    d->video_dec.Start();
    d->v_started = true;
}

void XCompositeExporter::OpenAudioDecoder(ClipDecoder* d)
{
    if (!d->ap) return;
    d->audio_dec.set_gpu_decode(false);
    d->audio_dec.set_frame_cache(true);
    d->audio_dec.ignoreMaxPkts(true);
    d->audio_dec.set_stream_index(d->demux.audio_index());
    d->audio_dec.set_block_size(4);
    if (!d->audio_dec.Open(d->ap->para)) {
        d->ap.reset();
        return;
    }
    d->router.a = &d->audio_dec;
    d->router.a_idx = d->demux.audio_index();
    if (!d->demux_started_) {
        d->demux.set_next(&d->router);
        d->demux.Start();
        d->demux_started_ = true;
    }
    d->audio_dec.Start();
    d->a_started = true;
}

//弹出视频下一帧(等待), false 表示源已读尽
bool XCompositeExporter::PopVideoFrame(ClipDecoder* d, AVFrame** out, long long* out_ms)
{
    int nulls = 0, polls = 0;
    for (;;) {
        if (stop_requested_.load()) { d->v_eof = true; return false; }
        AVFrame* f = d->video_dec.GetFrame();
        if (f) {
            *out = f;
            *out_ms = av_rescale_q(f->pts, *d->vp->time_base, {1, 1000});
            UpdateBackpressure(d);
            return true;
        }
        if (d->demux.is_eof()) {
            if (++nulls > 50) { d->v_eof = true; return false; }   //~100ms 余量
        } else {
            nulls = 0;
        }
        if (++polls > 6000) { d->v_eof = true; return false; }     //兜底防卡死
        MSleep(2);
    }
}

//取 cur/next 前瞻中最近目标时刻的帧(cur 满足即返回, 不复制)
AVFrame* XCompositeExporter::ReadClipFrame(const XExportClip* clip, long long target_ms)
{
    ClipDecoder* d = GetDecoder(clip);
    if (!d || !d->vp) return nullptr;
    if (!d->v_started) OpenVideoDecoder(d);
    if (!d->v_started) return nullptr;

    for (;;) {
        if (stop_requested_.load()) return nullptr;
        //cur 已覆盖目标
        if (d->v_cur && d->v_cur_ms <= target_ms &&
            (!d->v_next || d->v_next_ms > target_ms))
            break;
        //next 不超过目标: 提升为 cur 继续向后推
        if (d->v_next && d->v_next_ms <= target_ms) {
            av_frame_free(&d->v_cur);
            d->v_cur = d->v_next;
            d->v_cur_ms = d->v_next_ms;
            d->v_next = nullptr;
            d->v_next_ms = -1;
            continue;
        }
        //需要下一帧
        if (!d->v_eof && !d->v_next) {
            AVFrame* f = nullptr;
            long long ms = -1;
            if (PopVideoFrame(d, &f, &ms)) {
                d->v_next = f;
                d->v_next_ms = ms;
                continue;
            }
            break;  //源读尽
        }
        break;      //next 已越过目标
    }
    if (!d->v_cur) {   //目标之前无帧, 用下一帧顶替
        d->v_cur = d->v_next;
        d->v_cur_ms = d->v_next_ms;
        d->v_next = nullptr;
        d->v_next_ms = -1;
    }
    return d->v_cur;
}

//弹出音频下一帧并重采样到混音规格, 追加到 abuf; false 表示读到结尾
bool XCompositeExporter::AppendAudioFrame(ClipDecoder* d)
{
    AVFrame* f = nullptr;
    int nulls = 0, polls = 0;
    for (;;) {
        if (stop_requested_.load()) { d->a_eof = true; return false; }
        f = d->audio_dec.GetFrame();
        if (f) break;
        if (d->demux.is_eof()) {
            if (++nulls > 50) { d->a_eof = true; return false; }
        } else {
            nulls = 0;
        }
        if (++polls > 6000) { d->a_eof = true; return false; }
        MSleep(2);
    }

    if (!d->a_rr) d->a_rr.reset(new XResample());
    if (d->a_rr->Need(f, mix_ctx_)) d->a_rr->Create(f, mix_ctx_);
    if (!d->a_rr->is_active()) {
        av_frame_free(&f);
        d->a_eof = true;
        return false;
    }
    AVFrame* out = d->a_rr->Convert(f);
    av_frame_free(&f);
    if (!out || out->nb_samples <= 0) {
        if (out) av_frame_free(&out);
        return true;
    }

    //帧的源绝对采样位置(以目标采样率计)
    long long fab = d->abuf_abs + (long long)d->abuf_l.size();
    if (out->pts != AV_NOPTS_VALUE)
        fab = av_rescale_q(out->pts, *d->ap->time_base, {1, 1000}) *
              mix_ctx_->sample_rate / 1000;

    int n = out->nb_samples;
    int off = 0;
    if (d->abuf_l.empty()) {
        d->abuf_abs = fab;
    } else {
        long long expected = d->abuf_abs + (long long)d->abuf_l.size();
        if (fab > expected) {
            //空洞补零(理论上连续解码不会出现)
            long long gap = fab - expected;
            d->abuf_l.resize(d->abuf_l.size() + gap, 0.0f);
            d->abuf_r.resize(d->abuf_r.size() + gap, 0.0f);
        } else if (fab < expected) {
            //重叠/重复帧: 跳过重复部分
            long long lead = expected - fab;
            off = lead < n ? (int)lead : n;
        }
    }
    const float* srcL = (const float*)out->data[0];
    const float* srcR = (const float*)out->data[1];
    for (int k = off; k < n; ++k) {
        d->abuf_l.push_back(srcL[k]);
        d->abuf_r.push_back(srcR[k]);
    }
    av_frame_free(&out);
    UpdateBackpressure(d);
    return true;
}

//从片段音频缓冲取出 [need_lo, need_hi) 源采样并累加到 dstL/dstR
void XCompositeExporter::PullAudio(const XExportClip* clip, long long need_lo,
                                   long long need_hi,
                                   float* dstL, float* dstR)
{
    ClipDecoder* d = GetDecoder(clip);
    if (!d || !d->ap) return;
    if (!d->a_started) OpenAudioDecoder(d);
    if (!d->a_started) return;

    //确保缓冲覆盖到 need_hi
    int wait = 0;
    while ((long long)d->abuf_l.size() < need_hi - d->abuf_abs) {
        if (d->a_eof || stop_requested_.load()) break;
        if (!AppendAudioFrame(d)) break;
        if (++wait > 4000) break;
    }

    long long have = (long long)d->abuf_l.size();
    if (have <= 0) return;

    //丢弃 need_lo 之前
    if (d->abuf_abs < need_lo) {
        long long skip = need_lo - d->abuf_abs;
        if (skip >= have) {
            d->abuf_l.clear();
            d->abuf_r.clear();
            d->abuf_abs = need_lo;
            return;
        }
        d->abuf_l.erase(d->abuf_l.begin(), d->abuf_l.begin() + skip);
        d->abuf_r.erase(d->abuf_r.begin(), d->abuf_r.begin() + skip);
        d->abuf_abs = need_lo;
    }

    long long take = std::min(need_hi, d->abuf_abs + (long long)d->abuf_l.size()) - need_lo;
    if (take <= 0) return;
    for (long long k = 0; k < take; ++k) {
        dstL[k] += d->abuf_l[(size_t)k];
        dstR[k] += d->abuf_r[(size_t)k];
    }
    d->abuf_l.erase(d->abuf_l.begin(), d->abuf_l.begin() + take);
    d->abuf_r.erase(d->abuf_r.begin(), d->abuf_r.begin() + take);
    d->abuf_abs += take;
}

void XCompositeExporter::CompositeAndEncodeVideo(int frame_idx, long long timeline_ms)
{
    fbo_.BeginFrame();

    //按 Z 序绘制所有激活片段
    auto act = model_.getActiveClips(timeline_ms);
    for (const XExportClip* c : act) {
        if (c->type == XClipType::VIDEO || c->type == XClipType::AUDIO) {
            long long local_ms = timeline_ms - c->pos_ms;
            if (local_ms < 0) local_ms = 0;
            long long src_ms = c->source_start_ms + local_ms;
            AVFrame* f = ReadClipFrame(c, src_ms);
            if (!f) continue;
            fbo_.SetViewport(c->rect.x, c->rect.y, c->rect.w, c->rect.h);
            switch (f->format) {
            case AV_PIX_FMT_YUV420P:
            case AV_PIX_FMT_YUVJ420P:
                fbo_.DrawYV12(f->data[0], f->linesize[0],
                              f->data[1], f->linesize[1],
                              f->data[2], f->linesize[2],
                              XTools::get_rotation_from_frame(f));
                break;
            case AV_PIX_FMT_NV12:
                fbo_.DrawNV12(f->data[0], f->linesize[0],
                              f->data[1], f->linesize[1]);
                break;
            default:
                break;
            }
        }
        else if (c->type == XClipType::STICKER || c->type == XClipType::SUBTITLE) {
            if (!rgba_provider_) continue;
            AVFrame* f = rgba_provider_(c, timeline_ms);
            if (!f) continue;
            fbo_.SetViewport(c->rect.x, c->rect.y, c->rect.w, c->rect.h);
            fbo_.DrawRGBA(f->data[0], f->linesize[0], f->width, f->height,
                          (float)c->alpha);
            av_frame_free(&f);
        }
    }

    fbo_.EndFrame();

    //读回像素(GL 自下而上) -> 翻转 -> RGBA 帧送编码器(编码侧转 YUV420P)
    std::vector<unsigned char> rgba((size_t)fbo_.width() * fbo_.height() * 4);
    if (!fbo_.ReadPixelsRGBA(rgba.data()))
        return;
    AVFrame* f = av_frame_alloc();
    if (!f) return;
    f->format = AV_PIX_FMT_RGBA;
    f->width = fbo_.width();
    f->height = fbo_.height();
    if (av_frame_get_buffer(f, 32) < 0) {
        av_frame_free(&f);
        return;
    }
    const size_t rowbytes = (size_t)f->width * 4;
    const size_t pitch = (size_t)f->linesize[0];
    for (int y = 0; y < f->height; ++y)
        memcpy(f->data[0] + (size_t)y * pitch,
               rgba.data() + (size_t)(f->height - 1 - y) * rowbytes, rowbytes);
    f->pts = frame_idx;
    video_enc_->Do(f);
}

void XCompositeExporter::MixAndEncodeAudio(long long s0, long long s1)
{
    const long long n = s1 - s0;
    if (n <= 0) return;
    const int sr = aopts_.sample_rate;
    std::vector<float> L((size_t)n, 0.0f), R((size_t)n, 0.0f);

    for (const XExportClip& c : model_.clips) {
        if (c.type != XClipType::VIDEO && c.type != XClipType::AUDIO) continue;
        ClipDecoder* d = GetDecoder(&c);
        if (!d || !d->ap) continue;
        long long pos_s = c.pos_ms * sr / 1000;
        long long dur_s = c.duration_ms() * sr / 1000;
        long long lo = std::max(s0, pos_s);
        long long hi = std::min(s1, pos_s + dur_s);
        if (hi <= lo) continue;
        long long src_lo = c.source_start_ms * sr / 1000 + (lo - pos_s);
        if (stop_requested_.load()) return;
        PullAudio(&c, src_lo, src_lo + (hi - lo), &L[lo - s0], &R[lo - s0]);
    }

    //并入待发缓冲
    pend_l_.insert(pend_l_.end(), L.begin(), L.end());
    pend_r_.insert(pend_r_.end(), R.begin(), R.end());

    //按编码器帧大小切块发送(PCM等为0时按1024), 剩余留到下一轮或结尾补齐
    long long kChunk = audio_enc_ ? audio_enc_->audio_frame_size() : 0;
    if (kChunk <= 0) kChunk = 1024;
    while ((long long)pend_l_.size() >= kChunk) {
        AVFrame* f = av_frame_alloc();
        if (!f) return;
        f->format = AV_SAMPLE_FMT_FLTP;
        f->sample_rate = sr;
        av_channel_layout_default(&f->ch_layout, aopts_.channels);
        f->nb_samples = (int)kChunk;
        if (av_frame_get_buffer(f, 0) < 0) {
            av_frame_free(&f);
            return;
        }
        memcpy(f->data[0], pend_l_.data(), (size_t)kChunk * sizeof(float));
        memcpy(f->data[1], pend_r_.data(), (size_t)kChunk * sizeof(float));
        f->pts = pend_pts_;   //样本域, 编码器时间基 {1, sr}
        audio_enc_->Do(f);
        pend_l_.erase(pend_l_.begin(), pend_l_.begin() + kChunk);
        pend_r_.erase(pend_r_.begin(), pend_r_.begin() + kChunk);
        pend_pts_ += kChunk;
    }
}

//结尾不足一帧的部分补零到编码器帧大小再发送, 保证编码器能出包
void XCompositeExporter::FlushPendingAudio(int sr)
{
    long long kChunk = audio_enc_ ? audio_enc_->audio_frame_size() : 0;
    if (kChunk <= 0) kChunk = 1024;
    if (pend_l_.empty())
        return;
    while ((long long)pend_l_.size() < kChunk) {
        pend_l_.push_back(0.0f);
        pend_r_.push_back(0.0f);
    }
    AVFrame* f = av_frame_alloc();
    if (!f) return;
    f->format = AV_SAMPLE_FMT_FLTP;
    f->sample_rate = sr;
    av_channel_layout_default(&f->ch_layout, aopts_.channels);
    f->nb_samples = (int)kChunk;
    if (av_frame_get_buffer(f, 0) < 0) {
        av_frame_free(&f);
        return;
    }
    memcpy(f->data[0], pend_l_.data(), (size_t)kChunk * sizeof(float));
    memcpy(f->data[1], pend_r_.data(), (size_t)kChunk * sizeof(float));
    f->pts = pend_pts_;
    audio_enc_->Do(f);
}

void XCompositeExporter::StopClipDecoder(ClipDecoder* d)
{
    if (!d) return;
    d->demux.Exit();
    d->demux.Wait();
    if (d->v_started) {
        d->video_dec.Exit();
        d->video_dec.Wait();
    }
    if (d->a_started) {
        d->audio_dec.Exit();
        d->audio_dec.Wait();
    }
    av_frame_free(&d->v_cur);
    av_frame_free(&d->v_next);
}

void XCompositeExporter::ExportThread(const std::string& out_url)
{
    (void)out_url;
    long long total_ms = model_.total_ms();
    if (total_ms <= 0) {
        error_ = "total duration zero";
        running_.store(false);
        finished_.store(true);
        return;
    }

    const int sr = aopts_.sample_rate;
    const long long fps = vopts_.fps > 0 ? vopts_.fps : 25;
    const long long total_frames = (total_ms * fps + 999) / 1000;

    //混音目标参数(不 open, 只做重采样参考)
    mix_ctx_ = avcodec_alloc_context3(nullptr);
    if (mix_ctx_) {
        mix_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
        mix_ctx_->sample_rate = sr;
        av_channel_layout_default(&mix_ctx_->ch_layout, aopts_.channels);
    }

    if (audio_only_) {
        //纯音频: 按时间步进混音, 不依赖视频帧计数(每步约一视频帧时长)
        const long long total_samples = total_ms * sr / 1000;
        const long long chunk = sr / fps > 0 ? sr / fps : 1024;
        long long s = 0;
        while (s < total_samples && !stop_requested_.load()) {
            long long s1 = std::min(s + chunk, total_samples);
            progress_.store(total_samples > 0 ? (float)s / (float)total_samples : 1.0f);
            MixAndEncodeAudio(s, s1);
            s = s1;
        }
        progress_.store(1.0f);
    } else {
        for (long long i = 0; i < total_frames && !stop_requested_.load(); ++i) {
            progress_.store((float)i / (float)total_frames);
            long long timeline_ms = i * 1000 / fps;
            //视频合成编码
            CompositeAndEncodeVideo((int)i, timeline_ms);
            //音频混合编码(按帧推进采样区间)
            long long s0 = av_rescale(i, sr, (int)fps);
            long long s1 = av_rescale(i + 1, sr, (int)fps);
            MixAndEncodeAudio(s0, s1);
        }
    }
    //结尾不足一帧的音频补零发送
    FlushPendingAudio(sr);

    //停掉片段解码器
    for (auto& d : decoders_)
        if (d) StopClipDecoder(d.get());

    //结束编码线程(内部冲刷剩余帧与包), 再收尾封装;
    //纯音频模式视频编码器未初始化, 跳过
    if (video_enc_) {
        video_enc_->Exit();
        video_enc_->Wait();
    }
    audio_enc_->Exit();
    audio_enc_->Wait();
    mux_->Exit();
    mux_->Wait();
    if (mux_->has_error())
        error_ = mux_->error();

    if (mix_ctx_)
        avcodec_free_context(&mix_ctx_);

    running_.store(false);
    finished_.store(true);
}