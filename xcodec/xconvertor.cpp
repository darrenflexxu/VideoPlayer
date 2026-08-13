#include "predefine_header.h"

#include <cstdio>

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
  end_of_file_ = end_of_decode_ = end_of_encode_ = end_of_mux_ = false;
  start_time_ms_ = NowMs();
  last_percent_ = -1;
  progress_open_ = false;

  AVCodecParameters *tmp_video_para = nullptr;
  AVCodecParameters *tmp_audio_para = nullptr;
  AVRational enc_video_time_base = {1, 1000};
  AVRational enc_audio_time_base = {1, 1000};

  if (video_para) {
    // 编码器按源时间基数接收帧pts, 保证编码后包的时间基数与源一致
    video_encode_.set_gpu_encode(gpu_encode_);
    video_encode_.set_time_base(video_time_base);
    video_encode_.Open(video_para, video_opts);
    if (!video_encode_.is_open()) {
      error_ = "打开视频编码器失败(编解码器不支持?)";
      return;
    }
    auto c = video_encode_.GetCodecContext();
    c->refs = 4;
    c->gop_size = 2;
    c->max_b_frames = 0;
    enc_video_time_base = c->time_base;  // 以avcodec_open2后的实际为准
    video_encode_.set_stream_index(0);
    tmp_video_para = avcodec_parameters_alloc();
    avcodec_parameters_from_context(tmp_video_para, c);
  }

  if (audio_para) {
    audio_encode_.set_gpu_encode(false);  // 音频不做硬件编码
    audio_encode_.set_time_base(audio_time_base);
    audio_encode_.Open(audio_para, audio_opts);
    if (!audio_encode_.is_open()) {
      error_ = "打开音频编码器失败(编解码器不支持?)";
      if (tmp_video_para) avcodec_parameters_free(&tmp_video_para);
      return;
    }
    enc_audio_time_base = audio_encode_.GetCodecContext()->time_base;
    // 纯音频时音频流索引为0, 有视频时为1
    audio_encode_.set_stream_index(video_encode_.is_open() ? 1 : 0);
    tmp_audio_para = avcodec_parameters_alloc();
    avcodec_parameters_from_context(
        tmp_audio_para, audio_encode_.GetCodecContext());
  }

  mux_.ignoreMaxPkts(true);
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
  {
    auto vc = demux_.CopyVideoPara();
    auto ac = demux_.CopyAudioPara();
    if (vc) {
      if (vc->frame_count > 0) {
        total_frame_count_ += vc->frame_count;
      } else if (vc->para->framerate.num > 0) {
        total_frame_count_ +=
            vc->total_ms * vc->para->framerate.num / vc->para->framerate.den /
            1000;
      }
    }
    if (ac) {
      int frame_size =
          ac->para->frame_size > 0 ? ac->para->frame_size : 1024;
      if (ac->frame_count > 0) {
        total_frame_count_ += ac->frame_count;
      } else if (ac->para->sample_rate > 0) {
        total_frame_count_ += ac->total_ms * ac->para->sample_rate / 1000 /
                              frame_size;
      }
    }
    if (total_frame_count_ <= 0) {
      total_frame_count_ = 1;  // 防止除零
    }
  }

  XThread::Start();
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

void XConvertor::Main() {
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
        // 完成: 正常时置100%, 出错时保留当前值并换行收尾
        printf("\r进度 %d%%\n",
               mux_.has_error() ? (last_percent_ >= 0 ? last_percent_ : 0)
                                : 100);
        fflush(stdout);
        progress_open_ = false;
      }
      break;
    }

    // 控制台进度(节流: 百分比变化才打印, 避免每包flush拖慢性能)
    if (show_progress_) {
      int percent = (int)(GetPos() * 100);
      if (percent != last_percent_) {
        printf("\r进度 %d%%  ", percent);
        fflush(stdout);
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
