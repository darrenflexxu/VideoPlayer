#pragma once

#include "types.h"

struct DLL_API IVideoFrame {
    virtual uint8_t * buffer() = 0; // YUV420
    virtual int width() = 0;
    virtual int height() = 0;
};

extern "C"
{
    DLL_API void ReleaseVideoFrame(IVideoFrame*);
}
