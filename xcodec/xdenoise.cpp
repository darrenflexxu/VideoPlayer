#include "xdenoise.h"

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
}

#include <cstdio>

XAudioDenoise::~XAudioDenoise() { Close(); }

bool XAudioDenoise::Open(int sample_rate, int channels, float nr_db)
{
    Close();
    if (sample_rate <= 0 || channels <= 0)
        return false;

    AVChannelLayout cl = {};
    av_channel_layout_default(&cl, channels);
    char desc[64] = {0};
    av_channel_layout_describe(&cl, desc, sizeof(desc));

    char args[256] = {0};
    snprintf(args, sizeof(args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=fltp:channel_layout=%s",
             sample_rate, sample_rate, desc);

    graph_ = avfilter_graph_alloc();
    if (!graph_)
        return false;

    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    const AVFilter* afftdn = avfilter_get_by_name("afftdn");
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffer || !afftdn || !abuffersink) {
        fprintf(stderr, "[DENOISE] filter missing(abuffer/afftdn/abuffersink)\n");
        Close();
        return false;
    }
    if (avfilter_graph_create_filter(&src_, abuffer, "src", args, nullptr,
                                     graph_) < 0) {
        fprintf(stderr, "[DENOISE] abuffer create failed: %s\n", args);
        Close();
        return false;
    }
    AVFilterContext* dn = nullptr;
    if (avfilter_graph_create_filter(&dn, afftdn, "dn", nullptr, nullptr,
                                     graph_) < 0) {
        fprintf(stderr, "[DENOISE] afftdn create failed\n");
        Close();
        return false;
    }
    if (nr_db > 0)
        av_opt_set_double(dn, "nr", (double)nr_db, 0);
    if (avfilter_graph_create_filter(&sink_, abuffersink, "sink", nullptr,
                                     nullptr, graph_) < 0) {
        fprintf(stderr, "[DENOISE] abuffersink create failed\n");
        Close();
        return false;
    }
    //abuffersink 不做格式约束: afftdn 直通输入格式(FLTP), 输出由调用方校验

    if (avfilter_link(src_, 0, dn, 0) < 0 ||
        avfilter_link(dn, 0, sink_, 0) < 0 ||
        avfilter_graph_config(graph_, nullptr) < 0) {
        fprintf(stderr, "[DENOISE] graph link/config failed\n");
        Close();
        return false;
    }
    return true;
}

void XAudioDenoise::Close()
{
    if (graph_)
        avfilter_graph_free(&graph_);
    graph_ = nullptr;
    src_ = nullptr;
    sink_ = nullptr;
}

std::vector<AVFrame*> XAudioDenoise::Drain()
{
    std::vector<AVFrame*> outs;
    if (!graph_)
        return outs;
    for (;;) {
        AVFrame* f = av_frame_alloc();
        if (!f)
            break;
        int re = av_buffersink_get_frame(sink_, f);
        if (re == 0) {
            outs.push_back(f);
            continue;
        }
        av_frame_free(&f);
        break;  //EAGAIN无输出或EOF
    }
    return outs;
}

std::vector<AVFrame*> XAudioDenoise::Filter(AVFrame* in)
{
    std::vector<AVFrame*> outs;
    if (!graph_ || !in)
        return outs;
    if (av_buffersrc_add_frame(src_, in) < 0)
        return outs;
    return Drain();
}

std::vector<AVFrame*> XAudioDenoise::Flush()
{
    std::vector<AVFrame*> outs;
    if (!graph_)
        return outs;
    av_buffersrc_add_frame(src_, nullptr);
    return Drain();
}
