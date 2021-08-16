#include "xplayer.h"
#include "xaudio_play.h"
//暂停或者播放
void XPlayer::Pause(bool is_pause)
{
    XThread::Pause(is_pause);
    demux_.Pause(is_pause);
    audio_decode_.Pause(is_pause);
    video_decode_.Pause(is_pause);
    XAudioPlay::Instance()->Pause(is_pause);
}
//设置视频播放位置，毫秒
bool XPlayer::Seek(long long ms)
{
    demux_.Seek(ms);
    audio_decode_.Clear();
    video_decode_.Clear();
    XAudioPlay::Instance()->Clear();
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
        delete view_;
        view_ = nullptr;
    }
    XAudioPlay::Instance()->Close();
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

        if (!video_decode_.Open(vp->para))
        {
            return false;
        }
        //用于过滤音频包
        video_decode_.set_stream_index(demux_.video_index());
        //缓冲
        video_decode_.set_block_size(100);
        //视频渲染
        if (!view_)
            view_ = XVideoView::Create();
        view_->set_win_id(winid);
        if (!view_->Init(vp->para))
            return false;
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

        //初始化音频播放
        XAudioPlay::Instance()->Open(*ap);

        //设置时间基数
        double time_base = 0;
    }
    else
    {
        demux_.set_syn_type(XSYN_VIDEO);//根据视频同步
    }

    //解封装数据传到当前类
    demux_.set_next(this);
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

//渲染视频 播放音频
void XPlayer::Update()
{
    //渲染视频
    if (view_)
    {
        auto f = video_decode_.GetFrame();
        if (f)
        {
            view_->DrawFrame(f);
            XFreeFrame(&f);
        }
    }

    //音频播放
    auto au = XAudioPlay::Instance();
    auto f = audio_decode_.GetFrame();
    if (!f)return;
    au->Push(f);
    XFreeFrame(&f);
}
void XPlayer::SetSpeed(float s)
{
    XAudioPlay::Instance()->SetSpeed(s);
}
void XPlayer::Main()
{
    long long syn = 0;
    auto au = XAudioPlay::Instance();
    auto ap = demux_.CopyAudioPara();
    auto vp = demux_.CopyVideoPara();
    video_decode_.set_time_base(vp->time_base);
    while (!is_exit_)
    {
        if (is_pause())
        {
            MSleep(1);
            continue;
        }
        this->pos_ms_ = video_decode_.cur_ms();
        if (ap)
        {
            syn = XRescale(au->cur_pts(), ap->time_base, vp->time_base);
            audio_decode_.set_syn_pts(au->cur_pts() + 10000);
            video_decode_.set_syn_pts(syn);
        }
        MSleep(1);
    }
}
