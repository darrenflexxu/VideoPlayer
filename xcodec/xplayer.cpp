#include "predefine_header.h"

//暂停或者播放
void XPlayer::Pause(bool is_pause)
{
    XThread::Pause(is_pause);
    demux_.Pause(is_pause);
    audio_decode_.Pause(is_pause);
    video_decode_.Pause(is_pause);
    //SDL设备全局共享: 仅主播放器操作暂停, 叠加层/后台预热只停自身线程
    if (own_audio_)
        XAudioPlay::Instance()->Pause(is_pause);
}

XPlayer::~XPlayer()
{
    Stop();   //停各子线程、释放渲染与音频资源
}
//设置视频播放位置，毫秒
bool XPlayer::Seek(long long ms)
{
    //解封装定位到目标之前的关键帧
    demux_.Seek(ms);
    //快进读取到目标位置(绕过实时限速)
    demux_.set_fast_forward_ms(ms);
    //先清空解码缓存
    audio_decode_.Clear();
    video_decode_.Clear();
    //只清本源缓冲(旧实现 Clear() 会清空所有源, 导致其他轨静音)
    XAudioPlay::Instance()->clearSource(audio_source_);
    //再设置seek目标: 关键帧与目标之间的帧由解码侧丢弃, 保证从目标位置开始播放
    //同时把当前进度置为目标值, 避免UI进度条先跳回旧位置
    video_decode_.set_seek_target_ms(ms);
    audio_decode_.set_seek_target_ms(ms);
    this->pos_ms_ = ms;
    return true;
}
void XPlayer::Stop()
{
    Exit();
    demux_.Exit();
    audio_decode_.Exit();
    video_decode_.Exit();
    Wait();
    demux_.Wait();
    audio_decode_.Wait();
    video_decode_.Wait();
    if (view_)
    {
        view_->Close();
        //外部注入的 view(如 UI 层 QOpenGLWidget)由调用方释放
        if (own_view_)
            delete view_;
        view_ = nullptr;
        own_view_ = false;
    }
    //多播放器混音: 移除本源的缓冲; SDL音频设备由时间轴共享常驻,
    //切段时不再关闭设备, 否则后一段因设备被关闭而无声
    XAudioPlay::Instance()->removeSource(audio_source_);
}
bool XPlayer::Open(const char* url, void* winid)
{
    //解封装
    if (!demux_.Open(url))
        return false;
    //视频解码
    auto vp = demux_.CopyVideoPara();
    if (vp)
    {
        //视频总时长
        this->total_ms_ = vp->total_ms;

        video_decode_.set_gpu_decode(gpu_decode_);
        video_decode_.set_gpu_direct_render(gpu_direct_render_);

        if (!video_decode_.Open(vp->para))
        {
            return false;
        }
        //用于过滤音频包
        video_decode_.set_stream_index(demux_.video_index());
        //缓冲
        video_decode_.set_block_size(100);
        //视频渲染: 叠加层播放器不创建/不绑定view, 由外层合成器取帧
        if (!view_ && !no_view_)
        {
            view_ = XVideoView::Create(XVideoView::Shader);
            own_view_ = true;
        }
        if (view_)
        {
            view_->set_win_id(winid);
            if (!view_->Init(vp->para))
                return false;
        }
    }

    auto ap = demux_.CopyAudioPara();
    if (ap)
    {
        //音频解码
        if (!audio_decode_.Open(ap->para))
        {
            return false;
        }
        //缓冲
        audio_decode_.set_block_size(100);

        //用于过滤视频数据
        audio_decode_.set_stream_index(demux_.audio_index());

        //frame 缓冲
        audio_decode_.set_frame_cache(true);

        //时间基数(用于seek后丢弃目标前的帧)
        audio_decode_.set_time_base(ap->time_base);

        //初始化音频播放: 仅拥有设备的播放器(主轨)打开SDL(同规格复用, 不重开),
        //混音源独立注册; 各源时钟独立, 切段互不干扰
        if (own_audio_)
            XAudioPlay::Instance()->Open(*ap);
        if (ap->time_base && ap->time_base->num > 0)
            audio_tb_ = (double)ap->time_base->den / (double)ap->time_base->num;
        bindAudioSource(audio_source_);
    }
    else
    {
        demux_.set_syn_type(XSYN_VIDEO);//根据视频同步
    }

    //纯音频文件(无视频流)以音频总长作为时长, 便于进度与耗尽判断
    if (!vp && ap)
        this->total_ms_ = ap->total_ms;

    //解封装数据传到当前类
    demux_.set_next(this);

    //清掉上次解码残留的"帧缓冲/EOF"状态, 保证重开文件走实时逐帧渲染路径
    ClearFinish();
    return true;
}

void XPlayer::Do(AVPacket* pkt)
{
    if(audio_decode_.is_open())
        audio_decode_.Do(pkt);
    if(video_decode_.is_open())
        video_decode_.Do(pkt);
}
void XPlayer::Start()
{
    demux_.Start();
    if(video_decode_.is_open())
        video_decode_.Start();
    if (audio_decode_.is_open())
        audio_decode_.Start();
    XThread::Start();
}

bool XPlayer::IsFinish() {
  //纯音频源: 以播放时钟到达总时长判断结束。
  //不能用解码帧缓存是否清空判断: Update每次只取一帧, 解码缓存会先于实际播放耗尽,
  //而混音源里仍有大量已缓冲音频在播放, 误判为结束会导致播放被中断。
  if (!video_decode_.is_open())
    return total_ms_ > 0 && pos_ms_ >= total_ms_ - 60;
  return video_decode_.IsVideoFinish();
}

void XPlayer::ClearFinish() {
  demux_.ClearEOF();
  video_decode_.ClearFinish();
  XVideoFilterProc::GetInstance()->Clear();
}

//渲染视频 播放音频
void XPlayer::Update()
{
    //渲染视频
  if (view_) {
    auto f = video_decode_.GetFrame();
    if (f) {
      view_->set_gpu_render_direct(gpu_decode_ && gpu_direct_render_);
      XVideoFilterProc::GetInstance()->Action(f);
      view_->DrawFrame(f, video_decode_.GetCodecContext());
      XFreeFrame(&f);
    }
  }

    //音频播放(注入混音器对应源)
    auto au = XAudioPlay::Instance();
    auto f = audio_decode_.GetFrame();
    if (!f)return;
    au->PushTo(audio_source_, f);
    XFreeFrame(&f);
}
void XPlayer::SetSpeed(float s)
{
    XAudioPlay::Instance()->SetSpeed(s);
}

bool XPlayer::GetVideoFrameCopy(AVFrame** out)
{
    *out = nullptr;
    if (!video_decode_.is_open())
        return false;
    auto f = video_decode_.GetFrame();
    if (!f)
        return false;
    //与渲染路径一致地应用滤镜(旋转/翻转等)
    XVideoFilterProc::GetInstance()->Action(f);
    AVFrame* copy = av_frame_alloc();
    if (!copy || av_frame_ref(copy, f) < 0)
    {
        if (copy)
            av_frame_free(&copy);
        XFreeFrame(&f);
        return false;
    }
    XFreeFrame(&f);
    *out = copy;
    return true;
}
//切换混音源绑定(基片换装用): 绑定源id并重置其独立时钟
void XPlayer::bindAudioSource(int id)
{
    audio_source_ = id;
    auto au = XAudioPlay::Instance();
    au->addSource(id);
    if (audio_tb_ > 0)
        au->set_source_time_base(id, audio_tb_);
    au->reset_source_clock(id);
}
//设置音量 0.0~1.0, 内部换算为SDL的0~128; 只作用于本播放器混音源
void XPlayer::SetVolume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    int sdl_vol = (int)(volume * 128.0f);
    XAudioPlay::Instance()->setSourceVolume(audio_source_, sdl_vol);
}
void XPlayer::Main()
{
    long long syn = 0;
    auto au = XAudioPlay::Instance();
    auto ap = demux_.CopyAudioPara();
    auto vp = demux_.CopyVideoPara();
    AVRational ms_tb{ 1, 1000 };
    if (vp)
        video_decode_.set_time_base(vp->time_base);
    while (!is_exit_)
    {
        if (is_pause())
        {
            MSleep(1);
            continue;
        }
        if (vp)
        {
            //视频源: 播放进度取视频解码位置(已按音频播放时钟同步)
            this->pos_ms_ = video_decode_.cur_ms();
            if (ap)
            {
                syn = XRescale(au->cur_pts(), ap->time_base, vp->time_base);
                audio_decode_.set_syn_pts(au->cur_pts() + 10000);
                video_decode_.set_syn_pts(syn);
            }
        }
        else if (ap)
        {
            //纯音频源: 取音频"播放"时钟作为播放进度;
            //不能用音频解码位置(解码会远跑在播放之前, 导致播放头跑到结尾)
            this->pos_ms_ = XRescale(au->cur_pts(), ap->time_base, &ms_tb);
        }
        MSleep(1);
    }
}
