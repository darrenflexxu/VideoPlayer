#pragma once
#include "xdecode_task.h"
#include "xdemux_task.h"
#include "xencode_task.h"
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

  void Pause(bool is_pause) override;

  void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
  void set_gpu_encode(bool gpu) { gpu_encode_ = gpu; }

  std::shared_ptr<XPara> GetVideoCodec();
  std::shared_ptr<XPara> GetAudioCodec();

  float GetPos();

  // 转码完成(正常结束或被Stop终止)
  bool IsFinished() { return finished_; }
  bool HasError() { return !error_.empty(); }
  std::string GetError() { return error_; }

  // 输出一次转码过程统计, 用于调试
  std::string DumpInfo();

  // 实际是否使用了硬件编码/解码(失败回退软件后为 false)
  bool video_encode_gpu_used() { return video_encode_.gpu_used(); }
  bool video_decode_gpu_used() { return video_decode_.gpu_used(); }

 protected:
  XDemuxTask demux_;          // 解封装
  XDecodeTask audio_decode_;  // 音频解码
  XDecodeTask video_decode_;  // 视频解码
  XEncodeTask video_encode_;
  XEncodeTask audio_encode_;
  XMuxTask mux_;
  bool gpu_decode_ = false;
  bool gpu_encode_ = false;
  bool end_of_file_ = false;
  bool end_of_decode_ = false;
  bool end_of_encode_ = false;
  bool end_of_mux_ = false;
  int total_frame_count_ = 0;
  bool finished_ = false;
  std::string error_;
  long long start_time_ms_ = 0;
};
