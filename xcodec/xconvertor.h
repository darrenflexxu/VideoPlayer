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

  // 单文件截取区间(毫秒, 源时间轴), 0=不限; 在Open前调用
  void set_trim_ms(long long start_ms, long long end_ms) {
    start_ms_ = start_ms;
    end_ms_ = end_ms;
  }

  // 多片段拼接: urls为各片段(输出参数统一取第0段/用户指定), out_url为输出文件,
  // seg_trims[i]为该片段的截取区间(毫秒, 源时间轴, 0=不限); 需先Open(urls[0])
  void StartConcat(
      const std::vector<std::string>& urls,
      const char* out_url,
      AVCodecParameters* video_para,
      AVRational* video_time_base,
      AVCodecParameters* audio_para,
      AVRational* audio_time_base,
      const std::map<std::string, std::string>& video_opts,
      const std::map<std::string, std::string>& audio_opts,
      const std::vector<std::pair<long long, long long>>& seg_trims);

  void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
  void set_gpu_encode(bool gpu) { gpu_encode_ = gpu; }

  // 是否向控制台打印转码进度(\r进度 xx%), 默认开; --quiet等场景可关闭
  void set_show_progress(bool show) { show_progress_ = show; }

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
  // 打开输出编码器(不涉及mux), 生成临时输出参数(out_video_para/out_audio_para, 调用方释放)
  bool OpenEncoders(AVCodecParameters* video_para,
                    AVRational* video_time_base,
                    AVCodecParameters* audio_para,
                    AVRational* audio_time_base,
                    const std::map<std::string, std::string>& video_opts,
                    const std::map<std::string, std::string>& audio_opts,
                    AVCodecParameters** out_video_para,
                    AVCodecParameters** out_audio_para);
  // 打开某一片段的解封装+解码器(含截取seek与流布局校验), 并启动其线程
  bool OpenSegment(int i);
  void StartPipeline();  // 启动 mux/编码/解码/demux 线程
  void MainConcat();     // 拼接主循环(段切换/结束)
  // 分阶段进度: 解封装/写入按时间轴, 解码/编码按帧数, 链式收敛保证顺序
  void CalcStages(int& demux_p, int& dec_p, int& enc_p, int& mux_p,
                  int& bottleneck);

  // 解封装在输出时间轴上的毫秒位置(拼接时为各段累计)
  long long demux_pos_ms() {
    return concat_mode_ ? (seg_start_ms_ + demux_.read_ms()) : demux_.read_ms();
  }

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
  int video_total_frames_ = 0;  // 视频输出帧估算(分阶段进度用)
  int audio_total_frames_ = 0;  // 音频输出帧估算(分阶段进度用)
  long long total_ms_ = 0;  // 输入总时长(视频/音频取大), 用于按时间轴算进度
  bool finished_ = false;
  std::string error_;
  long long start_time_ms_ = 0;
  bool show_progress_ = true;
  int last_percent_ = -1;
  bool progress_open_ = false;
  long long start_ms_ = 0;  // 单文件截取起点(毫秒), 0=不截
  long long end_ms_ = 0;    // 单文件截取终点(毫秒), 0=不截
  bool concat_mode_ = false;
  std::vector<std::string> seg_urls_;
  std::vector<std::pair<long long, long long>> seg_trims_;
  std::vector<long long> seg_dur_ms_;     // 各片段有效时长(毫秒)
  std::vector<long long> seg_frame_dur_ms_;  // 各片段帧时长(毫秒, 段间留隙用)
  std::vector<bool> seg_has_video_;       // 各片段是否有视频流(布局校验)
  std::vector<bool> seg_has_audio_;
  int cur_seg_ = 0;
  int seg_count_ = 0;
  long long seg_start_ms_ = 0;  // 当前段之前各段有效时长之和(进度用)
  long long pts_offset_ms_ = 0; // 当前段输出时间轴偏移(毫秒)
  bool last_seg_ = false;       // 当前是否为最后一段
};
