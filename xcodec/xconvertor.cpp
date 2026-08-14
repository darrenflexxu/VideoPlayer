#include "predefine_header.h"

#include <cstdio>

// 渲染单行分段进度条: 总进度 + 各阶段▓░进度条 + 瓶颈标记
// 整行定长(各段名称固定/条10格/数字3位/瓶颈6列补齐), 保证\r覆盖无残影
// bottleneck: 瓶颈阶段索引(0-3), 4=无瓶颈(-)
static void PrintStageLine(int percent, int demux_p, int dec_p, int enc_p,
                           int mux_p, int bottleneck, bool newline) {
  const char* names[4] = {"解封装", "解码", "编码", "写入"};
  int pct[4] = {demux_p, dec_p, enc_p, mux_p};
  printf("\r进度 %3d%% ", percent);
  for (int i = 0; i < 4; ++i) {
    int f = pct[i] / 10;
    if (f > 10) f = 10;
    if (f < 0) f = 0;
    printf("|%s", names[i]);
    for (int k = 0; k < f; ++k) printf("▓");
    for (int k = f; k < 10; ++k) printf("░");
    printf("%3d%%", pct[i]);
  }
  // 瓶颈标记固定宽度(中文按6列补齐), 无瓶颈时显示 -
  const char* bn[5] = {"解封装", "解码  ", "编码  ", "写入  ", "—     "};
  printf(" |瓶颈:%s", bn[(bottleneck >= 0 && bottleneck < 4) ? bottleneck : 4]);
  if (newline) printf("\n");
  fflush(stdout);
}

// 暂停或者继续转码
void XConvertor::Pause(bool is_pause) {
  XThread::Pause(is_pause);
  demux_.Pause(is_pause);
  audio_decode_.Pause(is_pause);
  video_decode_.Pause(is_pause);
  video_encode_.Pause(is_pause);
  audio_encode_.Pause(is_pause);
  mux_.Pause(is_pause);
}

std::shared_ptr<XPara> XConvertor::GetVideoCodec() {
  return demux_.CopyVideoPara();
}

std::shared_ptr<XPara> XConvertor::GetAudioCodec() {
  return demux_.CopyAudioPara();
}

float XConvertor::GetPos() {
  if (finished_) {
    return 1.0f;
  }
  // 主路径: 输出时间戳/输入总时长(输入时长为容器精确值, 不受帧数估算误差影响)
  if (total_ms_ > 0) {
    auto done = mux_.output_ms();
    if (done >= total_ms_) {
      return 1.0f;
    }
    return float(done) / float(total_ms_);
  }
  // 无时长信息(直播等)退回包数估算
  if (total_frame_count_ <= 0) {
    return 0;
  }
  auto done = mux_.packet_count();
  if (done >= total_frame_count_) {
    return 1.0f;
  }
  return float(done) / float(total_frame_count_);
}

void XConvertor::Stop() {
  Exit();
  demux_.Exit();
  audio_decode_.Exit();
  video_decode_.Exit();
  video_encode_.Exit();
  audio_encode_.Exit();
  mux_.Exit();
  Wait();
  demux_.Wait();
  audio_decode_.Wait();
  video_decode_.Wait();
  video_encode_.Wait();
  audio_encode_.Wait();
  mux_.Wait();
  finished_ = true;
  // 取消/异常退出时进度行可能还挂着\r, 补换行避免后续输出接在同一行
  if (show_progress_ && progress_open_) {
    printf("\n");
    fflush(stdout);
    progress_open_ = false;
  }
}

bool XConvertor::Open(const char* url) {
  // 单文件截取区间(起点seek/终点停读由解封装处理)
  demux_.set_trim(start_ms_, end_ms_);
  // 解封装
  if (!demux_.Open(url))
    return false;
  // 视频解码
  auto vp = demux_.CopyVideoPara();
  if (vp) {
    video_decode_.set_gpu_decode(gpu_decode_);

    if (!video_decode_.Open(vp->para)) {
      return false;
    }
    // 用于过滤音频包
    video_decode_.set_stream_index(demux_.video_index());
    video_decode_.ignoreMaxPkts(true);
  }

  auto ap = demux_.CopyAudioPara();
  if (ap) {
    // 音频解码
    if (!audio_decode_.Open(ap->para)) {
      return false;
    }
    // 用于过滤视频数据
    audio_decode_.set_stream_index(demux_.audio_index());
    audio_decode_.ignoreMaxPkts(true);
  }
  demux_.set_next(this);
  video_decode_.set_next(&video_encode_);
  audio_decode_.set_next(&audio_encode_);
  video_encode_.set_next(&mux_);
  audio_encode_.set_next(&mux_);
  return true;
}

void XConvertor::Do(AVPacket* pkt) {
  if (!pkt) return;
  // EOF标记包(buf为空), 结束解封装
  if (pkt->buf == nullptr) {
    demux_.Exit();
    end_of_file_ = true;
    return;
  }
  if (audio_decode_.is_open())
    audio_decode_.Do(pkt);
  if (video_decode_.is_open())
    video_decode_.Do(pkt);
}

void XConvertor::Start(const char* url,
                       AVCodecParameters* video_para,
                       AVRational* video_time_base,
                       AVCodecParameters* audio_para,
                       AVRational* audio_time_base,
                       const std::map<std::string, std::string>& video_opts,
                       const std::map<std::string, std::string>& audio_opts) {
  finished_ = false;
  error_.clear();
  concat_mode_ = false;
  end_of_file_ = end_of_decode_ = end_of_encode_ = end_of_mux_ = false;
  start_time_ms_ = NowMs();
  last_percent_ = -1;
  progress_open_ = false;

  // 单文件截取: 起点之前的帧由编码器丢弃(毫秒PTS模式, 输出时间轴统一毫秒)
  video_encode_.set_ms_pts_mode(start_ms_ > 0);
  audio_encode_.set_ms_pts_mode(start_ms_ > 0);
  video_encode_.set_trim_start_ms(start_ms_);
  audio_encode_.set_trim_start_ms(start_ms_);

  AVCodecParameters* tmp_video_para = nullptr;
  AVCodecParameters* tmp_audio_para = nullptr;
  if (!OpenEncoders(video_para, video_time_base, audio_para, audio_time_base,
                    video_opts, audio_opts, &tmp_video_para, &tmp_audio_para)) {
    return;
  }
  AVRational enc_video_time_base = {1, 1000};
  AVRational enc_audio_time_base = {1, 1000};
  if (video_encode_.is_open())
    enc_video_time_base = video_encode_.GetCodecContext()->time_base;
  if (audio_encode_.is_open())
    enc_audio_time_base = audio_encode_.GetCodecContext()->time_base;

  mux_.ignoreMaxPkts(true);
  // 截取模式: 编码器输出绝对毫秒时间戳, 封装时不再以首包为0重定基
  mux_.set_ms_mode(start_ms_ > 0);
  if (!mux_.Open(url, tmp_video_para, &enc_video_time_base, tmp_audio_para,
                 &enc_audio_time_base)) {
    error_ = "创建输出文件失败: ";
    if (tmp_video_para) avcodec_parameters_free(&tmp_video_para);
    if (tmp_audio_para) avcodec_parameters_free(&tmp_audio_para);
    return;
  }
  if (tmp_video_para) {
    avcodec_parameters_free(&tmp_video_para);
  }
  if (tmp_audio_para) {
    avcodec_parameters_free(&tmp_audio_para);
  }

  // 估算总帧数(有些文件nb_frames为0, 用时长*帧率/采样率推算)
  total_frame_count_ = 0;
  video_total_frames_ = 0;
  audio_total_frames_ = 0;
  total_ms_ = 0;
  {
    auto vc = demux_.CopyVideoPara();
    auto ac = demux_.CopyAudioPara();
    if (vc) {
      if (vc->total_ms > total_ms_) total_ms_ = vc->total_ms;
      if (vc->frame_count > 0) {
        video_total_frames_ += vc->frame_count;
      } else if (vc->para->framerate.num > 0) {
        video_total_frames_ +=
            vc->total_ms * vc->para->framerate.num / vc->para->framerate.den /
            1000;
      }
      total_frame_count_ += video_total_frames_;
    }
    if (ac) {
      if (ac->total_ms > total_ms_) total_ms_ = ac->total_ms;
      int frame_size =
          ac->para->frame_size > 0 ? ac->para->frame_size : 1024;
      if (ac->frame_count > 0) {
        audio_total_frames_ += ac->frame_count;
      } else if (ac->para->sample_rate > 0) {
        audio_total_frames_ += ac->total_ms * ac->para->sample_rate / 1000 /
                               frame_size;
      }
      total_frame_count_ += audio_total_frames_;
    }
    if (total_frame_count_ <= 0) {
      total_frame_count_ = 1;  // 防止除零
    }
  }

  // 单文件截取: 总时长与帧数分母按有效区间缩放(进度/GetPos用)
  if ((start_ms_ > 0 || end_ms_ > 0) && total_ms_ > 0) {
    long long eff =
        (end_ms_ > 0 && end_ms_ < total_ms_ ? end_ms_ : total_ms_) - start_ms_;
    if (eff < 0) eff = 0;
    if (eff < total_ms_) {
      double ratio = (double)eff / (double)total_ms_;
      video_total_frames_ = (int)(video_total_frames_ * ratio);
      audio_total_frames_ = (int)(audio_total_frames_ * ratio);
      total_ms_ = eff;
      total_frame_count_ = (video_encode_.is_open() ? video_total_frames_ : 0) +
                           (audio_encode_.is_open() ? audio_total_frames_ : 0);
      if (total_frame_count_ <= 0) {
        total_frame_count_ = 1;  // 防止除零
      }
    }
  }

  XThread::Start();
  StartPipeline();
}

// 分阶段进度: 解封装/写入按时间轴(精确), 解码/编码按帧数估算
// 瓶颈用收敛前的原始值判定; 收敛保证流水线顺序 解封装>=解码>=编码>=写入
void XConvertor::CalcStages(int& demux_p, int& dec_p, int& enc_p, int& mux_p,
                            int& bottleneck) {
  int raw[4] = {0, 0, 0, 0};
  if (total_ms_ > 0) {
    raw[0] = (int)(demux_pos_ms() * 100 / total_ms_);
    raw[3] = (int)(mux_.output_ms() * 100 / total_ms_);
    if (raw[0] > 100) raw[0] = 100;
    if (raw[3] > 100) raw[3] = 100;
  }
  if (total_frame_count_ > 0) {
    // 只统计实际参与转码的流(例如 -v none 时不把视频帧计入分母)
    int act_total = (video_encode_.is_open() ? video_total_frames_ : 0) +
                    (audio_encode_.is_open() ? audio_total_frames_ : 0);
    int act_dec =
        (video_encode_.is_open() ? video_decode_.get_Current_decode_frame_count()
                                 : 0) +
        (audio_encode_.is_open() ? audio_decode_.get_Current_decode_frame_count()
                                 : 0);
    int act_enc = (video_encode_.is_open() ? video_encode_.frame_count() : 0) +
                  (audio_encode_.is_open() ? audio_encode_.frame_count() : 0);
    if (act_total > 0) {
      raw[1] = (int)(act_dec * 100 / act_total);
      raw[2] = (int)(act_enc * 100 / act_total);
      if (raw[1] > 100) raw[1] = 100;
      if (raw[2] > 100) raw[2] = 100;
    }
  }
  // 时间轴进度受"末帧pts<容器时长"误差影响到不了100, 阶段真正完成后按完成标志置100
  if (end_of_file_) raw[0] = 100;  // 解封装已读到EOF(所有包已读取)
  if (end_of_mux_) raw[3] = 100;   // 写入已结束(编码器输出全部排空)
  // 瓶颈: 收敛前原始值中的最小者, 全部相等时无瓶颈
  bottleneck = 4;
  {
    int bi = 0;
    for (int i = 1; i < 4; ++i)
      if (raw[i] < raw[bi]) bi = i;
    if (!(raw[0] == raw[1] && raw[1] == raw[2] && raw[2] == raw[3]))
      bottleneck = bi;
  }
  // 链式收敛: 下游不可能快于上游
  demux_p = raw[0];
  dec_p = raw[1] < raw[0] ? raw[1] : raw[0];
  enc_p = raw[2] < dec_p ? raw[2] : dec_p;
  mux_p = raw[3] < enc_p ? raw[3] : enc_p;
}

bool XConvertor::OpenEncoders(
    AVCodecParameters* video_para,
    AVRational* video_time_base,
    AVCodecParameters* audio_para,
    AVRational* audio_time_base,
    const std::map<std::string, std::string>& video_opts,
    const std::map<std::string, std::string>& audio_opts,
    AVCodecParameters** out_video_para,
    AVCodecParameters** out_audio_para) {
  *out_video_para = nullptr;
  *out_audio_para = nullptr;
  if (video_para) {
    // 编码器按源时间基数接收帧pts, 保证编码后包的时间基数与源一致
    // (拼接毫秒模式时Open内部改为{1,1000})
    video_encode_.set_gpu_encode(gpu_encode_);
    video_encode_.set_time_base(video_time_base);
    video_encode_.Open(video_para, video_opts);
    if (!video_encode_.is_open()) {
      error_ = "打开视频编码器失败(编解码器不支持?)";
      return false;
    }
    auto c = video_encode_.GetCodecContext();
    c->refs = 4;
    c->gop_size = 2;
    c->max_b_frames = 0;
    video_encode_.set_stream_index(0);
    *out_video_para = avcodec_parameters_alloc();
    avcodec_parameters_from_context(*out_video_para, c);
  }

  if (audio_para) {
    audio_encode_.set_gpu_encode(false);  // 音频不做硬件编码
    audio_encode_.set_time_base(audio_time_base);
    audio_encode_.Open(audio_para, audio_opts);
    if (!audio_encode_.is_open()) {
      error_ = "打开音频编码器失败(编解码器不支持?)";
      if (*out_video_para) avcodec_parameters_free(out_video_para);
      return false;
    }
    // 纯音频时音频流索引为0, 有视频时为1
    audio_encode_.set_stream_index(video_encode_.is_open() ? 1 : 0);
    *out_audio_para = avcodec_parameters_alloc();
    avcodec_parameters_from_context(*out_audio_para,
                                    audio_encode_.GetCodecContext());
  }
  return true;
}

// 启动 mux/编码/解码/demux 流水线线程
void XConvertor::StartPipeline() {
  mux_.Start();
  if (video_encode_.is_open()) {
    video_encode_.Start();
  }
  if (audio_encode_.is_open()) {
    audio_encode_.Start();
  }
  if (video_decode_.is_open())
    video_decode_.Start();
  if (audio_decode_.is_open())
    audio_decode_.Start();
  demux_.Start();
}

// 打开某一片段的解封装+解码器(含截取seek与流布局校验)
bool XConvertor::OpenSegment(int i) {
  long long ss = i < (int)seg_trims_.size() ? seg_trims_[i].first : 0;
  long long to = i < (int)seg_trims_.size() ? seg_trims_[i].second : 0;
  demux_.set_trim(ss, to);
  if (!demux_.Open(seg_urls_[i].c_str())) {
    char b[128];
    snprintf(b, sizeof(b), "打开片段%d失败", i + 1);
    error_ = b;
    return false;
  }
  auto vp = demux_.CopyVideoPara();
  auto ap = demux_.CopyAudioPara();
  // 输出流布局为所有片段的并集: 本段缺的流该段留空(视频无画面/音频静音)
  bool want_video = want_video_;
  bool want_audio = want_audio_;
  // 本段无音频流: 输出需补静音帧, 使音频轨从0开始连续(首段无音频时不能留空,
  // 否则mux会以首个真实音频包为0重定基, 破坏后续时间戳)
  silent_audio_seg_ = want_audio && !ap;
  video_decode_.set_gpu_decode(gpu_decode_);
  if (want_video) {
    if (vp) {
      video_decode_.set_stream_index(demux_.video_index());
      video_decode_.ignoreMaxPkts(true);
      if (!video_decode_.Open(vp->para)) {
        char b[128];
        snprintf(b, sizeof(b), "打开片段%d视频解码器失败", i + 1);
        error_ = b;
        return false;
      }
      video_decode_.set_time_base(vp->time_base);
      // 编码器跨段复用, 各段源时间基数可能不同(如mkv为1/1000), 必须同步更新,
      // 否则毫秒模式用第0段的时间基数换算本段帧pts会产生错误时间戳
      if (video_encode_.is_open()) video_encode_.set_time_base(vp->time_base);
    } else {
      // 本段无视频: 关闭解码器, 该段视频留空
      video_decode_.Stop();
      if (i > 0)
        std::cout << "片段" << (i + 1) << "无视频流, 该段输出视频将留空" << std::endl;
    }
  }
  if (want_audio) {
    if (ap) {
      audio_decode_.set_stream_index(demux_.audio_index());
      audio_decode_.ignoreMaxPkts(true);
      if (!audio_decode_.Open(ap->para)) {
        char b[128];
        snprintf(b, sizeof(b), "打开片段%d音频解码器失败", i + 1);
        error_ = b;
        return false;
      }
      audio_decode_.set_time_base(ap->time_base);
      if (audio_encode_.is_open()) audio_encode_.set_time_base(ap->time_base);
    } else {
      // 本段无音频: 关闭解码器, 该段音频静音
      audio_decode_.Stop();
      if (i > 0)
        std::cout << "片段" << (i + 1) << "无音频流, 该段输出音频将静音" << std::endl;
    }
  }
  return true;
}

// 当前段无音频流时, 按该段有效时长向音频编码器补静音帧, 使输出音频轨连续
// (帧pts换算成编码器输入时间基数, Do()里转回毫秒并叠加段偏移; 背压自限速)
void XConvertor::FeedSilentAudio() {
  if (!audio_encode_.is_open() || !silent_audio_seg_) return;
  int i = cur_seg_;
  long long ss = i < (int)seg_trims_.size() ? seg_trims_[i].first : 0;
  long long eff = i < (int)seg_dur_ms_.size() ? seg_dur_ms_[i] : 0;
  if (eff <= 0) return;
  auto c = audio_encode_.GetCodecContext();
  if (!c || c->sample_rate <= 0) return;
  int fs = c->frame_size > 0 ? c->frame_size : 1024;
  long long frame_ms = 1000LL * fs / c->sample_rate;
  if (frame_ms <= 0) frame_ms = 21;
  AVRational tb = audio_encode_.cur_time_base();
  AVRational ms_tb = {1, 1000};
  int n = (int)((eff + frame_ms - 1) / frame_ms);
  fprintf(stderr, "[DBG-FS] seg%d silent feed n=%d eff=%lld off=%lld\n", i, n,
          eff, pts_offset_ms_);
  for (int j = 0; j < n; ++j) {
    AVFrame* f = av_frame_alloc();
    if (!f) break;
    f->format = c->sample_fmt;
    f->sample_rate = c->sample_rate;
    av_channel_layout_copy(&f->ch_layout, &c->ch_layout);
    f->nb_samples = fs;
    if (av_frame_get_buffer(f, 0) < 0) {
      av_frame_free(&f);
      break;
    }
    f->pts = av_rescale_q(ss + j * frame_ms, ms_tb, tb);
    av_samples_set_silence(f->data, 0, f->nb_samples, f->ch_layout.nb_channels,
                           (AVSampleFormat)f->format);
    audio_encode_.Do(f);
  }
}

// 多片段拼接: 复用已Open的第0段, 编码器/mux全程复用, 段间只换解封装+解码器
void XConvertor::StartConcat(
    const std::vector<std::string>& urls,
    const char* out_url,
    AVCodecParameters* video_para,
    AVRational* video_time_base,
    AVCodecParameters* audio_para,
    AVRational* audio_time_base,
    const std::map<std::string, std::string>& video_opts,
    const std::map<std::string, std::string>& audio_opts,
    const std::vector<std::pair<long long, long long>>& seg_trims) {
  finished_ = false;
  error_.clear();
  concat_mode_ = true;
  end_of_file_ = end_of_decode_ = end_of_encode_ = end_of_mux_ = false;
  start_time_ms_ = NowMs();
  last_percent_ = -1;
  progress_open_ = false;

  seg_urls_ = urls;
  seg_trims_ = seg_trims;
  seg_count_ = (int)urls.size();
  cur_seg_ = 0;
  last_seg_ = (seg_count_ == 1);
  seg_start_ms_ = 0;
  pts_offset_ms_ = 0;
  seg_dur_ms_.clear();
  seg_frame_dur_ms_.clear();
  seg_has_video_.clear();
  seg_has_audio_.clear();
  total_ms_ = 0;
  video_total_frames_ = 0;
  audio_total_frames_ = 0;
  total_frame_count_ = 0;

  // 编码器毫秒PTS模式(整个拼接复用同一编码器, 段间只更新偏移/截取)
  video_encode_.set_ms_pts_mode(true);
  audio_encode_.set_ms_pts_mode(true);
  video_encode_.set_pts_offset_ms(0);
  audio_encode_.set_pts_offset_ms(0);
  long long ss0 = seg_trims_.empty() ? 0 : seg_trims_[0].first;
  video_encode_.set_trim_start_ms(ss0);
  audio_encode_.set_trim_start_ms(ss0);

  // 探测各片段时长/帧时长, 计算有效时长与进度分母;
  // 同时抓取并集流参数: 当第0段缺某流但后面片段有, 用第一个含该流的片段参数打开编码器
  AVCodecParameters* union_video_para = nullptr;
  AVCodecParameters* union_audio_para = nullptr;
  AVRational union_video_tb = {0, 0};
  AVRational union_audio_tb = {0, 0};
  bool got_uv = false, got_ua = false;
  for (int i = 0; i < seg_count_; ++i) {
    long long full = 0;
    long long vfd = 40, afd = 40;  // 视频/音频帧时长(毫秒)估算, 兜底40ms
    bool hv = false, ha = false;
    AVFormatContext* fc = nullptr;
    if (avformat_open_input(&fc, seg_urls_[i].c_str(), nullptr, nullptr) == 0) {
      if (avformat_find_stream_info(fc, nullptr) >= 0) {
        for (unsigned s = 0; s < fc->nb_streams; ++s) {
          auto st = fc->streams[s];
          long long dur = 0;
          if (st->duration > 0)
            dur = av_rescale_q(st->duration, st->time_base, {1, 1000});
          else
            dur = av_rescale_q(fc->duration, AV_TIME_BASE_Q, {1, 1000});
          if (dur > full) full = dur;
          auto t = st->codecpar->codec_type;
          if (t == AVMEDIA_TYPE_VIDEO) {
            hv = true;
            if (!got_uv) {
              union_video_para = avcodec_parameters_alloc();
              avcodec_parameters_copy(union_video_para, st->codecpar);
              union_video_tb = st->time_base;
              got_uv = true;
            }
            if (st->codecpar->framerate.num > 0 && st->codecpar->framerate.den > 0)
              vfd = 1000LL * st->codecpar->framerate.den /
                    st->codecpar->framerate.num;
          } else if (t == AVMEDIA_TYPE_AUDIO) {
            ha = true;
            if (!got_ua) {
              union_audio_para = avcodec_parameters_alloc();
              avcodec_parameters_copy(union_audio_para, st->codecpar);
              union_audio_tb = st->time_base;
              got_ua = true;
            }
            if (st->codecpar->sample_rate > 0) {
              int fs = st->codecpar->frame_size > 0 ? st->codecpar->frame_size
                                                    : 1024;
              afd = 1000LL * fs / st->codecpar->sample_rate;
            }
          }
        }
      }
      avformat_close_input(&fc);
    }
    seg_has_video_.push_back(hv);
    seg_has_audio_.push_back(ha);
    long long ss = i < (int)seg_trims_.size() ? seg_trims_[i].first : 0;
    long long to = i < (int)seg_trims_.size() ? seg_trims_[i].second : 0;
    long long eff = full;
    if (to > 0 && to < eff) eff = to;
    eff -= ss;
    if (eff < 0) eff = 0;
    seg_dur_ms_.push_back(eff);
    seg_frame_dur_ms_.push_back(vfd > afd ? vfd : afd);
    total_ms_ += eff;
    if (eff > 0) {
      if (hv && vfd > 0) video_total_frames_ += (int)(eff / vfd);
      if (ha && afd > 0) audio_total_frames_ += (int)(eff / afd);
    }
  }
  if (total_ms_ <= 0) total_ms_ = 1;  // 防止除零

  // 输出流布局为所有片段的并集(第0段缺的流, 若后面片段有则保留)
  want_video_ = false;
  want_audio_ = false;
  for (bool b : seg_has_video_) want_video_ |= b;
  for (bool b : seg_has_audio_) want_audio_ |= b;

  // 打开输出编码器+封装(一次, 后续段不复用编码器重开)
  AVCodecParameters* tmp_video_para = nullptr;
  AVCodecParameters* tmp_audio_para = nullptr;
  AVCodecParameters* eff_video_para = video_para;
  AVCodecParameters* eff_audio_para = audio_para;
  AVRational* eff_video_tb = video_time_base;
  AVRational* eff_audio_tb = audio_time_base;
  if (want_video_ && !eff_video_para && got_uv) {
    eff_video_para = union_video_para;
    eff_video_tb = &union_video_tb;
  }
  if (want_audio_ && !eff_audio_para && got_ua) {
    eff_audio_para = union_audio_para;
    eff_audio_tb = &union_audio_tb;
  }
  if (!OpenEncoders(eff_video_para, eff_video_tb, eff_audio_para, eff_audio_tb,
                    video_opts, audio_opts, &tmp_video_para, &tmp_audio_para)) {
    avcodec_parameters_free(&union_video_para);
    avcodec_parameters_free(&union_audio_para);
    return;
  }
  if (union_video_para) avcodec_parameters_free(&union_video_para);
  if (union_audio_para) avcodec_parameters_free(&union_audio_para);
  AVRational enc_video_time_base = {1, 1000};
  AVRational enc_audio_time_base = {1, 1000};
  if (video_encode_.is_open())
    enc_video_time_base = video_encode_.GetCodecContext()->time_base;
  if (audio_encode_.is_open())
    enc_audio_time_base = audio_encode_.GetCodecContext()->time_base;

  mux_.ignoreMaxPkts(true);
  // 拼接模式: 编码器输出绝对毫秒时间戳, 封装时不再以首包为0重定基,
  // 否则第0段无音频时首包非0会把后续音频时间轴整体平移
  mux_.set_ms_mode(true);
  if (!mux_.Open(out_url, tmp_video_para, &enc_video_time_base, tmp_audio_para,
                 &enc_audio_time_base)) {
    error_ = "创建输出文件失败: ";
    if (tmp_video_para) avcodec_parameters_free(&tmp_video_para);
    if (tmp_audio_para) avcodec_parameters_free(&tmp_audio_para);
    return;
  }
  if (tmp_video_para) avcodec_parameters_free(&tmp_video_para);
  if (tmp_audio_para) avcodec_parameters_free(&tmp_audio_para);

  // 静音段的音频帧计入进度分母, 避免补静音后音频编码/写入进度提前到100
  if (audio_encode_.is_open()) {
    auto ac = audio_encode_.GetCodecContext();
    long long fm = 21;
    if (ac && ac->sample_rate > 0) {
      int fs = ac->frame_size > 0 ? ac->frame_size : 1024;
      fm = 1000LL * fs / ac->sample_rate;
    }
    if (fm <= 0) fm = 21;
    for (int i = 0; i < seg_count_; ++i) {
      if (!seg_has_audio_[i] && seg_dur_ms_[i] > 0)
        audio_total_frames_ += (int)(seg_dur_ms_[i] / fm);
    }
  }

  total_frame_count_ = (video_encode_.is_open() ? video_total_frames_ : 0) +
                       (audio_encode_.is_open() ? audio_total_frames_ : 0);
  if (total_frame_count_ <= 0) total_frame_count_ = 1;  // 防止除零

  // 打开第0段(复用之前Open的解封装/解码器会被重开)并启动流水线
  if (!OpenSegment(0)) return;
  XThread::Start();
  StartPipeline();
  // 第0段无音频时先补静音帧, 保证音频轨从0开始连续
  FeedSilentAudio();
}

void XConvertor::MainConcat() {
  while (!is_exit_) {
    if (is_pause()) {
      MSleep(1);
      continue;
    }

    if (!end_of_decode_ && end_of_file_) {
      video_decode_.Exit();
      audio_decode_.Exit();
      end_of_decode_ = true;
    }

    if (end_of_decode_ &&
        (!video_decode_.is_open() || video_decode_.EndOfDecode()) &&
        (!audio_decode_.is_open() || audio_decode_.EndOfDecode())) {
      if (!last_seg_) {
        // ---- 段边界: 各解码器已排空, 算本段实际时长并切到下一段 ----
        long long dur = 0;
        if (video_decode_.is_open()) dur = video_decode_.cur_ms();
        if (audio_decode_.is_open())
          dur = audio_decode_.cur_ms() > dur ? audio_decode_.cur_ms() : dur;
        long long ts = cur_seg_ < (int)seg_trims_.size()
                           ? seg_trims_[cur_seg_].first
                           : 0;
        long long fd = cur_seg_ < (int)seg_frame_dur_ms_.size()
                           ? seg_frame_dur_ms_[cur_seg_]
                           : 40;
        dur -= ts;  // cur_ms是段内绝对位置
        if (dur <= 0 && cur_seg_ < (int)seg_dur_ms_.size())
          dur = seg_dur_ms_[cur_seg_];  // 兜底用探测有效时长
        // 加一帧时长, 避免段尾段首时间戳重叠
        pts_offset_ms_ += dur + fd;
        // 上一段线程已排空退出, join后线程对象才能复用
        demux_.Wait();
        video_decode_.Wait();
        audio_decode_.Wait();
        seg_start_ms_ += seg_dur_ms_[cur_seg_];
        ++cur_seg_;
        last_seg_ = (cur_seg_ == seg_count_ - 1);
        end_of_file_ = end_of_decode_ = false;
        // 编码器复用, 段间空闲时更新偏移/截取(供下一段帧Do()使用)
        video_encode_.set_pts_offset_ms(pts_offset_ms_);
        audio_encode_.set_pts_offset_ms(pts_offset_ms_);
        long long ss = cur_seg_ < (int)seg_trims_.size()
                           ? seg_trims_[cur_seg_].first
                           : 0;
        video_encode_.set_trim_start_ms(ss);
        audio_encode_.set_trim_start_ms(ss);
        if (!OpenSegment(cur_seg_)) {
          finished_ = true;
          break;
        }
        demux_.Start();
        if (video_decode_.is_open()) video_decode_.Start();
        if (audio_decode_.is_open()) audio_decode_.Start();
        // 本段无音频时补静音帧, 覆盖该段有效时长, 使音频轨连续
        FeedSilentAudio();
        continue;
      }
      // 最后一段: 编码器排空收尾
      video_encode_.Exit();
      audio_encode_.Exit();
      end_of_encode_ = true;
    }

    if (!end_of_mux_ && end_of_encode_ &&
        (!video_encode_.is_open() || video_encode_.EndEncode()) &&
        (!audio_encode_.is_open() || audio_encode_.EndEncode())) {
      mux_.Exit();
      end_of_mux_ = true;
    }

    if (end_of_mux_ && mux_.EndOfMux()) {
      if (mux_.has_error()) {
        error_ = mux_.error();
      }
      finished_ = true;
      if (show_progress_) {
        int dp = 0, dc = 0, ec = 0, mp = 0, bn = 4;
        CalcStages(dp, dc, ec, mp, bn);
        if (!mux_.has_error()) {
          dp = dc = ec = mp = 100;
          bn = 4;
        }
        PrintStageLine(dp == 100 ? 100
                                 : (last_percent_ >= 0 ? last_percent_ : 0),
                       dp, dc, ec, mp, bn, true);
        progress_open_ = false;
      }
      break;
    }

    if (show_progress_) {
      int percent = (int)(GetPos() * 100);
      if (percent != last_percent_) {
        int dp = 0, dc = 0, ec = 0, mp = 0, bn = 4;
        CalcStages(dp, dc, ec, mp, bn);
        PrintStageLine(percent, dp, dc, ec, mp, bn, false);
        last_percent_ = percent;
        progress_open_ = true;
      }
    }
    MSleep(1);
  }
}

void XConvertor::Main() {
  if (concat_mode_) {
    MainConcat();
    return;
  }
  while (!is_exit_) {
    if (is_pause()) {
      MSleep(1);
      continue;
    }

    if (!end_of_decode_ && end_of_file_) {
      video_decode_.Exit();
      audio_decode_.Exit();
      end_of_decode_ = true;
    }

    if (!end_of_encode_ && end_of_decode_ &&
        (!video_decode_.is_open() || video_decode_.EndOfDecode()) &&
        (!audio_decode_.is_open() || audio_decode_.EndOfDecode())) {
      video_encode_.Exit();
      audio_encode_.Exit();
      end_of_encode_ = true;
    }

    if (!end_of_mux_ && end_of_encode_ &&
        (!video_encode_.is_open() || video_encode_.EndEncode()) &&
        (!audio_encode_.is_open() || audio_encode_.EndEncode())) {
      mux_.Exit();
      end_of_mux_ = true;
    }

    if (end_of_mux_ && mux_.EndOfMux()) {
      if (mux_.has_error()) {
        error_ = mux_.error();
      }
      finished_ = true;
      if (show_progress_) {
        // 完成: 正常时各阶段为100, 出错时按实际值渲染并换行收尾
        int dp = 0, dc = 0, ec = 0, mp = 0, bn = 4;
        CalcStages(dp, dc, ec, mp, bn);
        if (!mux_.has_error()) {
          dp = dc = ec = mp = 100;
          bn = 4;
        }
        PrintStageLine(dp == 100 ? 100
                                 : (last_percent_ >= 0 ? last_percent_ : 0),
                       dp, dc, ec, mp, bn, true);
        progress_open_ = false;
      }
      break;
    }

    // 控制台进度(节流: 百分比变化才打印, 避免每包flush拖慢性能)
    if (show_progress_) {
      int percent = (int)(GetPos() * 100);
      if (percent != last_percent_) {
        int dp = 0, dc = 0, ec = 0, mp = 0, bn = 4;
        CalcStages(dp, dc, ec, mp, bn);
        PrintStageLine(percent, dp, dc, ec, mp, bn, false);
        last_percent_ = percent;
        progress_open_ = true;
      }
    }
    MSleep(1);
  }
}

std::string XConvertor::DumpInfo() {
  std::string s;
  char buf[256];
  auto vc = demux_.CopyVideoPara();
  auto ac = demux_.CopyAudioPara();
  s += "==== 转码信息 ====\n";
  if (vc) {
    snprintf(buf, sizeof(buf), "输入视频: %s %dx%d %.2ffps 时长%lldms\n",
             avcodec_get_name(vc->para->codec_id), vc->para->width,
             vc->para->height,
             vc->para->framerate.den
                 ? (double)vc->para->framerate.num / vc->para->framerate.den
                 : 0,
             (long long)vc->total_ms);
    s += buf;
  }
  if (ac) {
    snprintf(buf, sizeof(buf), "输入音频: %s %dHz %d声道 时长%lldms\n",
             avcodec_get_name(ac->para->codec_id), ac->para->sample_rate,
             ac->para->ch_layout.nb_channels, (long long)ac->total_ms);
    s += buf;
  }
  if (video_decode_.is_open()) {
    snprintf(buf, sizeof(buf), "视频解码: %s 收包%d 出帧%d 硬件=%s\n",
             video_decode_.decoder_name(), video_decode_.recv_packet_count(),
             video_decode_.get_Current_decode_frame_count(),
             video_decode_.gpu_used() ? "是" : "否");
    s += buf;
  }
  if (audio_decode_.is_open()) {
    snprintf(buf, sizeof(buf), "音频解码: %s 收包%d 出帧%d 硬件=%s\n",
             audio_decode_.decoder_name(), audio_decode_.recv_packet_count(),
             audio_decode_.get_Current_decode_frame_count(),
             audio_decode_.gpu_used() ? "是" : "否");
    s += buf;
  }
  if (video_encode_.is_open()) {
    snprintf(buf, sizeof(buf), "视频编码: %s 入帧%d 硬件=%s\n",
             video_encode_.encoder_name(), video_encode_.frame_count(),
             video_encode_.gpu_used() ? "是" : "否");
    s += buf;
  }
  if (audio_encode_.is_open()) {
    snprintf(buf, sizeof(buf), "音频编码: %s 入帧%d\n",
             audio_encode_.encoder_name(), audio_encode_.frame_count());
    s += buf;
  }
  snprintf(buf, sizeof(buf), "解封装读取: %d 包, 封装写入: %d 包\n",
           demux_.read_packet_count(), mux_.packet_count());
  s += buf;
  if (start_time_ms_ > 0) {
    snprintf(buf, sizeof(buf), "耗时: %lld ms\n",
             (long long)(NowMs() - start_time_ms_));
    s += buf;
  }
  if (!error_.empty()) {
    s += "错误: " + error_ + "\n";
  }
  return s;
}
