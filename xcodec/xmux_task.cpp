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

int XMuxTask::video_packet_count() {
  return video_packet_count_;
}

void XMuxTask::Main() {
  xmux_.WriteHead();

  while (!is_exit_) {
    unique_lock<mutex> lock(mux_);
    auto pkt = pkts_.Pop();
    if (!pkt) {
      MSleep(1);
      continue;
    }
    xmux_.Write(pkt);

    if (pkt->stream_index == xmux_.video_index()) {
      video_packet_count_ += 1;
    }
    cout << "W" << flush;
    av_packet_free(&pkt);
  }
  
  {
    unique_lock<mutex> lock(mux_);
    while (auto pkt = pkts_.Pop()) {
      xmux_.Write(pkt);

      if (pkt->stream_index == xmux_.video_index()) {
        video_packet_count_ += 1;
      }
      cout << "W" << flush;
      av_packet_free(&pkt);
    }
  }
  xmux_.WriteEnd();
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