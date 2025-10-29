#pragma once

#include "xtools.h"

class XCODEC_API XVideoFilterProc {
 public:
   enum FilterType {
     kFaceDect = 0
  };
  struct Context;

  static XVideoFilterProc* GetInstance();

  bool Action(AVFrame* video_frame, const std::vector<FilterType>& types = {});
  void Clear();

 private:
  XVideoFilterProc();

  std::shared_ptr<Context> ctx_;
};