#pragma once

#include "types.h"

struct VideoPlayerCallBack;

struct DLL_API IVideoPlayer {
    virtual void setVideoPlayerCallBack(VideoPlayerCallBack *pointer) = 0;
    virtual bool startPlay(const char* filePath) = 0;
    virtual bool replay() = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual bool stop(bool isWait = false) = 0;
    virtual void seek(int64_t pos) = 0; //单位是微秒
    virtual void setMute(bool isMute) = 0;
    virtual void setVolume(float value) = 0;
    virtual float getVolume() = 0;
    virtual int64_t getTotalTime() = 0; //单位微秒
    virtual double getCurrentTime() = 0; //单位秒
};

extern "C"
{
    DLL_API IVideoPlayer* CreateVideoPlayer();
    DLL_API void ReleaseVideoPlayer(IVideoPlayer*);
}
