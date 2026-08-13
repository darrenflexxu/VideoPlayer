#pragma once
#include "xtools.h"

extern "C" {
#include <libavutil/channel_layout.h>
}

struct AVFrame;
struct AVCodecContext;
struct SwrContext;

/// <summary>
/// 音频重采样(采样格式/采样率/声道布局转换)封装, 基于 libswresample
/// 转码时音频解码帧无法直接送入编码器时需要先重采样
/// </summary>
class XCODEC_API XResample {
 public:
  ~XResample();
  // 依据源frame参数与目标编码器上下文参数创建(或重建)重采样器
  bool Create(const AVFrame* src, const AVCodecContext* dst);
  // 需要重采样(未创建或参数有变化)
  bool Need(const AVFrame* src, const AVCodecContext* dst) const;
  bool is_active() const { return swr_ != nullptr; }
  // 转换一帧, 返回新帧(调用者用 av_frame_free 释放), 失败返回 nullptr
  AVFrame* Convert(AVFrame* in);

 private:
  SwrContext* swr_ = nullptr;
  int src_sample_rate_ = 0;
  int dst_sample_rate_ = 0;
  int src_fmt_ = 0;
  int dst_fmt_ = 0;
  AVChannelLayout src_ch_ = {};
  AVChannelLayout dst_ch_ = {};
};