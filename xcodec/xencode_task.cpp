#include "predefine_header.h"

#include <cstdlib>

using namespace std;

namespace {
// 视频格式/尺寸转换的 sws 缓存
class VideoSws {
 public:
  struct SwsContext* Get(int sw,
                         int sh,
                         int sf,
                         int dw,
                         int dh,
                         int df) {
    if (ctx_ && sw == sw_ && sh == sh_ && dw == dw_ && dh == dh_ &&
        sf == sf_ && df == df_) {
      return ctx_;
    }
    if (ctx_) sws_freeContext(ctx_);
    ctx_ = sws_getContext(sw, sh, (enum AVPixelFormat)sf, dw, dh,
                          (enum AVPixelFormat)df, SWS_BICUBIC, nullptr,
                          nullptr, nullptr);
    sw_ = sw;
    sh_ = sh;
    dw_ = dw;
    dh_ = dh;
    sf_ = sf;
    df_ = df;
    return ctx_;
  }
  ~VideoSws() {
    if (ctx_) sws_freeContext(ctx_);
  }

 private:
  struct SwsContext* ctx_ = nullptr;
  int sw_ = 0, sh_ = 0, dw_ = 0, dh_ = 0, sf_ = 0, df_ = 0;
};
}  // namespace

// 视频帧格式/尺寸转换为目标格式, 返回新帧(需 av_frame_free), 失败返回nullptr
static AVFrame* ConvertVideoFrame(AVFrame* in, AVPixelFormat dst_fmt, int dw,
                                  int dh) {
  if (!in || in->width <= 0 || in->height <= 0) return nullptr;
  static VideoSws conv;
  struct SwsContext* sws = conv.Get(in->width, in->height,
                                    (int)(enum AVPixelFormat)in->format, dw, dh,
                                    (int)dst_fmt);
  if (!sws) return nullptr;
  AVFrame* out = av_frame_alloc();
  if (!out) return nullptr;
  out->format = dst_fmt;
  out->width = dw;
  out->height = dh;
  if (av_frame_get_buffer(out, 32) < 0) {
    av_frame_free(&out);
    return nullptr;
  }
  sws_scale(sws, (const uint8_t* const*)in->data, in->linesize, 0, in->height,
            out->data, out->linesize);
  av_frame_copy_props(out, in);
  return out;
}

void XEncodeTask::set_time_base(AVRational* time_base) {
  if (!time_base)
    return;
  unique_lock<mutex> lock(mux_);
  if (time_base_)
    delete time_base_;
  time_base_ = new AVRational();
  time_base_->den = time_base->den;
  time_base_->num = time_base->num;
}

AVCodecContext* XEncodeTask::GetCodecContext() const {
  return encode_.get_codec_context();
}

const char* XEncodeTask::encoder_name() {
  auto c = encode_.get_codec_context();
  return (c && c->codec) ? c->codec->name : "";
}

bool XEncodeTask::EndEncode() {
  return end_encode_;
}

void XEncodeTask::NextPacket(AVPacket* pkt) {
  if (!pkt) {
    return;
  }
  unique_lock<mutex> lock(mux_);
  pkts_cache_.push_back(pkt);
  pkts_cache_.sort([](AVPacket* a, AVPacket* b) { return a->dts < b->dts; });

  if (pkts_cache_.size() < 100) {
    return;
  }

  while (!pkts_cache_.empty()) {
    Next(pkts_cache_.front());
    pkts_cache_.pop_front();
  }
}

/// <summary>
/// 清理缓存
/// </summary>
void XEncodeTask::Clear() {
  unique_lock<mutex> lock(mux_);
  cur_pts_ = -1;
  encode_.Clear();
}
void XEncodeTask::Stop() {
  XThread::Stop();

  unique_lock<mutex> lock(mux_);
  encode_.set_c(nullptr);
  is_open_ = false;
  if (time_base_)
    delete time_base_;
  time_base_ = nullptr;
}
/// <summary>
/// 打开解码器
/// </summary>
bool XEncodeTask::Open(AVCodecParameters* para,
                       const std::map<std::string, std::string>& opts) {
  if (!para) {
    LOGERROR("para is null!");
    return false;
  }
  unique_lock<mutex> lock(mux_);
  is_open_ = false;
  gpu_used_ = false;

  // 尝试用指定模式打开编码器(应用选项中设置必须在avcodec_open2之前)
  auto open_with = [&](bool gpu) -> bool {
    auto c = encode_.Create(para->codec_id, true, gpu);
    if (!c) {
      LOGERROR("encode_.Create failed!");
      return false;
    }
    avcodec_parameters_to_context(c, para);
    if (time_base_ && (time_base_->num > 0 && time_base_->den > 0)) {
      // 编码器按源时间基数接收帧pts, 保证编码后包的时间基数与源一致
      c->time_base = *time_base_;
    }
    // 把SPS/PPS等参数放到extradata(avcC), 输出mp4等容器要求
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    // 清掉源文件的codec_tag(如avc1), 让封装器按输出编码器重新分配
    c->codec_tag = 0;
    // 像素格式/采样格式以编码器的默认支持为准
    if (c->codec_type == AVMEDIA_TYPE_VIDEO) {
      if (gpu) {
        c->pix_fmt = AV_PIX_FMT_NV12;
      } else if (c->codec && c->codec->pix_fmts &&
                 c->codec->pix_fmts[0] != AV_PIX_FMT_NONE) {
        c->pix_fmt = c->codec->pix_fmts[0];
      } else {
        c->pix_fmt = AV_PIX_FMT_YUV420P;
      }
    } else if (c->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (c->codec && c->codec->sample_fmts &&
          c->codec->sample_fmts[0] != AV_SAMPLE_FMT_NONE) {
        c->sample_fmt = c->codec->sample_fmts[0];
      } else {
        c->sample_fmt = AV_SAMPLE_FMT_FLTP;
      }
    }
    encode_.set_c(c);
    for (auto& kv : opts) {
      if (kv.first == "bit_rate") {
        c->bit_rate = atoll(kv.second.c_str());
        continue;
      }
      auto re = av_opt_set(c->priv_data, kv.first.c_str(), kv.second.c_str(), 0);
      if (re != 0) {
        cerr << "set encoder opt[" << kv.first << "] failed!" << endl;
      }
    }
    // 不做额外探针: 实测向h264_qsv等send(NULL)冲刷后会破坏后续编码输出,
    // 且本机QSV打开即可用(失败会走上面的软件回退), 运行时失败由Do()计数日志暴露
    return encode_.Open();
  };

  if (open_with(gpu_encode_)) {
    gpu_used_ = gpu_encode_;
  } else if (gpu_encode_) {
    // GPU 编码失败(如无QSV设备), 回退软件编码
    LOGERROR("gpu encode open failed! fallback to software");
    if (!open_with(false)) {
      LOGERROR("encode_.Open() failed!");
      return false;
    }
  } else {
    LOGERROR("encode_.Open() failed!");
    return false;
  }

  LOGINFO("Open encode success!");
  is_open_ = true;
  return true;
}

// 责任链处理函数
void XEncodeTask::Do(AVFrame* frame) {
  if (!frame) return;
  unique_lock<mutex> lock(mux_);
  auto c = encode_.get_codec_context();
  if (!c) {
    av_frame_free(&frame);
    return;
  }

  if (c->codec_type == AVMEDIA_TYPE_AUDIO) {
    // 音频: 重采样到编码器期望的采样格式/采样率/声道布局
    if (!resample_) resample_.reset(new XResample());
    if (resample_->Need(frame, c)) resample_->Create(frame, c);
    if (resample_->is_active()) {
      auto out = resample_->Convert(frame);
      av_frame_free(&frame);
      if (!out) return;
      frame = out;
    }
  } else if (c->codec_type == AVMEDIA_TYPE_VIDEO) {
    // 视频: 缩放/格式转换到编码器期望的分辨率和像素格式
    // 用实际使用的gpu_used_(而非请求的gpu_encode_), 避免GPU失败回退软件后
    // 仍给libx264/libx265喂NV12导致送帧全部失败
    AVPixelFormat dst_fmt = gpu_used_ ? AV_PIX_FMT_NV12 : c->pix_fmt;
    if (dst_fmt == AV_PIX_FMT_NONE) dst_fmt = AV_PIX_FMT_YUV420P;
    int dw = c->width > 0 ? c->width : frame->width;
    int dh = c->height > 0 ? c->height : frame->height;
    if (frame->width != dw || frame->height != dh ||
        (enum AVPixelFormat)frame->format != dst_fmt) {
      auto out = ConvertVideoFrame(frame, dst_fmt, dw, dh);
      av_frame_free(&frame);
      if (!out) return;
      frame = out;
    }
  }

  if (encode_.Send(frame)) {
    encode_fail_count_ = 0;
  } else {
    // 送帧失败(硬件编码器可能运行中失效), 记录并继续, 避免静默丢帧
    encode_fail_count_ += 1;
    if (encode_fail_count_ == 1 || encode_fail_count_ % 60 == 0) {
      cerr << "encode Send failed! count=" << encode_fail_count_ << endl;
    }
  }
  frame_count_ += 1;
}

// 线程主函数
void XEncodeTask::Main() {
  while (!is_exit_) {
    if (is_pause())  // 暂停
    {
      MSleep(1);
      continue;
    }    
    // 发送到解码线程
    auto pkg = av_packet_alloc();
    auto ret = encode_.Recv(pkg);

    if (!ret) {
      av_packet_free(&pkg);
      MSleep(1);
      continue;
    }
    cout << "E" << flush;
    pkg->stream_index = stream_index_;
    NextPacket(pkg);
    MSleep(1);
  }

  do {
    auto pkg = av_packet_alloc();
    auto ret = encode_.Recv(pkg);

    if (!ret) {
      av_packet_free(&pkg);
      break;
    }
    cout << "E" << flush;
    pkg->stream_index = stream_index_;
    NextPacket(pkg);
  } while (!is_exit_);
  auto pkgs = encode_.End();

  for (auto pkg : pkgs) {
    cout << "E" << flush;
    pkg->stream_index = stream_index_;
    NextPacket(pkg);
  }

  while (!pkts_cache_.empty()) {
    Next(pkts_cache_.front());
    pkts_cache_.pop_front();
  }
  end_encode_ = true;
  cout << endl << "encode frame count(" << frame_count_ << ")" << endl << flush;
}
