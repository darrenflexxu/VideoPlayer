#include <stdio.h>
#include "PcmVolumeControl.h"
#include "VideoPlayer.h"

void VideoPlayer::sdlAudioCallBackFunc(void* userdata, Uint8* stream, int len) {
  VideoPlayer* player = (VideoPlayer*)userdata;
  player->sdlAudioCallBack(stream, len);
}

void VideoPlayer::sdlAudioCallBack(Uint8* stream, int len) {
  int len1, audio_data_size;
  /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据
   */
  while (len > 0) {
    /*  audio_buf_index 和 audio_buf_size
     * 标示我们自己用来放置解码出来的数据的缓冲区，*/
    /*   这些数据待copy到SDL缓冲区， 当audio_buf_index >=
     * audio_buf_size的时候意味着我*/
    /*   们的缓冲为空，没有数据可供copy，这时候需要调用audio_decode_frame来解码出更
     /*   多的桢数据 */
    if (audio_buf_index_ >= audio_buf_size_) {
      audio_data_size = decodeAudioFrame();
      /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
      if (audio_data_size <= 0) {
        /* silence */
        audio_buf_size_ = 1024;
        /* 清零，静音 */
        memset(audio_buf_, 0, audio_buf_size_);
      } else {
        audio_buf_size_ = audio_data_size;
      }
      audio_buf_index_ = 0;
    }
    /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
    len1 = audio_buf_size_ - audio_buf_index_;

    if (len1 > len) {
      len1 = len;
    }
    if (audio_buf_ == NULL) {
      return;
    }
    // 静音 或者 是在暂停的时候跳转了
    if (is_mute_ || is_need_pause_) {
      memset(audio_buf_ + audio_buf_index_, 0, len1);
    } else {
      PcmVolumeControl::RaiseVolume((char*)audio_buf_ + audio_buf_index_, len1,
                                    1, volume_);
    }
    memcpy(stream, (uint8_t*)audio_buf_ + audio_buf_index_, len1);
    len -= len1;
    stream += len1;
    audio_buf_index_ += len1;
  }
}

int VideoPlayer::decodeAudioFrame(bool isBlock) {
  int audioBufferSize = 0;

  while (true) {
    if (is_quit_) {
      is_audio_thread_finished_ = true;
      clearAudioQuene();  // 清空队列
      break;
    }
    if (is_pause_)
      break;

    condition_audio_->Lock();
    // 等待音频包队列
    if (audio_pack_list_.empty()) {
      if (isBlock) {
        condition_audio_->Wait();
        condition_audio_->Unlock();
        continue;
      } else {
        condition_audio_->Unlock();
        break;
      }
    }

    AVPacket packet = audio_pack_list_.front();
    audio_pack_list_.pop_front();
    condition_audio_->Unlock();

    // 时钟同步
    if (packet.pts != AV_NOPTS_VALUE) {
      audio_clock_ = av_q2d(audio_stream_->time_base) * packet.pts;
    }

    // 跳转标志包特殊处理
    if (&(packet.data[0]) && strcmp((char*)packet.data, FLUSH_DATA) == 0) {
#if LIBAVCODEC_VERSION_MAJOR < 58
      avcodec_flush_buffers(audio_stream_->codec);
#else
      avcodec_flush_buffers(audio_codec_ctx_);
#endif
      av_packet_unref(&packet);
      continue;
    }

    if (seek_flag_audio_) {
      if (audio_clock_ < seek_time_) {
        av_packet_unref(&packet);  // 防止泄漏
        continue;
      } else {
        seek_flag_audio_ = false;
      }
    }

    // 解码AVPacket->AVFrame（新版用send/receive模式）
    int ret = avcodec_send_packet(audio_codec_ctx_, &packet);
    av_packet_unref(&packet);
    if (ret < 0) {
      // 错误处理（如不是EAGAIN/EOF）
      continue;
    }

    // 循环接收所有解码输出（通常音频每个send只会输出一个frame）
    while (ret >= 0) {
      if (!audio_frame_)
        audio_frame_ = av_frame_alloc();
      ret = avcodec_receive_frame(audio_codec_ctx_, audio_frame_);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        // 错误处理
        break;
      }
      AVChannelLayout layout;
      av_channel_layout_default(&layout, audio_tgt_channels_);
      if (audio_frame_->format != out_sample_fmt_ ||
          audio_frame_->ch_layout.nb_channels != layout.nb_channels ||
          audio_frame_->sample_rate != out_sample_rate_) {
        // 重采样输出帧分配
        if (!audio_frame_resample_)
          audio_frame_resample_ = av_frame_alloc();

        av_channel_layout_default(&audio_frame_resample_->ch_layout,
                                  audio_tgt_channels_);
        audio_frame_resample_->sample_rate = out_sample_rate_;
        audio_frame_resample_->format = out_sample_fmt_;

        int nb_samples = av_rescale_rnd(
            swr_get_delay(swr_ctx_, audio_frame_->sample_rate) +
                audio_frame_->nb_samples,
            out_sample_rate_, audio_frame_->sample_rate, AV_ROUND_UP);
        audio_frame_resample_->nb_samples = nb_samples;

        int ret2 = av_frame_get_buffer(audio_frame_resample_, 0);
        if (ret2 < 0) {
          av_log(nullptr, AV_LOG_ERROR, "Failed to allocate resample buffer\n");
          av_frame_unref(audio_frame_resample_);
          continue;
        }

        // 重采样（新版API）
        int len2 =
            swr_convert(swr_ctx_, audio_frame_resample_->data, nb_samples,
                        (const uint8_t**)audio_frame_->extended_data,
                        audio_frame_->nb_samples);

        if (len2 < 0) {
          av_log(nullptr, AV_LOG_ERROR, "swr_convert failed.\n");
          continue;
        }

        int resampled_data_size = av_samples_get_buffer_size(
            nullptr, audio_tgt_channels_, len2, out_sample_fmt_, 1);
        if (resampled_data_size <= 0)
          continue;
        memcpy(audio_buf_, audio_frame_resample_->data[0],
               resampled_data_size);  // audio_buf_ 需预分配

        audioBufferSize = resampled_data_size;
      } else {
        // 不需要重采样，直接拷贝
        int data_size = av_samples_get_buffer_size(
            nullptr, audio_frame_->ch_layout.nb_channels,
            audio_frame_->nb_samples, (AVSampleFormat)audio_frame_->format, 1);
        memcpy(audio_buf_, audio_frame_->data[0], data_size);
        audioBufferSize = data_size;
      }

      av_frame_unref(audio_frame_);
      break;  // 取到有效帧退出（多数情况下每个包最多解一个音频帧）
    }
    break;
  }
  return audioBufferSize;
}