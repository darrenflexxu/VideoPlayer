/*/*******************************************************************************
**                                                                            **
**                     Jiedi(China nanjing)Ltd.                               **
**	               创建：丁宋涛 夏曹俊，此代码可用作为学习参考                **
*******************************************************************************/

/*****************************FILE INFOMATION***********************************
**
** Project       :FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课

** Contact       : xiacaojun@qq.com
**  博客   : http://blog.csdn.net/jiedichina
**	视频课程 : 网易云课堂	http://study.163.com/u/xiacaojun		
			   腾讯课堂		https://jiedi.ke.qq.com/				
			   csdn学院               http://edu.csdn.net/lecturer/lecturer_detail?lecturer_id=961	
**             51cto学院              http://edu.51cto.com/lecturer/index/user_id-12016059.html	
** 			   老夏课堂		http://www.laoxiaketang.com 
**                              更多资料请在此网页下载            http://ffmpeg.club
**  FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课  课程群 ：639014264加入群下载代码和学员交流
**                           微信公众号  : jiedi2007
**		头条号	 : 夏曹俊
**
*****************************************************************************
//！！！！！！！！！FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课 课程  QQ群：639014264下载代码和学员交流*/

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

/// <summary>
/// 音频播放 单件模式
/// </summary>
class XCODEC_API XAudioPlay
{
public:
    static XAudioPlay* Instance();

    virtual void Push(AVFrame* frame);
    virtual bool Open(AVCodecParameters* para);
    virtual bool Open(XPara &para);

    //打开音频 开始播放 调用回调函数
    virtual bool Open(XAudioSpec& spec) = 0;
    virtual void Close() = 0;

    //获取当前的播放位置
    virtual long long cur_pts() = 0;

    void Push(const unsigned char* data, int size,long long pts)
    {
        std::unique_lock<std::mutex> lock(mux_);
        audio_datas_.push_back(XData());
        audio_datas_.back().pts = pts;
        audio_datas_.back().data.assign(data, data + size);
    }

    //播放速度
    virtual void SetSpeed(float s)
    {
        auto spec = spec_;
        auto old_freq = spec.freq;
        spec.freq *= s;
        Open(spec);
        spec_.freq = old_freq;
    }

    //音量
    void set_volume(int v) 
    { 
        volume_ = v; 
    }
    
    //时间基数，用于生产播放进度
    void set_time_base(double b) { time_base_ = b; }
protected:
    double time_base_ = 0;
    XAudioPlay();
    virtual void Callback(unsigned char* stream, int len) = 0;
    static void AudioCallback(void* userdata, unsigned char* stream, int len)
    {
        auto ap = (XAudioPlay*)userdata;
        ap->Callback(stream, len);
    }
    std::list<XData> audio_datas_;//音频缓冲列表
    std::mutex mux_;
    unsigned char volume_ = 128;// 0~128 音量
    XAudioSpec spec_;
};

