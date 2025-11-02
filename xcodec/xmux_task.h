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

  int video_packet_count();

 private:
  XMux xmux_;
  XAVPacketList pkts_;
  std::mutex mux_;
  bool end_mux_ = false;
  int block_size_ = 0;
  int video_packet_count_ = 0;
};
