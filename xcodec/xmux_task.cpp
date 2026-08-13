#include "predefine_header.h"

using namespace std;

void XMuxTask::Do(AVPacket* pkt) {
  pkts_.Push(pkt);
  if (block_size_ <= 0)
    return;
  while (!is_exit_) {
    if (pkts_.Size() > block_size_) {
      MSleep(1);
      continue;
    }
    break;
  }
}

bool XMuxTask::EndOfMux() {
  return end_mux_;
}

int XMuxTask::packet_count() {
  return packet_count_;
}

void XMuxTask::Main() {
  if (!xmux_.WriteHead()) {
    has_error_ = true;
    error_ = "写输出文件头失败";
    end_mux_ = true;
    xmux_.set_c(nullptr);
    return;
  }

  while (!is_exit_) {
    unique_lock<mutex> lock(mux_);
    auto pkt = pkts_.Pop();
    if (!pkt) {
      MSleep(1);
      continue;
    }
    if (!xmux_.Write(pkt)) {
      has_error_ = true;
      error_ = "写输出文件失败";
      av_packet_free(&pkt);
      break;
    }
    packet_count_ += 1;
    av_packet_free(&pkt);
  }

  if (!has_error_) {
    {
      unique_lock<mutex> lock(mux_);
      while (auto pkt = pkts_.Pop()) {
        if (!xmux_.Write(pkt)) {
          has_error_ = true;
          error_ = "写输出文件失败";
          av_packet_free(&pkt);
          break;
        }
        packet_count_ += 1;
        av_packet_free(&pkt);
      }
    }
    if (!xmux_.WriteEnd()) {
      has_error_ = true;
      error_ = "写输出文件尾失败";
    }
  }
  xmux_.set_c(nullptr);
  end_mux_ = true;
}

bool XMuxTask::Open(const char* url,
                    AVCodecParameters* video_para,
                    AVRational* video_time_base,
                    AVCodecParameters* audio_para,
                    AVRational* audio_time_base) {
  auto c = xmux_.Open(url, video_para, audio_para);
  if (!c)
    return false;
  xmux_.set_c(c);
  xmux_.set_src_video_time_base(video_time_base);
  xmux_.set_src_audio_time_base(audio_time_base);
  return true;
}