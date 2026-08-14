#pragma once
#include "xformat.h"
#include <climits>
//////////////////////////////////////
/// 媒体封装

class XCODEC_API XMux :public XFormat
{
public:
    //////////////////////////////////////////////////
    //// 打开封装
    static AVFormatContext* Open(const char* url,
        AVCodecParameters* video_para = nullptr,
        AVCodecParameters* audio_para = nullptr
    );

    bool WriteHead();

    bool Write(AVPacket* pkt);

    bool WriteEnd();

    //已写入封装的包在源时间轴上的最大毫秒位置(用于计算转码进度)
    long long output_ms();

    //音视频时间基础
    void set_src_video_time_base(AVRational* tb);
    void set_src_audio_time_base(AVRational* tb);

    // 毫秒PTS模式(拼接/截取): 包时间戳已是输出时间轴上的绝对毫秒,
    // 不再以首包为0重定基, 否则首包非0(如第0段无音频时)会把后续时间轴整体平移
    void set_ms_mode(bool on) { ms_mode_ = on; }

    ~XMux();
private:
    AVRational* src_video_time_base_ = nullptr;
    AVRational* src_audio_time_base_ = nullptr;
    bool ms_mode_ = false;

    long long begin_video_pts_ = -1;//原视频开始时间
    long long begin_audio_pts_ = -1;//原音频开始时间
    long long output_ms_ = 0;       //已写入的最大源时间位置(毫秒)
    long long last_video_dts_ = LLONG_MIN;//已写入视频包的最大dts(输出时间基数, 强制单调)
    long long last_audio_dts_ = LLONG_MIN;//已写入音频包的最大dts(输出时间基数, 强制单调)
    int video_stream_index_ = 0;
    int audio_stream_index_ = 0;
};

