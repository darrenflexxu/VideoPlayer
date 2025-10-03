#include "xconvertor.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

// 暂停或者播放
void XConvertor::Pause(bool is_pause) {
  XThread::Pause(is_pause);
  demux_.Pause(is_pause);
  audio_decode_.Pause(is_pause);
  video_decode_.Pause(is_pause);
  mux_.Pause(is_pause);
}

void XConvertor::Stop() {
  Exit();
  demux_.Exit();
  audio_decode_.Exit();
  video_decode_.Exit();
  mux_.Exit();
  Wait();
  demux_.Wait();
  audio_decode_.Wait();
  video_decode_.Wait();
  mux_.Wait();
}
bool XConvertor::Open(const char* url, void* winid) {
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
    // 缓冲
    video_decode_.set_block_size(100);
  }

  auto ap = demux_.CopyAudioPara();
  if (ap) {
    // 音频解码
    if (!audio_decode_.Open(ap->para)) {
      return false;
    }
    // 缓冲
    audio_decode_.set_block_size(100);

    // 用于过滤视频数据
    audio_decode_.set_stream_index(demux_.audio_index());

    // frame 缓冲
    audio_decode_.set_frame_cache(true);

    // 设置时间基数
    double time_base = 0;
  } else {
    demux_.set_syn_type(XSYN_VIDEO);  // 根据视频同步
  }

  // 解封装数据传到当前类
  demux_.set_next(this);
  return true;
}

void XConvertor::Do(AVPacket* pkt) {
  if (audio_decode_.is_open())
    audio_decode_.Do(pkt);
  if (video_decode_.is_open())
    video_decode_.Do(pkt);
}
void XConvertor::Start(const char* url,
                       AVCodecParameters* video_para,
                       AVRational* video_time_base,
                       AVCodecParameters* audio_para,
                       AVRational* audio_time_base) {
  if (video_para) {
    video_encode_.set_c(
        XCodec::Create(video_para->codec_id, true, gpu_encode_));
  }

  if (audio_para) {
    audio_encode_.set_c(XCodec::Create(audio_para->codec_id, true, false));
  }
  mux_.Open(url, video_para, video_time_base, audio_para, audio_time_base);
  demux_.Start();
  if (video_decode_.is_open())
    video_decode_.Start();
  if (audio_decode_.is_open())
    audio_decode_.Start();
  mux_.Start();
  XThread::Start();
}

bool XConvertor::IsDecodeFinish() {
  return video_decode_.IsVideoFinish();
}

bool XConvertor::IsFinish() {
  return IsDecodeFinish() && mux_.IsEmptyPacket();
}

// 渲染视频 播放音频
void XConvertor::Update() {
  // 渲染视频
  auto vf = video_decode_.GetFrame();
  if (vf) {
    auto packet = video_encode_.Encode(vf);

    if (packet) {
      mux_.Do(packet);
    }
    XFreeFrame(&vf);
  } else if (IsDecodeFinish()) {
    auto packets = video_encode_.End();

    for (auto packet : packets) {
      mux_.Do(packet);
    }
  }
  // 音频播放
  auto af = audio_decode_.GetFrame();
  if (af) {
    auto packet = audio_encode_.Encode(vf);

    if (packet) {
      mux_.Do(packet);
    }
    XFreeFrame(&af);
  } else {
    auto packets = audio_encode_.End();

    for (auto packet : packets) {
      mux_.Do(packet);
    }
  }
}

void XConvertor::Main() {
  long long syn = 0;
  auto ap = demux_.CopyAudioPara();
  auto vp = demux_.CopyVideoPara();
  video_decode_.set_time_base(vp->time_base);
  while (!is_exit_) {
    if (is_pause()) {
      MSleep(1);
      continue;
    }
    this->pos_ms_ = video_decode_.cur_ms();

    if (ap) {
      syn = audio_decode_.cur_ms();
      audio_decode_.set_syn_pts(audio_decode_.cur_ms() + 10000);
      video_decode_.set_syn_pts(syn);
    }
    MSleep(1);
  }
}
