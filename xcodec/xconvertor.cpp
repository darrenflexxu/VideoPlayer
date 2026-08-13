#include "predefine_header.h"

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
  if (total_frame_count_ == 0) {
    total_frame_count_ = GetVideoCodec()->frame_count +
                         (GetAudioCodec()  ? GetAudioCodec()->frame_count : 0);
  }
  return float(mux_.video_packet_count()) /
     float (total_frame_count_);
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
  if (audio_decode_.is_open())
    audio_decode_.Do(pkt);
  if (video_decode_.is_open())
    video_decode_.Do(pkt);

  if (pkt && pkt->stream_index == video_decode_.stream_index() &&
      pkt->buf == nullptr) {
    demux_.Exit();
    end_of_file_ = true;
  }
}

void XConvertor::Start(const char* url,
                       AVCodecParameters* video_para,
                       AVRational* video_time_base,
                       AVCodecParameters* audio_para,
                       AVRational* audio_time_base,
                       const std::map<std::string, std::string>& video_opts,
                       const std::map<std::string, std::string>& audio_opts) {
  AVCodecParameters *tmp_video_para = nullptr;
  if (video_para) {
    video_encode_.set_gpu_encode(gpu_encode_);
    video_encode_.Open(video_para, video_opts);
    video_encode_.GetCodecContext()->refs = 4;
    video_encode_.GetCodecContext()->gop_size = 2;
    video_encode_.GetCodecContext()->max_b_frames = 0;
    video_encode_.GetCodecContext()->profile = AV_PROFILE_H264_HIGH;
    video_encode_.GetCodecContext()->flags = AV_CODEC_FLAG_QSCALE;
    video_encode_.GetCodecContext()->global_quality = 0;
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "preset", "veryslow",
               0);  // "best"或"veryslow"更高质量
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "profile", "high",
               0);  // 尽量不用baseline
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "look_ahead", "1",
               0);  // 启用lookahead提升质量
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "crf", "16", 0);
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "rc-lookahead", "60",
               0);
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "tune", "zerolatency",
               0);
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "async_depth", "1",
               0);
    av_opt_set(video_encode_.GetCodecContext()->priv_data, "x264-params",
               "aq-mode=3:aq-strength=0.9:psy-rd=0.9,0.05:me=umh:subme=10:"
               "trellis=2",
               0);
    video_encode_.set_stream_index(0);
    tmp_video_para = avcodec_parameters_alloc();
    avcodec_parameters_from_context(tmp_video_para,
                                    video_encode_.GetCodecContext());
  }

  if (audio_para) {
    audio_encode_.Open(audio_para, audio_opts);
    audio_encode_.set_stream_index(1);
  }
  mux_.ignoreMaxPkts(true);
  mux_.Open(url, tmp_video_para, video_time_base, audio_para,
            audio_time_base);

  if (tmp_video_para) {
    avcodec_parameters_free(&tmp_video_para);
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

    if (!end_of_encode_ && end_of_decode_ && video_decode_.EndOfDecode() &&
        (!audio_decode_.is_open() || audio_decode_.EndOfDecode())) {
      video_encode_.Exit();
      audio_encode_.Exit();
      end_of_encode_ = true;
    }

    if (!end_of_mux_ && end_of_encode_ && video_encode_.EndEncode() &&
        (!audio_encode_.is_open() || audio_encode_.EndEncode())) {
      mux_.Exit();
      end_of_mux_ = true;
    }

    if (end_of_mux_ && mux_.EndOfMux()) {
      break;
    }
    MSleep(1);
  }
}
