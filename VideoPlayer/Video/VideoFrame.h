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
    void setYbuf(const uint8_t *buf);
    void setUbuf(const uint8_t *buf);
    void setVbuf(const uint8_t *buf);
    uint8_t * buffer() override { return yuv420_buffer_; }
    int width() override { return width_; }
    int height() override { return height_; }

protected:
    uint8_t *yuv420_buffer_;
    int width_;
    int height_;
};
#endif // VIDEOFRAME_H