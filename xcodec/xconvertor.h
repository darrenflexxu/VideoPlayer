#pragma once
#include "xdecode_task.h"
#include "xdemux_task.h"
#include "xencode.h"
#include "xmux_task.h"
#include "xtools.h"
#include "xvideo_view.h"

class XCODEC_API XConvertor : public XThread {
 public:
  // 回调接收音视频包
  void Do(AVPacket* pkt) override;

  // 打开需要转码的音视频url
  bool Open(const char* url);
  void Stop();

  // 主线程 处理同步
  void Main() override;

  // 开启 解封装 音视频解码 和 处理同步的线程
  void Start(const char* url,
             AVCodecParameters* video_para,
             AVRational* video_time_base,
             AVCodecParameters* audio_para,
             AVRational* audio_time_base,
             const std::map<std::string, std::string>& video_opts,
             const std::map<std::string, std::string>& audio_opts);

  bool IsDecodeFinish();
  bool IsFinish();
  // 编码和封装
  void Update();

  void Pause(bool is_pause) override;

  void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
  void set_gpu_encode(bool gpu) { gpu_encode_ = gpu; }

  // 总时长 毫秒
  long long total_ms() { return total_ms_; }

  // 当前播放的位置 毫秒
  long long pos_ms() { return pos_ms_; }

 std::shared_ptr<XPara> GetVideoCodec();
  std::shared_ptr<XPara> GetAudioCodec();

 protected:
  XDemuxTask demux_;          // 解封装
  XDecodeTask audio_decode_;  // 音频解码
  XDecodeTask video_decode_;  // 视频解码
  XEncode video_encode_;
  XEncode audio_encode_;
  XMuxTask mux_;
  bool gpu_decode_ = false;
  bool gpu_encode_ = false;
  long long total_ms_ = 0;
  long long pos_ms_ = 0;
};
