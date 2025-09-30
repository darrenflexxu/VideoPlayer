#pragma once
#include "xdecode_task.h"
#include "xdemux_task.h"
#include "xtools.h"
#include "xvideo_view.h"

class XCODEC_API XConvertor : public XThread {
 public:
  // 回调接收音视频包
  void Do(AVPacket* pkt) override;

  // 打开音视频 初始化播放和渲染
  bool Open(const char* url, void* winid);
  void Stop();

  // 主线程 处理同步
  void Main() override;

  // 开启 解封装 音视频解码 和 处理同步的线程
  void Start();

  bool IsFinish();

  // 渲染视频 播放音频
  void Update();

  void Pause(bool is_pause) override;

  void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }

 protected:
  XDemuxTask demux_;          // 解封装
  XDecodeTask audio_decode_;  // 音频解码
  XDecodeTask video_decode_;  // 视频解码
  bool gpu_decode_ = false;
};
