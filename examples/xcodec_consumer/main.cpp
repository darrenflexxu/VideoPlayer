// XCodec 外部应用示例: 以动态链接方式调用 XCodec 完成音视频转码
// 特点: 只包含 XCodec 公共头文件, 无需包含 FFmpeg/Qt/SDL 等依赖头文件
#include "xconvertor.h"

#include <cstdio>
#include <map>
#include <string>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::printf("用法: %s <输入文件> <输出文件>\n", argv[0]);
    return 2;
  }

  XConvertor convertor;
  if (!convertor.Open(argv[1])) {
    std::fprintf(stderr, "打开输入文件失败: %s\n", argv[1]);
    return 1;
  }

  // 取源的音视频参数(保持源编码, 不做改动)
  auto vp = convertor.GetVideoCodec();
  auto ap = convertor.GetAudioCodec();

  std::map<std::string, std::string> video_opts;
  std::map<std::string, std::string> audio_opts;
  convertor.Start(argv[2],
                  vp ? vp->para : nullptr,
                  vp ? vp->time_base : nullptr,
                  ap ? ap->para : nullptr,
                  ap ? ap->time_base : nullptr,
                  video_opts, audio_opts);
  convertor.set_show_progress(false);

  if (convertor.HasError()) {
    std::fprintf(stderr, "启动转码失败: %s\n", convertor.GetError().c_str());
    return 1;
  }

  // 等待转码结束
  while (!convertor.IsFinished()) {
    MSleep(50);
  }

  std::printf("%s\n", convertor.DumpInfo().c_str());
  convertor.Stop();

  if (convertor.HasError()) {
    std::fprintf(stderr, "转码失败: %s\n", convertor.GetError().c_str());
    return 1;
  }
  std::printf("转码完成: %s\n", argv[2]);
  return 0;
}
