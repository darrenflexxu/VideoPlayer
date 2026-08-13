#include "predefine_header.h"

using namespace std;
bool XDemuxTask::Seek(long long ms)
{
    auto vp = demux_.CopyVideoPara();
    if (!vp)return false;
    auto pts = av_rescale_q(ms, { 1,1000 }, *vp->time_base);
    return demux_.Seek(pts, video_index());
}
void XDemuxTask::Stop()
{
    XThread::Stop();
    demux_.set_c(nullptr);
}
bool XDemuxTask::Open(std::string url, int timeout_ms)
{
    LOGDEBUG("XDemuxTask::Open begin!");
    demux_.set_c(nullptr);//断开之前的连接
    this->url_ = url;
    this->timeout_ms_ = timeout_ms;
    auto c = demux_.Open(url.c_str());
    if (!c)return false;
    demux_.set_c(c);
    demux_.set_time_out_ms(timeout_ms);
    LOGDEBUG("XDemuxTask::Open end!");
    return true;
}
void XDemuxTask::ClearEOF() {
  is_eof_ = false;
}
void XDemuxTask::Main()
{
    AVPacket pkt;

    while (!is_exit_)
    {
        if (is_pause())
        {
            MSleep(1);
            continue;
        }
        int errorCode = 0;

        if (!demux_.Read(&pkt, &errorCode))
        {
            //读取失败
            if (!demux_.is_connected())
            {
                Open(url_, timeout_ms_);
            }

            if (errorCode == AVERROR_EOF && !is_eof_) {
              is_eof_ = true;
              // 生成一个空包作为EOF标记(带视频流时走视频流, 纯音频走音频流)
              AVPacket eof = {};
              eof.stream_index = demux_.video_index() >= 0
                                     ? demux_.video_index()
                                     : demux_.audio_index();
              Next(&eof);
            }
            MSleep(1);
            continue;
        }

        //播放速度控制
        if (syn_type_ == XSYN_VIDEO &&
            pkt.stream_index == demux_.video_index())
        {
            auto dur = demux_.RescaleToMs(pkt.duration, pkt.stream_index);
            if (dur <= 0)
                dur = 40;
            //pkt.duration
            MSleep(dur);
        }
        read_pkt_count_ += 1;
        if (pkt.pts != AV_NOPTS_VALUE) {
          auto ms = demux_.RescaleToMs(pkt.pts, pkt.stream_index);
          if (ms > read_ms_) read_ms_ = ms;
        }
        Next(&pkt);
        av_packet_unref(&pkt);
        MSleep(1);
    }
    cout << endl << "demux packet(" << read_pkt_count_ << ")" << endl << flush;
}
