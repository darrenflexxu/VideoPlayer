#include "predefine_header.h"

namespace {
cv::Mat AVFrameToCVMat(AVFrame* yuv420Frame) {
  // 得到AVFrame信息
  int srcW = yuv420Frame->width;
  int srcH = yuv420Frame->height;
  SwsContext* swsCtx = sws_getContext(
      srcW, srcH, (AVPixelFormat)yuv420Frame->format, srcW, srcH,
      (AVPixelFormat)AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

  // 生成Mat对象
  cv::Mat mat;
  mat.create(cv::Size(srcW, srcH), CV_8UC3);

  // 格式转换，直接填充Mat的数据data
  AVFrame* bgr24Frame = av_frame_alloc();
  av_image_fill_arrays(bgr24Frame->data, bgr24Frame->linesize,
                       (uint8_t*)mat.data, (AVPixelFormat)AV_PIX_FMT_BGR24,
                       srcW, srcH, 1);
  sws_scale(swsCtx, (const uint8_t* const*)yuv420Frame->data,
            yuv420Frame->linesize, 0, srcH, bgr24Frame->data,
            bgr24Frame->linesize);

  // 释放
  av_frame_free(&bgr24Frame);
  sws_freeContext(swsCtx);
  return mat;
}

bool CVMatToAVFrame(cv::Mat& inMat, AVFrame* frame) {
  if (frame == nullptr) {
    return false;
  }
  av_frame_unref(frame);
  // 得到Mat信息
  AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
  int width = inMat.cols;
  int height = inMat.rows;

  // 创建AVFrame填充参数 注：调用者释放该frame
  frame->width = width;
  frame->height = height;
  frame->format = dstFormat;

  // 初始化AVFrame内部空间
  int ret = av_frame_get_buffer(frame, 32);
  if (ret < 0) {
    return false;
  }
  ret = av_frame_make_writable(frame);
  if (ret < 0) {
    return false;
  }
  // 转换颜色空间为YUV420
  cv::cvtColor(inMat, inMat, cv::COLOR_BGR2YUV_I420);
  // 按YUV420格式，设置数据地址
  int frame_size = width * height;
  unsigned char* data = inMat.data;
  memcpy(frame->data[0], data, frame_size);
  memcpy(frame->data[1], data + frame_size, frame_size / 4);
  memcpy(frame->data[2], data + frame_size * 5 / 4, frame_size / 4);
  return true;
}

cv::UMat AVFrameToCVUMat(AVFrame* yuv420Frame) {
  int srcW = yuv420Frame->width;
  int srcH = yuv420Frame->height;
  SwsContext* swsCtx = sws_getContext(
      srcW, srcH, (AVPixelFormat)yuv420Frame->format, srcW, srcH,
      (AVPixelFormat)AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);

  // 1. 创建临时 Mat
  cv::Mat mat(srcH, srcW, CV_8UC3);

  // 2. AVFrame 作为目标
  AVFrame* bgr24Frame = av_frame_alloc();
  av_image_fill_arrays(bgr24Frame->data, bgr24Frame->linesize,
                       (uint8_t*)mat.data, (AVPixelFormat)AV_PIX_FMT_BGR24,
                       srcW, srcH, 1);

  sws_scale(swsCtx, (const uint8_t* const*)yuv420Frame->data,
            yuv420Frame->linesize, 0, srcH, bgr24Frame->data,
            bgr24Frame->linesize);

  // 3. Mat 转 UMat
  cv::UMat umat;
  mat.copyTo(umat);

  // 释放
  av_frame_free(&bgr24Frame);
  sws_freeContext(swsCtx);

  return umat;
}

bool CVUMatToAVFrame(const cv::UMat& inUMat, AVFrame* frame) {
  if (frame == nullptr) {
    return false;
  }
  av_frame_unref(frame);

  AVPixelFormat dstFormat = AV_PIX_FMT_YUV420P;
  int width = inUMat.cols;
  int height = inUMat.rows;

  frame->width = width;
  frame->height = height;
  frame->format = dstFormat;

  int ret = av_frame_get_buffer(frame, 32);
  if (ret < 0) {
    return false;
  }
  ret = av_frame_make_writable(frame);
  if (ret < 0) {
    return false;
  }

  // UMat不能直接通过.data访问，需转为Mat
  cv::UMat tmpYUV;
  cv::cvtColor(inUMat, tmpYUV, cv::COLOR_BGR2YUV_I420);
  // 这时tmpYUV里是YUV420的平面格式

  // 拷贝到CPU可访问的Mat
  cv::Mat yuvMat = tmpYUV.getMat(cv::ACCESS_READ);

  int frame_size = width * height;
  unsigned char* data = yuvMat.data;

  memcpy(frame->data[0], data, frame_size);
  memcpy(frame->data[1], data + frame_size, frame_size / 4);
  memcpy(frame->data[2], data + frame_size * 5 / 4, frame_size / 4);
  return true;
}
}  // namespace

struct XVideoFilterProc::Context {
  Context() {
    if (!faceCascade.load("haarcascade_frontalface_default.xml")) {
      std::cout << "错误：无法加载 Haar 级联分类器！" << std::endl;
    }
  }

  cv::CascadeClassifier faceCascade;
  cv::Mat prevFrame;
  std::vector<cv::Ptr<cv::Tracker>> trackers;
  std::vector<cv::Rect> bboxes;
};

XVideoFilterProc::XVideoFilterProc() {
  ctx_ = std::make_shared<Context>();
}

XVideoFilterProc* XVideoFilterProc::GetInstance() {
  static XVideoFilterProc kProc;
  return &kProc;
}

void XVideoFilterProc::Clear() {
  ctx_ = std::make_shared<Context>();
}

bool XVideoFilterProc::Action(AVFrame* video_frame,
                                  const std::vector<FilterType>& types) {
  try {
    if (types.empty()) {
      return true;
    }
    auto frame = AVFrameToCVMat(video_frame);
    for (auto type : types) {
      if (type == kFaceDect) {
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        // 检测人脸
        int div = 6;
        cv::Mat small;
        cv::resize(gray, small, cv::Size(), 1.0 / (double)div,
                   1.0 / (double)div);
        cv::Size min_size(40 / div, 40 / div);  // 你实际需求
        std::vector<cv::Rect> faces;
        ctx_->faceCascade.detectMultiScale(small, faces, 1.2, 3, 0, min_size);

        for (auto& f : faces) {
          // 因为 resize 了，所以坐标要放大回原尺寸
          f.x *= div;
          f.y *= div;
          f.width *= div;
          f.height *= div;
          cv::rectangle(frame, f, cv::Scalar(0, 255, 0), div);
        }
      }
    }
    return CVMatToAVFrame(frame, video_frame);
#if 0
    if (ctx_->prevFrame.empty()) {
      ctx_->prevFrame = AVFrameToCVMat(video_frame);
      cv::cvtColor(ctx_->prevFrame, ctx_->prevFrame, cv::COLOR_BGR2GRAY);
      return true;
    }
    auto frame = AVFrameToCVMat(video_frame);
    cv::Mat grayFrame;
    cv::cvtColor(frame, grayFrame, cv::COLOR_BGR2GRAY);
    cv::Mat diffFrame;
    cv::absdiff(grayFrame, ctx_->prevFrame, diffFrame);
    cv::threshold(diffFrame, diffFrame, 30, 255, cv::THRESH_BINARY);
    ctx_->prevFrame = grayFrame.clone();
    return CVMatToAVFrame(diffFrame, video_frame);
#endif
#if 0
    auto frame = AVFrameToCVMat(video_frame);

    if (ctx_->prevFrame.empty()) {
      ctx_->prevFrame = frame;
      cv::selectROIs("Tracking", frame, ctx_->bboxes);

      for (const auto& box : ctx_->bboxes) {
        auto tracker = cv::TrackerMIL::create();
        tracker->init(frame, box);
        ctx_->trackers.push_back(tracker);
      }
      return true;
    }

    for (size_t i = 0; i < ctx_->trackers.size(); ++i) {
      ctx_->trackers[i]->update(frame, ctx_->bboxes[i]);
      cv::rectangle(frame, ctx_->bboxes[i], cv::Scalar(255, 0, 0), 2);
    }
    return CVMatToAVFrame(frame, video_frame);
#endif
    return true;
  } catch (...) {
    return false;
  }
}
