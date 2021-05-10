#include "VideoPlayer.h"
#include "VideoPlayerEventHandle.h"

extern AVPixelFormat hw_pix_fmt_;

void VideoPlayer::decodeVideoThread() {
    fprintf(stderr, "%s start \n", __FUNCTION__);
    is_video_thread_finished_ = false;
    int numBytes;
    double video_pts = 0; //当前视频的pts
    double audio_pts = 0; //音频pts
    ///解码视频相关
    AVFrame *pFrame, *pFrameYUV, *tmpFrame, *swFrame;
    uint8_t *out_buffer_yuv; //解码后的yuv数据
    struct SwsContext *img_convert_ctx = nullptr;  //用于解码后的视频格式转换
    pFrame = av_frame_alloc();
    swFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    ///由于解码后的数据不一定都是yuv420p，因此需要将解码后的数据统一转换成YUV420P    
    numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, codec_ctx_->width, codec_ctx_->height);
    out_buffer_yuv = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *)pFrameYUV, out_buffer_yuv, AV_PIX_FMT_YUV420P,
                   codec_ctx_->width, codec_ctx_->height);

    while (1) {
        if (is_quit_) {
            clearVideoQuene(); //清空队列
            break;
        }
        //判断暂停
        if (is_pause_ == true) {
            mSleep(10);
            continue;
        }
        condition_video_->Lock();

        if (video_pack_list_.size() <= 0) {
            condition_video_->Unlock();
            if (is_read_finished_) {
                //队列里面没有数据了且读取完毕了
                break;
            } else {
                mSleep(1); //队列只是暂时没有数据而已
                continue;
            }
        }
        AVPacket pkt1 = video_pack_list_.front();
        video_pack_list_.pop_front();
        condition_video_->Unlock();
        AVPacket *packet = &pkt1;
        //收到这个数据 说明刚刚执行过跳转 现在需要把解码器的数据 清除一下
        if (strcmp((char*)packet->data, FLUSH_DATA) == 0) {
            avcodec_flush_buffers(codec_ctx_);
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(codec_ctx_, packet) != 0) {
            printf("input AVPacket to decoder failed!\n");
            av_packet_unref(packet);
            continue;
        }

        while (0 == avcodec_receive_frame(codec_ctx_, pFrame)) {
            if (packet->dts == AV_NOPTS_VALUE && pFrame->opaque&& *(uint64_t*)pFrame->opaque != AV_NOPTS_VALUE) {
                video_pts = *(uint64_t *)pFrame->opaque;
            } else if (packet->dts != AV_NOPTS_VALUE) {
                video_pts = packet->dts;
            } else {
                video_pts = 0;
            }
            video_pts *= av_q2d(video_stream_->time_base);
            video_clock_ = video_pts;

            if (seek_flag_video_) {
                //发生了跳转 则跳过关键帧到目的时间的这几帧
                if (video_pts < seek_time_) {
                    av_packet_unref(packet);
                    continue;
                } else {
                    seek_flag_video_ = 0;
                }
            }
            ///音视频同步，实现的原理就是，判断是否到显示此帧图像的时间了，没到则休眠5ms，然后继续判断
            while (1) {
                if (is_quit_) {
                    break;
                }

                if (audio_stream_ != NULL && !is_audio_thread_finished_) {
                    if (is_read_finished_ && audio_pack_list_.size() <= 0) {//读取完了 且音频数据也播放完了 就剩下视频数据了  直接显示出来了 不用同步了
                        break;
                    }
                    ///有音频的情况下，将视频同步到音频
                    ///跟音频的pts做对比，比视频快则做延时
                    audio_pts = audio_clock_;
                } else {
                    ///没有音频的情况下，直接同步到外部时钟
                    audio_pts = (av_gettime() - video_start_time_) / 1000000.0;
                    audio_clock_ = audio_pts;
                }
                //主要是 跳转的时候 我们把video_clock设置成0了
                //因此这里需要更新video_pts
                //否则当从后面跳转到前面的时候 会卡在这里
                video_pts = video_clock_;

                if (video_pts <= audio_pts) {
                    break;
                }
                int delayTime = (video_pts - audio_pts) * 1000;
                delayTime = delayTime > 5 ? 5 : delayTime;

                if (!is_need_pause_) {
                    mSleep(delayTime);
                }
            }

            if (pFrame->format == hw_pix_fmt_) {
                /* retrieve data from GPU to CPU */
                if (av_hwframe_transfer_data(swFrame, pFrame, 0) >= 0) {
                    tmpFrame = swFrame;
                }
            } else {
                tmpFrame = pFrame;
            }

            if (img_convert_ctx == nullptr) {
                img_convert_ctx = sws_getContext(codec_ctx_->width, codec_ctx_->height,
                                                 AVPixelFormat(tmpFrame->format), codec_ctx_->width, codec_ctx_->height,
                                                 AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
            }
            sws_scale(img_convert_ctx, (uint8_t const * const *)tmpFrame->data, tmpFrame->linesize,
                      0, codec_ctx_->height, pFrameYUV->data, pFrameYUV->linesize);
            doDisplayVideo(out_buffer_yuv, codec_ctx_->width, codec_ctx_->height);

            if (is_need_pause_) {
                is_pause_ = true;
                is_need_pause_ = false;
            }
        }
        av_packet_unref(packet);
    }
    av_free(pFrame);
    av_free(swFrame);
    av_free(pFrameYUV);
    av_free(out_buffer_yuv);
    sws_freeContext(img_convert_ctx);

    if (!is_quit_) {
        is_quit_ = true;
    }
    is_video_thread_finished_ = true;
    fprintf(stderr, "%s finished \n", __FUNCTION__);
    return;
}