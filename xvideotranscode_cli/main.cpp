// 命令行版视频转码工具
// 用法见 PrintUsage()
#include "xconvertor.h"

#include <libavcodec/avcodec.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>

using std::string;

namespace {

// 命令行参数
struct Options {
  string input;
  string output;
  string vcodec = "keep";  // h264 / hevc / none / keep
  string acodec = "keep";  // aac / mp3 / none / keep
  int width = 0;
  int height = 0;
  long long vbr = 0;  // 视频码率 bps, 0=CRF固定质量
  long long abr = 0;  // 音频码率 bps, 0=编码器默认
  int asr = 0;        // 音频采样率
  string preset = "veryslow";
  string profile = "high";
  string tune = "zerolatency";
  string crf = "18";
  bool gpu = false;
  bool quiet = false;
};

void PrintUsage() {
  printf(
      "用法: XVideoTranscodeCLI -i <输入文件> -o <输出文件> [选项]\n"
      "\n"
      "必选:\n"
      "  -i <文件>          输入音视频文件\n"
      "  -o <文件>          输出文件(容器按扩展名识别: .mp4/.mkv/.mp3 等)\n"
      "\n"
      "编码器(-v none 时只转音频; -a none 时只转视频):\n"
      "  -v h264|hevc|none  视频编码器, 默认保持源\n"
      "  -a aac|mp3|none    音频编码器, 默认保持源\n"
      "\n"
      "视频选项:\n"
      "  -w <宽度>  -H <高度>  输出尺寸(自动取偶数)\n"
      "  --vbr <bps>          视频码率, 指定后按码率编码\n"
      "  --preset <名>         默认 veryslow\n"
      "  --crf <数值>          默认 18 (仅未指定码率时生效)\n"
      "  --profile <名>        默认 high\n"
      "  --tune <名>           默认 zerolatency\n"
      "\n"
      "音频选项:\n"
      "  --abr <bps>          音频码率\n"
      "  --asr <hz>           音频采样率(mp3仅支持22050/32000/44100/48000)\n"
      "\n"
      "其它:\n"
      "  --gpu                启用Intel QSV硬件解码/编码(失败自动回退软件)\n"
      "  --quiet              不打印进度\n"
      "  -h, --help           显示帮助\n"
      "\n"
      "示例:\n"
      "  XVideoTranscodeCLI -i in.mp4 -o out.mp4\n"
      "  XVideoTranscodeCLI -i in.mp4 -o out.mp4 -v hevc -w 1280 -h 720\n"
      "  XVideoTranscodeCLI -i in.mp4 -o out.mp3 -v none -a mp3 --abr 128000\n");
}

// 解析参数, 出错返回false
bool ParseArgs(int argc, char** argv, Options* opt) {
  bool has_in = false, has_out = false;
  for (int i = 1; i < argc; i++) {
    string a = argv[i];
    auto next = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        fprintf(stderr, "错误: 缺少 %s 参数值!\n", name);
        return nullptr;
      }
      return argv[++i];
    };
    if (a == "-i") {
      auto v = next("-i");
      if (!v) return false;
      opt->input = v;
      has_in = true;
    } else if (a == "-o") {
      auto v = next("-o");
      if (!v) return false;
      opt->output = v;
      has_out = true;
    } else if (a == "-v") {
      auto v = next("-v");
      if (!v) return false;
      opt->vcodec = v;
    } else if (a == "-a") {
      auto v = next("-a");
      if (!v) return false;
      opt->acodec = v;
    } else if (a == "-w") {
      const char* v = next("-w");
      if (v) opt->width = atoi(v);
    } else if (a == "-h" || a == "--help") {
      PrintUsage();
      exit(0);
    } else if (a == "-H" || a == "--height") {
      const char* v = next("-H");
      if (v) opt->height = atoi(v);
    } else if (a == "--vbr") {
      const char* v = next("--vbr");
      if (v) opt->vbr = atoll(v);
    } else if (a == "--abr") {
      const char* v = next("--abr");
      if (v) opt->abr = atoll(v);
    } else if (a == "--asr") {
      const char* v = next("--asr");
      if (v) opt->asr = atoi(v);
    } else if (a == "--preset") {
      opt->preset = next("--preset");
    } else if (a == "--crf") {
      opt->crf = next("--crf");
    } else if (a == "--profile") {
      opt->profile = next("--profile");
    } else if (a == "--tune") {
      opt->tune = next("--tune");
    } else if (a == "--gpu") {
      opt->gpu = true;
    } else if (a == "--quiet") {
      opt->quiet = true;
    } else {
      fprintf(stderr, "错误: 未知参数 %s\n", a.c_str());
      return false;
    }
  }
  if (!has_in || !has_out) {
    fprintf(stderr, "错误: 必须指定 -i 和 -o!\n");
    return false;
  }
  return true;
}

// GPU设备(618)修正在XConvertor内部自动回退

bool IsMp3SampleRate(int rate) {
  return rate == 22050 || rate == 32000 || rate == 44100 || rate == 48000;
}

int RunTranscode(const Options& opt) {
  XConvertor c;
  c.set_gpu_decode(opt.gpu);
  c.set_gpu_encode(opt.gpu);

  if (!c.Open(opt.input.c_str())) {
    fprintf(stderr, "打开输入文件失败: %s\n", opt.input.c_str());
    return 1;
  }
  auto vp = c.GetVideoCodec();
  auto ap = c.GetAudioCodec();

  std::map<std::string, std::string> video_opts, audio_opts;

  AVCodecParameters* video_para = vp ? vp->para : nullptr;
  AVCodecParameters* audio_para = ap ? ap->para : nullptr;

  // 视频编码器
  if (opt.vcodec == "none") {
    video_para = nullptr;
  } else if (opt.vcodec == "h264" && vp) {
    video_para->codec_id = AV_CODEC_ID_H264;
  } else if (opt.vcodec == "hevc" && vp) {
    video_para->codec_id = AV_CODEC_ID_HEVC;
  } else if (opt.vcodec != "keep") {
    fprintf(stderr, "错误: 不支持的视频编码器 %s (支持 h264/hevc/none)\n",
            opt.vcodec.c_str());
    return 2;
  }

  // 音频编码器
  if (opt.acodec == "none") {
    audio_para = nullptr;
  } else if (opt.acodec == "aac" && ap) {
    audio_para->codec_id = AV_CODEC_ID_AAC;
  } else if (opt.acodec == "mp3" && ap) {
    audio_para->codec_id = AV_CODEC_ID_MP3;
  } else if (opt.acodec != "keep") {
    fprintf(stderr, "错误: 不支持的音频编码器 %s (支持 aac/mp3/none)\n",
            opt.acodec.c_str());
    return 2;
  }

  // MP3限制: 不能带视频; 采样率只支持固定几个
  bool mp3_out = audio_para && audio_para->codec_id == AV_CODEC_ID_MP3;
  if (mp3_out && video_para) {
    fprintf(stderr, "错误: MP3(纯音频)输出不能包含视频流, 请加 -v none!\n");
    return 2;
  }

  // 视频参数
  if (video_para) {
    if (opt.width > 0) video_para->width = opt.width & ~1;  // 取偶数
    if (opt.height > 0) video_para->height = opt.height & ~1;
    if (opt.vbr > 0) {
      video_opts["bit_rate"] = std::to_string(opt.vbr);
    } else {
      // CRF固定质量
      video_opts["preset"] = opt.preset;
      // x265没有high等高配置, profile只对h264有效
      if (video_para->codec_id != AV_CODEC_ID_HEVC) {
        video_opts["profile"] = opt.profile;
      }
      video_opts["tune"] = opt.tune;
      video_opts["crf"] = opt.crf;
    }
  }

  // 音频参数
  if (audio_para) {
    if (opt.asr > 0) audio_para->sample_rate = opt.asr;
    if (mp3_out && !IsMp3SampleRate(audio_para->sample_rate)) {
      fprintf(stderr, "提示: mp3不支持采样率%d, 强制改为44100\n",
              audio_para->sample_rate);
      audio_para->sample_rate = 44100;
    }
    if (opt.abr > 0) audio_para->bit_rate = opt.abr;
  }

  c.Start(opt.output.c_str(),
          video_para, vp ? vp->time_base : nullptr,
          audio_para, ap ? ap->time_base : nullptr,
          video_opts, audio_opts);
  if (c.HasError()) {
    fprintf(stderr, "启动转码失败: %s\n", c.GetError().c_str());
    return 1;
  }

  if (opt.gpu && video_para && !c.video_encode_gpu_used()) {
    fprintf(stderr, "提示: QSV硬件编码不可用, 已回退软件编码\n");
  }

  // 进度显示
  int last_percent = -1;
  while (!c.IsFinished()) {
    float pos = c.GetPos();
    int percent = (int)(pos * 100);
    if (!opt.quiet && percent != last_percent) {
      printf("\r进度 %d%%  ", percent);
      fflush(stdout);
      last_percent = percent;
    }
    MSleep(50);
  }
  printf("\n");

  printf("%s\n", c.DumpInfo().c_str());
  c.Stop();
  if (c.HasError()) {
    fprintf(stderr, "转码失败: %s\n", c.GetError().c_str());
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  // 让中文提示在GBK控制台上正常显示
  SetConsoleOutputCP(CP_UTF8);

  if (argc < 2) {
    PrintUsage();
    return 0;
  }
  Options opt;
  if (!ParseArgs(argc, argv, &opt)) {
    PrintUsage();
    return 2;
  }
  return RunTranscode(opt);
}