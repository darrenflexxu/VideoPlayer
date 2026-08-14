#pragma once
#include "xmux.h"

class XCODEC_API XMuxTask : public XThread {
 public:
  void Main() override;
  /// <summary>
  /// 打开封装文件
  /// </summary>
  /// <param name="url">输出地址</param>
  /// <param name="video_para">视频参数</param>
  /// <param name="video_time_base">视频时间基数</param>
  /// <param name="audio_para">音频参数</param>
  /// <param name="audio_time_base">音频的时间基础</param>
  /// <returns></returns>
  bool Open(const char* url,
            AVCodecParameters* video_para = nullptr,
            AVRational* video_time_base = nullptr,
            AVCodecParameters* audio_para = nullptr,
            AVRational* audio_time_base = nullptr);

  // 接收数据
  void Do(AVPacket* pkt);

  bool EndOfMux();

  void set_block_size(int count) { block_size_ = count; }
  void ignoreMaxPkts(bool ignore) { pkts_.IgnoreMaxPackets(ignore); }

  // 毫秒PTS模式: 包时间戳已是输出时间轴上的绝对毫秒, 不再以首包为0重定基
  void set_ms_mode(bool on) { xmux_.set_ms_mode(on); }

  // 已写入封装的包总数(视频+音频)
  int packet_count();

  // 已写入封装的包在源时间轴上的最大毫秒位置(用于计算转码进度)
  long long output_ms() { return xmux_.output_ms(); }

  // 封装过程中是否发生了错误(打开/写头/写帧/写尾失败)
  bool has_error() { return has_error_; }
  std::string error() { return error_; }

 private:
  XMux xmux_;
  XAVPacketList pkts_;
  std::mutex mux_;
  bool end_mux_ = false;
  bool has_error_ = false;
  std::string error_;
  int block_size_ = 0;
  int packet_count_ = 0;
};
