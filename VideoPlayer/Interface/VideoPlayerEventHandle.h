#ifndef VIDEOPLAYEREVENTHANDLE_H
#define VIDEOPLAYEREVENTHANDLE_H

#include "types.h"
#include "IVideoFrame.h"

struct AVFrame;
struct AVCodecContext;

struct DLL_API VideoPlayerCallBack {
    ///打开文件失败
    virtual void onOpenVideoFileFailed(const int &code = 0) = 0;
    ///打开sdl失败的时候回调此函数
    virtual void onOpenSdlFailed(const int &code) = 0;
    ///获取到视频时长的时候调用此函数
    virtual void onTotalTimeChanged(const int64_t &uSec) = 0;
    ///播放器状态改变的时候回调此函数
    virtual void onPlayerStateChanged(const VideoPlayerState &state, const bool &hasVideo, const bool &hasAudio) = 0;
    ///播放视频，此函数不宜做耗时操作，否则会影响播放的流畅性。
    virtual void onDisplayVideo(IVideoFrame* videoFrame) = 0;
    ///是否开启GPU解码
    virtual bool OnEnableGPUDecode() = 0;
    ///是否直接在显存中渲染视频
    virtual bool OnRenderGPUNoCopy() = 0;
    /// 播放视频
    virtual void onDisplayVideo(AVFrame* frame, AVCodecContext* codec_ctx) = 0;
};
#endif // VIDEOPLAERYEVENTHANDLE_H