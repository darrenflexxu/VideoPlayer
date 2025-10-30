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
    // 视频总时长
    this->total_ms_ = vp->total_ms;
    // 视频总时长
    video_decode_.set_gpu_decode(gpu_decode_);

    if (!video_decode_.Open(vp->para)) {
      return false;
    }
    // 用于过滤音频包
    video_decode_.set_stream_index(demux_.video_index());
  }

  auto ap = demux_.CopyAudioPara();
  if (ap) {
    // 音频解码
    if (!audio_decode_.Open(ap->para)) {
      return false;
    }
    // 用于过滤视频数据
    audio_decode_.set_stream_index(demux_.audio_index());
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
  if (video_para) {
    video_encode_.set_gpu_encode(gpu_encode_);
    video_encode_.Open(video_para, video_opts);
    video_encode_.set_stream_index(0);
  }

  if (audio_para) {
    audio_encode_.Open(audio_para, audio_opts);
    audio_encode_.set_stream_index(1);
  }
  mux_.Open(url, video_para, video_time_base, audio_para, audio_time_base);
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

    if (!end_of_decode_ && end_of_file_ && video_decode_.IsVideoFinish()) {
      video_decode_.Exit();
      audio_decode_.Exit();
      end_of_decode_ = true;
    }

    if (!end_of_encode_ && end_of_decode_) {
      video_encode_.Exit();
      audio_encode_.Exit();
      end_of_encode_ = true;
    }

    if (end_of_encode_ && mux_.IsEmptyPacket()) {
      mux_.Exit();
      break;
    }
    MSleep(1);
  }
}
