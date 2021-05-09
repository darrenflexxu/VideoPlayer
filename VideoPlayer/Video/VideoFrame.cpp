#include "VideoFrame.h"

void ReleaseVideoFrame(IVideoFrame* frame) {
    if (frame) {
        delete frame;
    }
}

VideoFrame::VideoFrame() {
    buffer_ = nullptr;
}

VideoFrame::~VideoFrame() {
    if (buffer_ != nullptr) {
        free(buffer_);
        buffer_ = nullptr;
    }
}

void VideoFrame::initBuffer(const int &width, const int &height) {
    if (buffer_ != nullptr) {
        free(buffer_);
        buffer_ = nullptr;
    }
    width_ = width;
    height_ = height;
    buffer_ = (uint8_t*)malloc(width * height * 3 / 2);
}

void VideoFrame::setYUVbuf(const uint8_t *buf) {
    format_ = kPixelFormatYUV420P;
    int y_size = width_ * height_;
    memcpy(buffer_, buf, y_size * 3 / 2);
}