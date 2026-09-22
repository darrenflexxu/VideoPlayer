#pragma once
#include "xtools.h"
#define AUDIO_U8        0x0008  /**< Unsigned 8-bit samples */
#define AUDIO_S8        0x8008  /**< Signed 8-bit samples */
#define AUDIO_U16LSB    0x0010  /**< Unsigned 16-bit samples */
#define AUDIO_S16LSB    0x8010  /**< Signed 16-bit samples */
#define AUDIO_U16MSB    0x1010  /**< As above, but big-endian byte order */
#define AUDIO_S16MSB    0x9010  /**< As above, but big-endian byte order */
#define AUDIO_U16       AUDIO_U16LSB
#define AUDIO_S16       AUDIO_S16LSB
/* @} */

/**
 *  \name int32 support
 */
 /* @{ */
#define AUDIO_S32LSB    0x8020  /**< 32-bit integer samples */
#define AUDIO_S32MSB    0x9020  /**< As above, but big-endian byte order */
#define AUDIO_S32       AUDIO_S32LSB
/* @} */

/**
 *  \name float32 support
 */
 /* @{ */
#define AUDIO_F32LSB    0x8120  /**< 32-bit floating point samples */
#define AUDIO_F32MSB    0x9120  /**< As above, but big-endian byte order */
#define AUDIO_F32       AUDIO_F32LSB
/* @} */

/**
 *  \name Native audio byte ordering
 */
 /* @{ */
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS    AUDIO_U16LSB
#define AUDIO_S16SYS    AUDIO_S16LSB
#define AUDIO_S32SYS    AUDIO_S32LSB
#define AUDIO_F32SYS    AUDIO_F32LSB
#else
#define AUDIO_U16SYS    AUDIO_U16MSB
#define AUDIO_S16SYS    AUDIO_S16MSB
#define AUDIO_S32SYS    AUDIO_S32MSB
#define AUDIO_F32SYS    AUDIO_F32MSB
#endif
#include <vector>
#include <list>
#include <mutex>
struct XAudioSpec
{
    int freq = 44100;//音频采样率
    unsigned short format = AUDIO_S16SYS;
    unsigned char channels = 2;
    unsigned short samples = 1024;
};

struct XData
{
    std::vector<unsigned char> data;
    int offset = 0; //偏移位置
    long long pts = 0;
};

//多流混音: 每个音频源(对应一条音轨/一个播放器)独立的缓冲队列与音量
struct XAudioSource
{
    int id = 0;
    int volume = 128; //0~128
    std::list<XData> datas;
    long long mixed_bytes = 0; //累计已混音字节数(调试/进度用, 持锁统计)
};

/// <summary>
/// 音频播放 单件模式
/// </summary>
class XCODEC_API XAudioPlay
{
public:
    static XAudioPlay* Instance();
    
    //暂停
    virtual void Pause(bool is_pause) = 0;

    virtual void Push(AVFrame* frame);
    virtual bool Open(AVCodecParameters* para);
    virtual bool Open(XPara &para);

    //打开音频 开始播放 调用回调函数
    virtual bool Open(XAudioSpec& spec) = 0;
    virtual void Close() = 0;
    virtual void Clear()
    {
        Close();
        SetSpeed(speed_);
    }

    //获取当前的播放位置
    virtual long long cur_pts() = 0;

    //向默认音频源(0)推送数据(旧接口兼容)
    void Push(const unsigned char* data, int size,long long pts)
    {
        PushTo(0, data, size, pts);
    }

    //指定源推送PCM数据
    void PushTo(int id, const unsigned char* data, int size, long long pts)
    {
        std::unique_lock<std::mutex> lock(mux_);
        auto& s = source(id);
        s.datas.push_back(XData());
        s.datas.back().pts = pts;
        s.datas.back().data.assign(data, data + size);
    }
    //指定源推送解码帧(自动交错平面数据)
    void PushTo(int id, AVFrame* frame);

    //注册一个音频源(重复注册无害); 播放控制器在打开播放器时调用
    void addSource(int id);
    void removeSource(int id);
    //设置某源音量 0~128
    void setSourceVolume(int id, int v);
    //清空所有源缓冲(seek/切换时调用), 不重开SDL设备
    void ClearBuffers();
    //某源累计已混音字节数(调试/导出比对用)
    long long mixedBytes(int id);

    //播放速度
    virtual void SetSpeed(float s)
    {
        speed_ = s;
        auto spec = spec_;
        auto old_freq = spec.freq;
        spec.freq *= s;
        Open(spec);
        spec_.freq = old_freq;
    }

    //音量(默认源)
    void set_volume(int v) 
    { 
        setSourceVolume(0, v);
    }
    
    //时间基数，用于生产播放进度
    void set_time_base(double b) { time_base_ = b; }
protected:
    double time_base_ = 0;
    float speed_ = 1.;
    XAudioPlay();
    virtual void Callback(unsigned char* stream, int len) = 0;
    static void AudioCallback(void* userdata, unsigned char* stream, int len)
    {
        auto ap = (XAudioPlay*)userdata;
        ap->Callback(stream, len);
    }
//按id取得源引用(若不存在则创建, 需已持锁)
    XAudioSource& source(int id);
    std::vector<XAudioSource> sources_;
    std::mutex mux_;
    XAudioSpec spec_;
};

