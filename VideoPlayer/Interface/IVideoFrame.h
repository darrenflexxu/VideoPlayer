#pragma once

#include "types.h"

struct DLL_API IVideoFrame {
    enum PixelFormat {
        kPixelFormatYUV420P = 0,
        kPixelFormatUnknown
    };
    virtual PixelFormat format() = 0;
    virtual uint8_t * buffer() = 0;
    virtual int width() = 0;
    virtual int height() = 0;
};

extern "C"
{
    DLL_API void ReleaseVideoFrame(IVideoFrame*);
}
