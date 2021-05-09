#include "VideoPlayer.h"
#include <stdio.h>
#include "PcmVolumeControl.h"

void VideoPlayer::sdlAudioCallBackFunc(void *userdata, Uint8 *stream, int len) {
    VideoPlayer *player = (VideoPlayer*)userdata;
    player->sdlAudioCallBack(stream, len);
}

void VideoPlayer::sdlAudioCallBack(Uint8 *stream, int len) {
    int len1, audio_data_size;
    /*   len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，*/
        /*   这些数据待copy到SDL缓冲区， 当audio_buf_index >= audio_buf_size的时候意味着我*/
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
        //静音 或者 是在暂停的时候跳转了 
        if (is_mute_ || is_need_pause_)  {
            memset(audio_buf_ + audio_buf_index_, 0, len1);
        } else {
            PcmVolumeControl::RaiseVolume((char*)audio_buf_ + audio_buf_index_, len1, 1, volume_);
        }
        memcpy(stream, (uint8_t *)audio_buf_ + audio_buf_index_, len1);
        len -= len1;
        stream += len1;
        audio_buf_index_ += len1;
    }
}

int VideoPlayer::decodeAudioFrame(bool isBlock) {
    int audioBufferSize = 0;

    while (1) {
        if (is_quit_) {
            is_audio_thread_finished_ = true;
            clearAudioQuene(); //清空队列
            break;
        }
        //判断暂停
        if (is_pause_ == true) {
            break;
        }
        condition_audio_->Lock();

        if (audio_pack_list_.size() <= 0) {
            if (isBlock) {
                condition_audio_->Wait();
            } else {
                condition_audio_->Unlock();
                break;
            }
        }
        AVPacket packet = audio_pack_list_.front();
        audio_pack_list_.pop_front();
        condition_audio_->Unlock();
        AVPacket *pkt = &packet;
        /* if update, update the audio clock w/pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            audio_clock_ = av_q2d(audio_stream_->time_base) * pkt->pts;
        }
        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if (strcmp((char*)pkt->data, FLUSH_DATA) == 0) {
            avcodec_flush_buffers(audio_stream_->codec);
            av_packet_unref(pkt);
            continue;
        }

        if (seek_flag_audio_) {
            //发生了跳转 则跳过关键帧到目的时间的这几帧
            if (audio_clock_ < seek_time_) {
                continue;
            } else {
                seek_flag_audio_ = 0;
            }
        }
        //解码AVPacket->AVFrame
        int got_frame = 0;
        int size = avcodec_decode_audio4(audio_codec_ctx_, audio_frame_, &got_frame, &packet);
        //保存重采样之前的一个声道的数据方法
        //size_t unpadded_linesize = audio_frame_->nb_samples * av_get_bytes_per_sample((AVSampleFormat) audio_frame_->format);
        //static FILE * fp = fopen("out.pcm", "wb");
        //fwrite(audio_frame_->extended_data[0], 1, unpadded_linesize, fp);
        av_packet_unref(&packet);

        if (got_frame) {
            /// ffmpeg解码之后得到的音频数据不是SDL想要的，
            /// 因此这里需要重采样成44100 双声道 AV_SAMPLE_FMT_S16
            if (audio_frame_resample_ == NULL) {
                audio_frame_resample_ = av_frame_alloc();
            }
            if (audio_frame_resample_->nb_samples != audio_frame_->nb_samples) {
                audio_frame_resample_->nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx_, out_sample_rate_) + audio_frame_->nb_samples,
                                                             out_sample_rate_, in_sample_rate_, AV_ROUND_UP);
                av_samples_fill_arrays(audio_frame_resample_->data,
                                       audio_frame_resample_->linesize, audio_buf_,
                                       audio_tgt_channels_, audio_frame_resample_->nb_samples, out_sample_fmt_, 0);
            }
            int len2 = swr_convert(swr_ctx_, audio_frame_resample_->data, audio_frame_resample_->nb_samples, 
                (const uint8_t**)audio_frame_->data, audio_frame_->nb_samples);
            int resampled_data_size = len2 * audio_tgt_channels_ * av_get_bytes_per_sample(out_sample_fmt_);
            audioBufferSize = resampled_data_size;
            break;
        }
    }
    return audioBufferSize;
}