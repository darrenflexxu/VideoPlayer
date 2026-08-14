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
    // 重置本次读取状态(拼接复用同一对象重开)
    read_ms_ = 0;
    is_eof_ = false;
    trim_end_reached_ = false;
    // 截取起点: 向后跳到起点之前的关键帧, 起点前的帧由编码侧丢弃
    if (trim_start_ms_ > 0) {
      int sidx = demux_.video_index();
      auto vp = demux_.CopyVideoPara();
      if (sidx < 0) {
        sidx = demux_.audio_index();
        vp = demux_.CopyAudioPara();
      }
      if (vp && sidx >= 0) {
        auto pts = av_rescale_q(trim_start_ms_, {1, 1000}, *vp->time_base);
        demux_.Seek(pts, sidx);
      }
    }
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
        long long ms = -1;
        if (pkt.pts != AV_NOPTS_VALUE) {
          ms = demux_.RescaleToMs(pkt.pts, pkt.stream_index);
          // 截取时进度按段内有效位置(减去起点)计算, 保证从0开始
          long long pos = ms;
          if (trim_start_ms_ > 0) {
            pos = ms - trim_start_ms_;
            if (pos < 0) pos = 0;
          }
          if (pos > read_ms_) read_ms_ = pos;
        }
        if (trim_end_ms_ > 0 && ms > trim_end_ms_) {
          // 超过截取终点: 丢弃该包并当作本段结束(注入EOF标记一次)
          av_packet_unref(&pkt);
          if (!trim_end_reached_) {
            trim_end_reached_ = true;
            is_eof_ = true;
            AVPacket eof = {};
            eof.stream_index = demux_.video_index() >= 0
                                   ? demux_.video_index()
                                   : demux_.audio_index();
            Next(&eof);
          }
          MSleep(1);
          continue;
        }
        Next(&pkt);
        av_packet_unref(&pkt);
        MSleep(1);
    }
    cout << endl << "demux packet(" << read_pkt_count_ << ")" << endl << flush;
}
