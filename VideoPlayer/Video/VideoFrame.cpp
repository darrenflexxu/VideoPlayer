#include "VideoFrame.h"

void ReleaseVideoFrame(IVideoFrame* frame) {
    if (frame) {
        delete frame;
    }
}

VideoFrame::VideoFrame() {
    yuv420_buffer_ = nullptr;
}

VideoFrame::~VideoFrame() {
    if (yuv420_buffer_ != nullptr) {
        free(yuv420_buffer_);
        yuv420_buffer_ = nullptr;
    }
}

void VideoFrame::initBuffer(const int &width, const int &height) {
    if (yuv420_buffer_ != nullptr) {
        free(yuv420_buffer_);
        yuv420_buffer_ = nullptr;
    }
    width_ = width;
    height_ = height;
    yuv420_buffer_ = (uint8_t*)malloc(width * height * 3 / 2);
}

void VideoFrame::setYUVbuf(const uint8_t *buf) {
    int y_size = width_ * height_;
    memcpy(yuv420_buffer_, buf, y_size * 3 / 2);
}

void VideoFrame::setYbuf(const uint8_t *buf) {
    int y_size = width_ * height_;
    memcpy(yuv420_buffer_, buf, y_size);
}

void VideoFrame::setUbuf(const uint8_t *buf) {
    int y_size = width_ * height_;
    memcpy(yuv420_buffer_ + y_size, buf, y_size / 4);
}

void VideoFrame::setVbuf(const uint8_t *buf) {
    int y_size = width_ * height_;
    memcpy(yuv420_buffer_ + y_size + y_size / 4, buf, y_size / 4);
}