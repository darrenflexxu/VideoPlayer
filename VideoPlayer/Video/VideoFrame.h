#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Interface/IVideoFrame.h"

class VideoFrame : public IVideoFrame {
public:
    VideoFrame();
    ~VideoFrame();
    void initBuffer(const int &width, const int &height);
    void setYUVbuf(const uint8_t *buf);
    PixelFormat format() override { return format_; }
    uint8_t * buffer() override { return buffer_; }
    int width() override { return width_; }
    int height() override { return height_; }

protected:
    PixelFormat format_ = kPixelFormatUnknown;
    uint8_t *buffer_;
    int width_;
    int height_;
};
#endif // VIDEOFRAME_H