#include "xdecode.h"
#include <iostream>
using namespace std;
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
bool XDecode::Send(const AVPacket* pkt)  // 发送解码
{
  unique_lock<mutex> lock(mux_);
  if (!c_)
    return false;
  auto re = avcodec_send_packet(c_, pkt);
  if (re != 0)
    return false;
  return true;
}

bool XDecode::Recv(AVFrame* frame)  // 获取解码
{
  unique_lock<mutex> lock(mux_);
  return RecvFrame(frame);
}

bool XDecode::InitHW() {
  unique_lock<mutex> lock(mux_);
  if (!c_)
    return false;
  ;
  AVBufferRef* ctx = nullptr;  // 硬件加速上下文
  auto re = av_hwdevice_ctx_create(&ctx, AV_HWDEVICE_TYPE_QSV, NULL, NULL, 0);
  if (re != 0) {
    PrintErr(re);
    return false;
  }
  c_->hw_device_ctx = av_buffer_ref(ctx);
  c_->pix_fmt = AV_PIX_FMT_YUV420P;
  cout << "硬件加速：" << endl;
  return true;
}

bool XDecode::RecvFrame(AVFrame* frame) {
  if (!c_)
    return false;
  auto f = frame;
  if (c_->hw_device_ctx && !gpu_direct_render_)  // 硬件加速
  {
    f = av_frame_alloc();
  }
  auto re = avcodec_receive_frame(c_, f);
  if (re == 0) {
    if (c_->hw_device_ctx && !gpu_direct_render_)  // GPU解码
    {
      // 显存转内存 GPU =》 CPU
      re = av_hwframe_transfer_data(frame, f, 0);
      // 创建 swsContext
      struct SwsContext* sws_ctx = sws_getContext(
          frame->width, frame->height, (enum AVPixelFormat)frame->format,
          frame->width, frame->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL,
          NULL, NULL);

      // 准备输出帧
      AVFrame* yuv420p_frame = av_frame_alloc();
      yuv420p_frame->format = AV_PIX_FMT_YUV420P;
      yuv420p_frame->width = frame->width;
      yuv420p_frame->height = frame->height;
      av_frame_get_buffer(yuv420p_frame, 1);  // 分配数据缓冲
      // 格式转换
      sws_scale(sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
                frame->height, yuv420p_frame->data, yuv420p_frame->linesize);
      // 之后 yuv420p_frame 就是你要的 YUV420P（420P）格式了
      av_frame_unref(frame);
      av_frame_move_ref(frame, yuv420p_frame);
      av_frame_free(&yuv420p_frame);
      frame->pts = f->pts;
      av_frame_free(&f);
      if (re != 0) {
        PrintErr(re);
        return false;
      }
    }
    return true;
  }
  if (c_->hw_device_ctx && !gpu_direct_render_)
    av_frame_free(&f);
  return false;
}

std::vector<AVFrame*> XDecode::End()  // 获取缓存
{
  std::vector<AVFrame*> res;
  unique_lock<mutex> lock(mux_);
  if (!c_)
    return res;

  /// 取出缓存数据
  int ret = avcodec_send_packet(c_, NULL);
  while (ret >= 0) {
    auto frame = av_frame_alloc();

    if (!RecvFrame(frame)) {
      av_frame_free(&frame);
      break;
    }
    res.push_back(frame);
  }
  return res;
}