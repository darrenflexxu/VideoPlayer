#include "xshader.h"
#include <Windows.h>
#include <string>
extern "C"
{
#include <libavcodec/avcodec.h>
}

XShader::XShader() {
}

void XShader::Close() {
  ctx_.reset();
}

bool XShader::Init(int w, int h, Format fmt) {
  HWND hwnd = (HWND)win_id_;
  auto dc = GetDC(hwnd);
  RECT rect;
  GetClientRect(hwnd, &rect);
  ctx_ = std::make_shared<GLContext>();
  ctx_->setup(hwnd, dc);
  width_ = rect.right - rect.left;
  height_ = rect.bottom - rect.top;
  pix_w_ = w;
  pix_h_ = h;
  return true;
}

bool XShader::Draw(const unsigned char* data, int linesize) {
  return Draw(data, linesize, data + linesize * pix_h_, linesize / 4, data + linesize * pix_h_ + linesize * pix_h_ / 4 , linesize / 4);
}

bool XShader::Draw(
  const unsigned  char* y, int y_pitch,
  const unsigned  char* u, int u_pitch,
  const unsigned  char* v, int v_pitch
) {
  HWND hwnd = (HWND)win_id_;
  RECT rect;
  GetClientRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;

  if (width != width_ || height != height_) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
  }
  yv12_->Draw(y, y_pitch, u, u_pitch, v, v_pitch); 
  // Show  
  if (ctx_) {
    ctx_->swapBuffer();
  }  
  return true;
}

bool XShader::DrawFrame(AVFrame* frame, AVCodecContext* ctx) {
  if (!frame || !frame->data[0])return XVideoView::DrawFrame(frame, ctx);
  count_++;
  if (beg_ms_ <= 0) {
    beg_ms_ = clock();
  }
  //计算显示帧率
  else if ((clock() - beg_ms_) / (CLOCKS_PER_SEC / 1000) >= 1000) //一秒计算一次fps
  {
    render_fps_ = count_;
    count_ = 0;
    beg_ms_ = clock();
  }
  HWND hwnd = (HWND)win_id_;
  RECT rect;
  GetClientRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;

  if (width != width_ || height != height_) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
  }

  int linesize = 0;
  switch (frame->format) {
  case AV_PIX_FMT_YUV420P:
  case AV_PIX_FMT_YUVJ420P:
  {
    if (!yv12_) {
      yv12_ = std::make_shared<XShaderYV12>(pix_w_, pix_h_);
    }
    yv12_->Draw(frame->data[0], frame->linesize[0],//Y
      frame->data[1], frame->linesize[1],   //U
      frame->data[2], frame->linesize[2]    //V
    );
    break;
  }
  case AV_PIX_FMT_NV12:
  {
    if (!nv12_) {
      nv12_ = std::make_shared<XShaderNV12>(pix_w_, pix_h_);
    }
    nv12_->Draw(frame->data[0], frame->linesize[0], //Y
      frame->data[1], frame->linesize[1] //UV
    );
    break;
  }
  case AV_PIX_FMT_BGRA:
  case AV_PIX_FMT_ARGB:
  case AV_PIX_FMT_RGBA:
    return Draw(frame->data[0], frame->linesize[0]);
  default:
    break;
  }

  // Show  
  if (ctx_) {
    ctx_->swapBuffer();
  }
  return true;
}

bool XShader::IsExit() {
  return false;
}
