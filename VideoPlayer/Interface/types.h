#ifndef TYPES_H
#define TYPES_H

#include <stdio.h>
#include <stdint.h>

#ifdef WIN32
#ifdef VideoPlayer_EXPORTS
#define DLL_API _declspec(dllexport)
#else
#define DLL_API _declspec(dllimport)
#endif
#else
#define DLL_API
#endif

enum VideoPlayerState {
    VideoPlayer_Playing,
    VideoPlayer_Pause,
    VideoPlayer_Stop
};
extern "C" {

#if defined(WIN32)
#else
    void Sleep(long second);
#endif
    void DLL_API mSleep(int second);
    int64_t DLL_API getTimeStamp_MilliSecond(); //获取时间戳（毫秒）
}
#endif // TYPES_H
