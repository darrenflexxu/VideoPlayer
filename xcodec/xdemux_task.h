#pragma once
#include "xdemux.h"
enum XSYN_TYPE
{
    XSYN_NONE = 0,  //不做同步
    XSYN_VIDEO = 1, //根据视频同步，不处理音频
};

class XCODEC_API XDemuxTask :public XThread
{
public:

    bool Seek(long long ms);

    ///音频索引
    int audio_index() { return demux_.audio_index(); }
    
    ///视频索引
    int video_index() { return demux_.video_index(); }

    ///入口线程函数
    void Main();

    /// <summary>
    /// 打开解封装
    /// </summary>
    /// <param name="url">rtsp地址</param>
    /// <param name="timeout_ms">超时时间 单位毫秒</param>
    /// <returns></returns>
    bool Open(std::string url,int timeout_ms = 1000);

    void ClearEOF();

    //复制视频参数
    std::shared_ptr<XPara> CopyVideoPara()
    {
        return demux_.CopyVideoPara();
    }
    std::shared_ptr<XPara> CopyAudioPara()
    {
        return demux_.CopyAudioPara();
    }
    ///设置同步类型，只支持视频
    void set_syn_type(XSYN_TYPE t) { syn_type_ = t; }

    ///读取到的数据包数量(调试输出用)
    int read_packet_count() { return read_pkt_count_; }

    //已读包在源时间轴上的最大毫秒位置(用于计算解封装进度)
    long long read_ms() { return read_ms_; }

    ///停止线程并清理资源，需要Wait等待线程结束
    void Stop();
private:
    XDemux demux_;
    std::string url_;
    int timeout_ms_ = 0;//超时时间
    XSYN_TYPE syn_type_ = XSYN_NONE;
    bool is_eof_ = false;
    int read_pkt_count_ = 0;
    long long read_ms_ = 0;  //已读包的最大源时间位置(毫秒)
};

