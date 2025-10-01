#pragma once

#include "xtools.h"

class XCODEC_API XVideoFilterProc {
 public:
  struct Context;

  static XVideoFilterProc* GetInstance();

  bool FaceDetect(AVFrame* video_frame);

 private:
  XVideoFilterProc();

  std::shared_ptr<Context> ctx_;
};