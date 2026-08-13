#include "predefine_header.h"
#include "xresample.h"

extern "C" {
#include <libswresample/swresample.h>
}

using namespace std;

XResample::~XResample() {
  if (swr_) swr_free(&swr_);
  av_channel_layout_uninit(&src_ch_);
  av_channel_layout_uninit(&dst_ch_);
}

// 归一化声道布局: UNSPEC 时按声道数补默认布局, 便于比较
static void NormalizeLayout(AVChannelLayout* l) {
  if (l->order == AV_CHANNEL_ORDER_UNSPEC && l->nb_channels > 0) {
    av_channel_layout_default(l, l->nb_channels);
  }
}

bool XResample::Need(const AVFrame* src, const AVCodecContext* dst) const {
  if (!src || !dst) return false;
  if (!swr_) return true;
  if (src->sample_rate != src_sample_rate_) return true;
  if (dst->sample_rate != dst_sample_rate_) return true;
  if ((int)src->format != src_fmt_) return true;
  if ((int)dst->sample_fmt != dst_fmt_) return true;

  AVChannelLayout s = {};
  AVChannelLayout d = {};
  bool need = false;
  if (av_channel_layout_copy(&s, &src->ch_layout) == 0) {
    NormalizeLayout(&s);
    if (av_channel_layout_copy(&d, &dst->ch_layout) == 0) {
      NormalizeLayout(&d);
      if (av_channel_layout_compare(&s, &d) != 0) need = true;
    } else {
      need = true;
    }
  } else {
    need = true;
  }
  av_channel_layout_uninit(&s);
  av_channel_layout_uninit(&d);
  return need;
}

bool XResample::Create(const AVFrame* src, const AVCodecContext* dst) {
  if (swr_) swr_free(&swr_);
  av_channel_layout_uninit(&src_ch_);
  av_channel_layout_uninit(&dst_ch_);
  src_ch_ = {};
  dst_ch_ = {};
  if (!src || !dst) return false;

  src_sample_rate_ = src->sample_rate;
  dst_sample_rate_ = dst->sample_rate;
  src_fmt_ = (int)src->format;
  dst_fmt_ = (int)dst->sample_fmt;
  if (src_fmt_ == AV_SAMPLE_FMT_NONE || dst_fmt_ == AV_SAMPLE_FMT_NONE) return false;
  if (src_sample_rate_ <= 0 || dst_sample_rate_ <= 0) return false;

  if (av_channel_layout_copy(&src_ch_, &src->ch_layout) < 0) return false;
  NormalizeLayout(&src_ch_);
  if (av_channel_layout_copy(&dst_ch_, &dst->ch_layout) < 0) return false;
  NormalizeLayout(&dst_ch_);
  if (src_ch_.nb_channels <= 0 || dst_ch_.nb_channels <= 0) return false;

  swr_ = swr_alloc();
  if (!swr_) return false;
  av_opt_set_chlayout(swr_, "in_chlayout", &src_ch_, 0);
  av_opt_set_int(swr_, "in_sample_rate", src_sample_rate_, 0);
  av_opt_set_sample_fmt(swr_, "in_sample_fmt", (AVSampleFormat)src_fmt_, 0);
  av_opt_set_chlayout(swr_, "out_chlayout", &dst_ch_, 0);
  av_opt_set_int(swr_, "out_sample_rate", dst_sample_rate_, 0);
  av_opt_set_sample_fmt(swr_, "out_sample_fmt", (AVSampleFormat)dst_fmt_, 0);

  if (swr_init(swr_) < 0) {
    swr_free(&swr_);
    return false;
  }
  return true;
}

AVFrame* XResample::Convert(AVFrame* in) {
  if (!in || !swr_) return nullptr;
  int64_t out_samples = av_rescale_rnd(
      swr_get_delay(swr_, in->sample_rate) + in->nb_samples, dst_sample_rate_,
      in->sample_rate, AV_ROUND_UP);
  AVFrame* out = av_frame_alloc();
  if (!out) return nullptr;
  out->format = dst_fmt_;
  out->sample_rate = dst_sample_rate_;
  av_channel_layout_copy(&out->ch_layout, &dst_ch_);
  out->nb_samples = (int)out_samples;
  if (out->nb_samples <= 0 || av_frame_get_buffer(out, 0) < 0) {
    av_frame_free(&out);
    return nullptr;
  }
  int ret = swr_convert(swr_, out->data, out->nb_samples,
                        (const uint8_t* const*)in->data, in->nb_samples);
  if (ret < 0) {
    av_frame_free(&out);
    return nullptr;
  }
  out->nb_samples = ret;
  out->pts = in->pts;
  out->time_base = in->time_base;
  out->pkt_dts = in->pkt_dts;
  return out;
}