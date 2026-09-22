#include "predefine_header.h"

#ifdef WIN32
#include <windows.h>
#else
#include <SDL.h>
#endif

extern "C" SDL_Window* SDL_CreateWindowFromNSView(void* nsView);

XShader::XShader() {
}

void XShader::Close() {
  ctx_.reset();
#ifndef WIN32
  if (sdl_window_) {
    SDL_DestroyWindow(sdl_window_);
    sdl_window_ = nullptr;
  }
#endif
}

bool XShader::Init(int w, int h, Format fmt) {
#ifdef WIN32
  HWND hwnd = (HWND)win_id_;
  auto dc = GetDC(hwnd);
  RECT rect;
  GetClientRect(hwnd, &rect);
  ctx_ = std::make_shared<GLContext>();
  ctx_->setup(hwnd, dc);
  width_ = rect.right - rect.left;
  height_ = rect.bottom - rect.top;
#else
  SDL_Window* window = nullptr;
  if (win_id_) {
#if defined(__APPLE__)
    window = SDL_CreateWindowFromNSView(win_id_);
    if (!window) {
      printf("XShader: Failed to create SDL window from NSView (win_id_=%p)\n", win_id_);
    }
#else
    window = SDL_CreateWindowFrom(win_id_);
#endif
  } else {
    // Create standalone window
    window = SDL_CreateWindow("VideoPlayer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (window) {
      SDL_ShowWindow(window);
    }
  }
  if (!window) {
    printf("XShader: No window created, win_id_=%p\n", win_id_);
    return false;
  }
  printf("XShader: SDL window created: %p\n", window);
  ctx_ = std::make_shared<GLContext>();
  if (!ctx_->setup(window)) {
    printf("XShader: GLContext setup failed\n");
    SDL_DestroyWindow(window);
    return false;
  }
  printf("XShader: GLContext setup success\n");
  int w_w, w_h;
  SDL_GetWindowSize(window, &w_w, &w_h);
  printf("XShader: Window size: %dx%d\n", w_w, w_h);
  width_ = w_w;
  height_ = w_h;
  // Store window for later use
  sdl_window_ = window;
#endif
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
#ifdef WIN32
  HWND hwnd = (HWND)win_id_;
  RECT rect;
  GetClientRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
#else
  if (!sdl_window_) return false;
  int width, height;
  SDL_GetWindowSize(sdl_window_, &width, &height);
#endif

  if (width != width_ || height != height_) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
  }
  glClear(GL_COLOR_BUFFER_BIT);   //着色器不再自行清屏, 独立窗口路径在此统一清屏
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
#ifdef WIN32
  HWND hwnd = (HWND)win_id_;
  RECT rect;
  GetClientRect(hwnd, &rect);
  int width = rect.right - rect.left;
  int height = rect.bottom - rect.top;
#else
  if (!sdl_window_) return false;
  int width, height;
  SDL_GetWindowSize(sdl_window_, &width, &height);
#endif

  if (width != width_ || height != height_) {
    width_ = width;
    height_ = height;
    glViewport(0, 0, width, height);
  }

  int linesize = 0;
  glClear(GL_COLOR_BUFFER_BIT);   //着色器不再自行清屏, 此处统一清屏
  switch (frame->format) {
  case AV_PIX_FMT_YUV420P:
  case AV_PIX_FMT_YUVJ420P:
  {
    if (!yv12_) {
      auto rotate = XTools::get_rotation_from_frame(frame);
      yv12_ = std::make_shared<XShaderYV12>(pix_w_, pix_h_, rotate);
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
