#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <list>
#include <thread>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include <libavutil/time.h>
#include "libavutil/pixfmt.h"
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_types.h>
#include <SDL_name.h>
#include <SDL_main.h>
#include <SDL_config.h>
}

#include "types.h"
#include "Mutex/Cond.h"
#include "Interface/IVideoPlayer.h"

#define SDL_AUDIO_BUFFER_SIZE 1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

#define MAX_AUDIO_SIZE (50 * 20)
#define MAX_VIDEO_SIZE (25 * 20)

#define FLUSH_DATA "FLUSH"

/**
 * @brief The VideoPlayer class
 * 用到了c++11的语法，需要编译器开启c++11支持
 * 播放器类，纯c++实现，方便移植，与界面的交互通过回调函数的方式实现
 */

class VideoPlayer : public IVideoPlayer {
public:
    VideoPlayer();
    virtual ~VideoPlayer();

    ///初始化播放器（必需要调用一次）
    static bool initPlayer();
    /**
     * @brief setVideoPlayerCallBack 设置播放器回调函数
     * @param pointer
     */
    void setVideoPlayerCallBack(VideoPlayerCallBack *pointer) { video_player_callback_ = pointer; }
    bool startPlay(const char* filePath);
    bool replay(); //重新播放
    bool play(); //播放（用于暂停后，重新开始播放）
    bool pause(); //暂停播放
    bool stop(bool isWait = false); //停止播放-参数表示是否等待所有的线程执行完毕再返回
    void seek(int64_t pos); //单位是微秒
    void setMute(bool isMute) { is_mute_ = isMute; }
    void setVolume(float value);
    float getVolume() { return volume_; }
    int64_t getTotalTime(); //单位微秒
    double getCurrentTime(); //单位秒

protected:
    static void sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len);
    void readVideoFile(); //读取视频文件
    void decodeVideoThread();
    void sdlAudioCallBack(Uint8 *stream, int len);
    int decodeAudioFrame(bool isBlock = false);

private:
    bool inputVideoQuene(const AVPacket &pkt);
    void clearVideoQuene();
    bool inputAudioQuene(const AVPacket &pkt);
    void clearAudioQuene();
    ///本播放器中SDL仅用于播放音频，不用做别的用途
    int openSDL();
    void closeSDL();
    ///回调函数相关，主要用于输出信息给界面
    ///打开文件失败
    void doOpenVideoFileFailed(const int &code = 0);
    ///打开sdl失败的时候回调此函数
    void doOpenSdlFailed(const int &code);
    ///获取到视频时长的时候调用此函数
    void doTotalTimeChanged(const int64_t &uSec);
    ///播放器状态改变的时候回调此函数
    void doPlayerStateChanged(const VideoPlayerState &state, const bool &hasVideo, const bool &hasAudio);
    ///显示视频数据，此函数不宜做耗时操作，否则会影响播放的流畅性。
    void doDisplayVideo(const uint8_t *yuv420Buffer, const int &width, const int &height);
        
    ///SDL播放音频相关
    SDL_AudioDeviceID audio_id_;
    ///回调函数
    VideoPlayerCallBack *video_player_callback_;
    std::string file_path_; //视频文件路径
    VideoPlayerState player_state_; //播放状态
    ///音量相关变量
    bool is_mute_;
    float volume_; //音量 0~1 超过1 表示放大倍数
    /// 跳转相关的变量
    int seek_req_; //跳转标志
    int64_t seek_pos_; //跳转的位置 -- 微秒
    int seek_flag_audio_;//跳转标志 -- 用于音频线程中
    int seek_flag_video_;//跳转标志 -- 用于视频线程中
    double seek_time_; //跳转的时间(秒)  值和seek_pos是一样的
    ///播放控制相关
    bool is_need_pause_; //暂停后跳转先标记此变量
    bool is_pause_;  //暂停标志
    bool is_quit_;   //停止
    bool is_read_finished_; //文件读取完毕
    bool is_read_thread_finished_;
    bool is_video_thread_finished_; //视频解码线程
    bool is_audio_thread_finished_; //音频播放线程
    ///音视频同步相关
    uint64_t video_start_time_; //开始播放视频的时间
    uint64_t pause_start_time_; //暂停开始的时间
    double audio_clock_; ///音频时钟
    double video_clock_; ///<pts of last decoded frame / predicted pts of next decoded frame
    AVStream *video_stream_; //视频流
    AVStream *audio_stream_; //音频流
    ///视频相关
    AVFormatContext *format_ctx_ = nullptr;
    AVCodecContext *codec_ctx_ = nullptr;
    const AVCodec *codec_ = nullptr;
    AVHWDeviceType hw_device_type_ = AV_HWDEVICE_TYPE_NONE;
    AVPixelFormat hw_pix_fmt_ = AV_PIX_FMT_NONE;
    AVBufferRef* hw_device_ctx_ = nullptr;
    ///音频相关
    AVCodecContext *audio_codec_ctx_;
    const AVCodec *audio_codec_;
    AVFrame *audio_frame_;
    ///以下变量用于音频重采样
    /// 由于ffmpeg解码出来后的pcm数据有可能是带平面的pcm，因此这里统一做重采样处理，
    /// 重采样成44100的16 bits 双声道数据(AV_SAMPLE_FMT_S16)
    AVFrame *audio_frame_resample_;
    SwrContext *swr_ctx_;
    enum AVSampleFormat in_sample_fmt_; //输入的采样格式
    enum AVSampleFormat out_sample_fmt_;//输出的采样格式 16bit PCM
    int in_sample_rate_;//输入的采样率
    int out_sample_rate_;//输出的采样率
    int audio_tgt_channels_; ///av_get_channel_layout_nb_channels(out_ch_layout);
    unsigned int audio_buf_size_;
    unsigned int audio_buf_index_;

#ifdef _MSC_VER
    __declspec(align(16)) uint8_t audio_buf_[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
#else
    uint8_t audio_buf_[MAX_AUDIO_FRAME_SIZE * 4] __attribute__((aligned(16)));
#endif
    ///视频帧队列
    Cond *condition_video_;
    std::list<AVPacket> video_pack_list_;
    ///音频帧队列
    Cond *condition_audio_;
    std::list<AVPacket> audio_pack_list_;
};
#endif // VIDEOPLAYER_H