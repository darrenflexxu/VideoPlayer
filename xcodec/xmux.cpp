#include "predefine_header.h"

using namespace std;

void PrintErr(int err);

#define BERR(err) if(err!= 0){PrintErr(err);return 0;}
void XMux::set_src_video_time_base(AVRational* tb)
{
    if (!tb)return;
    unique_lock<mutex> lock(mux_);
    if (!src_video_time_base_)
        src_video_time_base_ = new AVRational();
    *src_video_time_base_ = *tb;
}
void XMux::set_src_audio_time_base(AVRational* tb)
{
    if (!tb)return;
    unique_lock<mutex> lock(mux_);
    if (!src_audio_time_base_)
        src_audio_time_base_ = new AVRational();
    *src_audio_time_base_ = *tb;
}
XMux::~XMux()
{
    unique_lock<mutex> lock(mux_);
    if (src_video_time_base_)
        delete src_video_time_base_;
    src_video_time_base_ = nullptr;
    if (src_audio_time_base_)
        delete src_audio_time_base_;
    src_audio_time_base_ = nullptr;
}

//////////////////////////////////////////////////
//// 打开封装
AVFormatContext* XMux::Open(const char* url,
    AVCodecParameters* video_para ,
    AVCodecParameters* audio_para )
{
    AVFormatContext* c = nullptr;
    //创建上下文
    auto re = avformat_alloc_output_context2(&c, NULL, NULL, url);
    BERR(re);

    //添加视频音频流
    if (video_para)
    {
        auto vs = avformat_new_stream(c, NULL);   //视频流
        avcodec_parameters_copy(vs->codecpar, video_para);
    }
    if (audio_para)
    {
        auto as = avformat_new_stream(c, NULL);   //音频流
        avcodec_parameters_copy(as->codecpar, audio_para);
    }

    //打开IO
    re = avio_open(&c->pb, url, AVIO_FLAG_WRITE);
    BERR(re);
    av_dump_format(c, 0, url, 1);
    return c;
}
bool XMux::Write(AVPacket* pkt)
{
    if (!pkt)return false;
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    //没读取到pts 重构考虑通过duration 计算
    if (pkt->pts == AV_NOPTS_VALUE)
    {
        pkt->pts = 0;
        pkt->dts = 0;
    }
    // 异步编码器(QSV等)偶发dts缺失, 用pts兜底, 避免按INT64_MIN换算后变负
    if (pkt->dts == AV_NOPTS_VALUE)
        pkt->dts = pkt->pts;
    if (pkt->stream_index == video_index_)
    {
        if (begin_video_pts_ < 0)
            begin_video_pts_ = pkt->pts;
        if (src_video_time_base_ && pkt->pts >= begin_video_pts_) {
          auto ms = av_rescale_q(pkt->pts - begin_video_pts_,
                                 *src_video_time_base_, {1, 1000});
          if (ms > output_ms_) output_ms_ = ms;
        }
        lock.unlock();
        RescaleTime(pkt, begin_video_pts_, src_video_time_base_);
        lock.lock();

    }
    else if (pkt->stream_index == audio_index_)
    {
        if (begin_audio_pts_ < 0)
            begin_audio_pts_ = pkt->pts;
        if (src_audio_time_base_ && pkt->pts >= begin_audio_pts_) {
          auto ms = av_rescale_q(pkt->pts - begin_audio_pts_,
                                 *src_audio_time_base_, {1, 1000});
          if (ms > output_ms_) output_ms_ = ms;
        }
        lock.unlock();
        RescaleTime(pkt, begin_audio_pts_, src_audio_time_base_);
        lock.lock();
    }

    // av_interleaved_write_frame 要求每个流的dts严格递增(相等也会被拒绝)且
    // dts<=pts, 异步编码器(aac/QSV)在毫秒时间基下偶发发出重复/回退/缺失/
    // 超过pts的dts导致写入失败, 这里兜底: dts恒<=pts且严格递增
    if (pkt->stream_index == video_index_ || pkt->stream_index == audio_index_)
    {
        long long& last = pkt->stream_index == video_index_
                              ? last_video_dts_
                              : last_audio_dts_;
        if (pkt->dts > pkt->pts)
            pkt->dts = pkt->pts;
        if (last != LLONG_MIN && pkt->dts <= last)
        {
            pkt->dts = last + 1;
            if (pkt->dts > pkt->pts)
                pkt->pts = pkt->dts;  // 同步抬升pts, 保持pts>=dts
        }
        last = pkt->dts;
    }

    //写入一帧数据，内部缓冲排序dts，通过pkt=null 可以写入缓冲
    auto re = av_interleaved_write_frame(c_,pkt);
    BERR(re);
    return true;
}

bool XMux::WriteEnd()
{
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    int re = 0;
    //auto re = av_interleaved_write_frame(c_, nullptr);//写入排序缓冲
    //BERR(re);
    re = av_write_trailer(c_);
    BERR(re);
    return true;
}
long long XMux::output_ms()
{
    unique_lock<mutex> lock(mux_);
    return output_ms_;
}
bool XMux::WriteHead()
{
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    //会改变timebase
    auto re = avformat_write_header(c_, nullptr);
    BERR(re);

    //打印输出上下文
    av_dump_format(c_, 0, c_->url, 1);
    this->begin_audio_pts_ = -1;
    this->begin_video_pts_ = -1;
    this->last_video_dts_ = LLONG_MIN;
    this->last_audio_dts_ = LLONG_MIN;


    return true;
}