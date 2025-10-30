#include "predefine_header.h"

using namespace std;
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
  auto c = encode_.Create(para->codec_id, true, gpu_encode_);
  if (!c) {
    LOGERROR("encode_.Create failed!");
    return false;
  }
  // 复制视频参数
  avcodec_parameters_to_context(c, para);
  encode_.set_c(c);

  for (const auto& opt : opts) {
    encode_.SetOpt(opt.first.c_str(), opt.second.c_str());
  }

  if (gpu_encode_) {
    encode_.get_codec_context()->pix_fmt = AV_PIX_FMT_NV12;
  }

  if (!encode_.Open()) {
    LOGERROR("encode_.Open() failed!");
    return false;
  }
  LOGINFO("Open encode success!");
  is_open_ = true;
  return true;
}

namespace {
class MyDecoder {
 public:
  SwsContext* get_or_create_sws(int srcW,
                                int srcH,
                                AVPixelFormat srcFmt,
                                int dstW,
                                int dstH,
                                AVPixelFormat dstFmt) {
    if (sws_ctx_ && srcW == srcW_ && srcH == srcH_ && dstW == dstW_ &&
        dstH == dstH_ && srcFmt == srcFmt_ && dstFmt == dstFmt_) {
      return sws_ctx_;
    }
    if (sws_ctx_)
      sws_freeContext(sws_ctx_);
    sws_ctx_ = sws_getContext(srcW, srcH, srcFmt, dstW, dstH, dstFmt,
                              SWS_BICUBIC, nullptr, nullptr, nullptr);
    srcW_ = srcW;
    srcH_ = srcH;
    dstW_ = dstW;
    dstH_ = dstH;
    srcFmt_ = srcFmt;
    dstFmt_ = dstFmt;
    return sws_ctx_;
  }
  ~MyDecoder() {
    if (sws_ctx_)
      sws_freeContext(sws_ctx_);
  }

 private:
  SwsContext* sws_ctx_ = nullptr;
  int srcW_ = 0, srcH_ = 0, dstW_ = 0, dstH_ = 0;
  AVPixelFormat srcFmt_ = AV_PIX_FMT_NONE, dstFmt_ = AV_PIX_FMT_NONE;
};
}  // namespace

// 责任链处理函数
void XEncodeTask::Do(AVFrame* frame) {
  unique_lock<mutex> lock(mux_);

  if (gpu_encode_) {
    static MyDecoder decoder;
    struct SwsContext* sws_ctx = decoder.get_or_create_sws(
        frame->width, frame->height, (enum AVPixelFormat)frame->format,
        frame->width, frame->height, AV_PIX_FMT_NV12);
    // 准备输出帧
    AVFrame* nv12_frame = av_frame_alloc();
    nv12_frame->format = AV_PIX_FMT_NV12;
    nv12_frame->width = frame->width;
    nv12_frame->height = frame->height;
    av_frame_get_buffer(nv12_frame, 1);  // 分配数据缓冲
    // 格式转换
    sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
              frame->height, nv12_frame->data, nv12_frame->linesize);
    av_frame_copy_props(nv12_frame, frame);
    av_frame_replace(frame, nv12_frame);
    av_frame_free(&nv12_frame);
  }
  encode_.Send(frame);
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
    cout << "E" << flush;
    auto pkg = av_packet_alloc();
    auto ret = encode_.Recv(pkg);

    if (!ret) {
      av_packet_free(&pkg);
      MSleep(1);
      continue;
    }
    pkg->stream_index = stream_index_;
    Next(pkg);
    MSleep(1);
  }
  auto pkgs = encode_.End();

  for (auto pkg : pkgs) {
    pkg->stream_index = stream_index_;
    Next(pkg);
  }
}
