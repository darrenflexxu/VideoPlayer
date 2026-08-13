#pragma once
#include <deque>
#include <memory>
#include "xencode.h"
#include "xresample.h"

class XCODEC_API XEncodeTask : public XThread {
 public:
  /// <summary>
  /// 打开解码器
  /// </summary>
  bool Open(AVCodecParameters* para,
            const std::map<std::string, std::string>& opts);

  // 责任链处理函数
  void Do(AVFrame* frame) override;

  // 线程主函数
  void Main() override;

  void set_stream_index(int i) { stream_index_ = i; }

  bool is_open() { return is_open_; }

  void set_gpu_encode(bool gpu) { gpu_encode_ = gpu; }

  /// <summary>
  /// 清理缓存
  /// </summary>
  void Clear();

  void Stop();

  // 当前播放位置的毫秒
  long long cur_ms() { return cur_ms_; };

  void set_time_base(AVRational* time_base);

  AVCodecContext* GetCodecContext() const;

  bool EndEncode();

  void NextPacket(AVPacket* pkt);

  // 已经送入编码器的帧数量
  int frame_count() { return frame_count_; }
  // 实际使用的硬件加速编码状态(失败回退软件后为 false)
  bool gpu_used() { return gpu_used_; }
  // 编码器名称(如 libx264/h264_qsv), 未打开返回空
  const char* encoder_name();

 private:
  long long cur_pts_ = -1;  // 当前解码到的pts（以解码数据为准）
  AVRational* time_base_ = nullptr;
  long long cur_ms_ = 0;  // 当前播放位置的毫秒
  bool is_open_ = false;
  int stream_index_ = 0;
  std::mutex mux_;
  XEncode encode_;
  bool gpu_encode_ = false;
  bool gpu_used_ = false;
  bool end_encode_ = false;
  int frame_count_ = 0;
  int encode_fail_count_ = 0;  // 连续/累计送帧失败次数(用于运行时诊断)
  std::list<AVPacket*> pkts_cache_;
  std::deque<AVFrame*> frame_queue_;  // 待编码帧队列(Do入队, Main出队送帧)
  std::unique_ptr<XResample> resample_;  // 音频重采样(音频编码使用)
};
