#pragma once
#include "xtools.h"
#include <vector>

struct AVFrame;

/// <summary>
/// 音频降噪(FFT谱减)封装, 基于 libavfilter 的 afftdn
/// 导出时对混音后的立体声浮点帧滤波, 压低稳态底噪
/// </summary>
class XCODEC_API XAudioDenoise {
 public:
  ~XAudioDenoise();
  //打开降噪器: 采样率/声道数/降噪强度dB; 失败返回false(调用方可回退直通)
  bool Open(int sample_rate, int channels, float nr_db = 12.0f);
  void Close();
  bool is_open() const { return graph_ != nullptr; }
  //滤一帧(FLTP), 返回0或多帧输出(调用者逐个释放); 输入帧仍由调用者释放
  std::vector<AVFrame*> Filter(AVFrame* in);
  //冲刷内部延迟, 返回剩余输出帧(调用者释放)
  std::vector<AVFrame*> Flush();

 private:
  std::vector<AVFrame*> Drain();
  struct AVFilterGraph* graph_ = nullptr;
  struct AVFilterContext* src_ = nullptr;
  struct AVFilterContext* sink_ = nullptr;
};
