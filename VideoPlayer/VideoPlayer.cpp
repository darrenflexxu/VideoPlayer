#include "VideoPlayer.h"
#include <stdio.h>
#include "Audio/PcmVolumeControl.h"

VideoPlayer::VideoPlayer() {
    condition_video_ = new Cond;
    condition_audio_ = new Cond;
    player_state_ = VideoPlayer_Stop;
    mVideoPlayerCallBack = nullptr;
    mAudioID = 0;
    is_mute_ = false;
    is_need_pause_ = false;
    volume_ = 1;
    video_stream_ = nullptr;
    audio_stream_ = nullptr;
    format_ctx_ = nullptr;
    codec_ctx_ = nullptr;
    codec_ = nullptr;
    audio_codec_ctx_ = nullptr;
    audio_codec_ = nullptr;
    audio_frame_ = nullptr;
    audio_frame_resample_ = nullptr;
    swr_ctx_ = nullptr;
}

VideoPlayer::~VideoPlayer() {
}

bool VideoPlayer::initPlayer() {
    av_register_all(); //初始化FFMPEG  调用了这个才能正常使用编码器和解码器
    avformat_network_init(); //支持打开网络文件
    return true;
}

bool VideoPlayer::startPlay(const std::string &filePath) {
    if (player_state_ != VideoPlayer_Stop) {
        return false;
    }
    is_quit_ = false;
    is_pause_ = false;
    if (!filePath.empty())
        file_path_ = filePath;
    //启动新的线程实现读取视频文件
    std::thread([&] (VideoPlayer *pointer) {
        pointer->readVideoFile();
    }, this).detach();
    return true;
}

bool VideoPlayer::replay() {
    stop();
    startPlay(file_path_);
    return true;
}

bool VideoPlayer::play() {
    is_need_pause_ = false;
    is_pause_ = false;

    if (player_state_ != VideoPlayer_Pause) {
        return false;
    }
    uint64_t pauseTime = av_gettime() - video_start_time_; //暂停了多长时间
    video_start_time_ += pauseTime; //将暂停的时间加到开始播放的时间上，保证同步不受暂停的影响
    player_state_ = VideoPlayer_Playing;
    doPlayerStateChanged(VideoPlayer_Playing, video_stream_ != nullptr, audio_stream_ != nullptr);
    return true;
}

bool VideoPlayer::pause() {
    fprintf(stderr, "%s is_pause_=%d \n", __FUNCTION__, is_pause_);
    is_pause_ = true;

    if (player_state_ != VideoPlayer_Playing) {
        return false;
    }
    pause_start_time_ = av_gettime();
    player_state_ = VideoPlayer_Pause;
    doPlayerStateChanged(VideoPlayer_Pause, video_stream_ != nullptr, audio_stream_ != nullptr);
    return true;
}

bool VideoPlayer::stop(bool isWait) {
    if (player_state_ == VideoPlayer_Stop) {
        return false;
    }
    player_state_ = VideoPlayer_Stop;
    is_quit_ = true;

    if (isWait) {
        while (!is_read_thread_finished_) {
            mSleep(3);
        }
    }
    return true;
}

void VideoPlayer::seek(int64_t pos) {
    if (!seek_req_) {
        seek_pos_ = pos;
        seek_req_ = 1;
    }
}

void VideoPlayer::setVolume(float value) {
    volume_ = value;
}

double VideoPlayer::getCurrentTime() {
    return audio_clock_;
}

int64_t VideoPlayer::getTotalTime() {
    return format_ctx_->duration;
}

int VideoPlayer::openSDL() {
    ///打开SDL，并设置播放的格式为:AUDIO_S16LSB 双声道，44100hz
    ///后期使用ffmpeg解码完音频后，需要重采样成和这个一样的格式，否则播放会有杂音
    SDL_AudioSpec wanted_spec, spec;
    int wanted_nb_channels = 2;
    int samplerate = 44100;
    wanted_spec.channels = wanted_nb_channels;
    wanted_spec.freq = samplerate;
    wanted_spec.format = AUDIO_S16SYS; // 具体含义请查看“SDL宏定义”部分
    wanted_spec.silence = 0;            // 0指示静音
    wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;  // 自定义SDL缓冲区大小
    wanted_spec.callback = sdlAudioCallBackFunc;  // 回调函数
    wanted_spec.userdata = this;                  // 传给上面回调函数的外带数据

    int num = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < num; i++) {
        mAudioID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(i, 0), false, &wanted_spec, &spec, 0);
        if (mAudioID > 0) {
            break;
        }
    }
    if (mAudioID <= 0) {
        is_audio_thread_finished_ = true;
        return -1;
    }
    fprintf(stderr, "mAudioID=%d\n\n\n\n\n\n", mAudioID);
    return 0;
}

void VideoPlayer::closeSDL() {
    if (mAudioID > 0) {
        SDL_LockAudioDevice(mAudioID);
        SDL_PauseAudioDevice(mAudioID, 1);
        SDL_UnlockAudioDevice(mAudioID);
        SDL_CloseAudioDevice(mAudioID);
    }
    mAudioID = 0;
}

void VideoPlayer::readVideoFile() {
    ///SDL初始化需要放入子线程中，否则有些电脑会有问题。
    if (SDL_Init(SDL_INIT_AUDIO)) {
        doOpenSdlFailed(-100);
        fprintf(stderr, "Could not initialize SDL - %s. \n", SDL_GetError());
        return;
    }
    is_read_thread_finished_ = false;
    is_read_finished_ = false;
    const char * file_path = file_path_.c_str();
    format_ctx_ = nullptr;
    codec_ctx_ = nullptr;
    codec_ = nullptr;
    audio_codec_ctx_ = nullptr;
    audio_codec_ = nullptr;
    audio_frame_ = nullptr;
    audio_stream_ = nullptr;
    video_stream_ = nullptr;
    audio_clock_ = 0;
    video_clock_ = 0;
    int audioStream, videoStream;
    //Allocate an AVFormatContext.
    format_ctx_ = avformat_alloc_context();

    if (avformat_open_input(&format_ctx_, file_path, nullptr, nullptr) != 0) {
        fprintf(stderr, "can't open the file. \n");
        doOpenVideoFileFailed();
        goto end;
    }

    if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
        fprintf(stderr, "Could't find stream infomation.\n");
        doOpenVideoFileFailed();
        goto end;
    }
    videoStream = -1;
    audioStream = -1;
    ///循环查找视频中包含的流信息，
    for (int i = 0; i < format_ctx_->nb_streams; i++) {
        if (format_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
        }
        if (format_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO  && audioStream < 0) {
            audioStream = i;
        }
    }
    doTotalTimeChanged(getTotalTime());
    ///打开视频解码器，并启动视频线程
    if (videoStream >= 0) {
        ///查找视频解码器
        codec_ctx_ = format_ctx_->streams[videoStream]->codec;
        codec_ = avcodec_find_decoder(codec_ctx_->codec_id);

        if (codec_ == nullptr) {
            fprintf(stderr, "PCodec not found.\n");
            doOpenVideoFileFailed();
            goto end;
        }
        ///打开视频解码器
        if (avcodec_open2(codec_ctx_, codec_, NULL) < 0) {
            fprintf(stderr, "Could not open video codec.\n");
            doOpenVideoFileFailed();
            goto end;
        }
        video_stream_ = format_ctx_->streams[videoStream];
        ///创建一个线程专门用来解码视频
        std::thread([&] (VideoPlayer *pointer) {
            pointer->decodeVideoThread();
        }, this).detach();
    }

    if (audioStream >= 0) {
        ///查找音频解码器
        audio_codec_ctx_ = format_ctx_->streams[audioStream]->codec;
        audio_codec_ = avcodec_find_decoder(audio_codec_ctx_->codec_id);

        if (audio_codec_ == NULL) {
            fprintf(stderr, "ACodec not found.\n");
            audioStream = -1;
        } else {
            ///打开音频解码器
            if (avcodec_open2(audio_codec_ctx_, audio_codec_, nullptr) < 0) {
                fprintf(stderr, "Could not open audio codec.\n");
                doOpenVideoFileFailed();
                goto end;
            }
            ///解码音频相关
            audio_frame_ = av_frame_alloc();
            //重采样设置选项-----------------------------------------------------------start
            audio_frame_resample_ = nullptr;
            //frame->16bit 44100 PCM 统一音频采样格式与采样率
            swr_ctx_ = nullptr;
            //输入的声道布局
            int in_ch_layout;
            //输出的声道布局
            int out_ch_layout = av_get_default_channel_layout(audio_tgt_channels_); ///AV_CH_LAYOUT_STEREO
            out_ch_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
            /// 这里音频播放使用了固定的参数
            /// 强制将音频重采样成44100 双声道  AV_SAMPLE_FMT_S16
            /// SDL播放中也是用了同样的播放参数
            //重采样设置选项----------------
            //输入的采样格式
            in_sample_fmt_ = audio_codec_ctx_->sample_fmt;
            //输出的采样格式 16bit PCM
            out_sample_fmt_ = AV_SAMPLE_FMT_S16;
            //输入的采样率
            in_sample_rate_ = audio_codec_ctx_->sample_rate;
            //输入的声道布局
            in_ch_layout = audio_codec_ctx_->channel_layout;
            //输出的采样率
            out_sample_rate_ = 44100;
            //输出的声道布局
            audio_tgt_channels_ = 2; ///av_get_channel_layout_nb_channels(out_ch_layout);
            out_ch_layout = av_get_default_channel_layout(audio_tgt_channels_); ///AV_CH_LAYOUT_STEREO
            out_ch_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;            
            /// wav/wmv 文件获取到的audio_codec_ctx_->channel_layout为0会导致后面的初始化失败，因此这里需要加个判断。
            if (in_ch_layout <= 0) {
                in_ch_layout = av_get_default_channel_layout(audio_codec_ctx_->channels);
            }
            swr_ctx_ = swr_alloc_set_opts(nullptr, out_ch_layout, out_sample_fmt_, out_sample_rate_,
                                        in_ch_layout, in_sample_fmt_, in_sample_rate_, 0, nullptr);
            /** Open the resampler with the specified parameters. */
            int ret = swr_init(swr_ctx_);
            if (ret < 0) {
                char buff[128] = {0};
                av_strerror(ret, buff, 128);
                fprintf(stderr, "Could not open resample context %s\n", buff);
                swr_free(&swr_ctx_);
                swr_ctx_ = nullptr;
                doOpenVideoFileFailed();
                goto end;
            }
            //存储pcm数据
            int out_linesize = out_sample_rate_ * audio_tgt_channels_;
            out_linesize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
            audio_stream_ = format_ctx_->streams[audioStream];
            ///打开SDL播放声音
            int code = openSDL();

            if (code == 0) {
                SDL_LockAudioDevice(mAudioID);
                SDL_PauseAudioDevice(mAudioID, 0);
                SDL_UnlockAudioDevice(mAudioID);
                is_audio_thread_finished_ = false;
            } else {
                doOpenSdlFailed(code);
            }
        }
    }
    player_state_ = VideoPlayer_Playing;
    doPlayerStateChanged(VideoPlayer_Playing, video_stream_ != nullptr, audio_stream_ != nullptr);
    video_start_time_ = av_gettime();
    fprintf(stderr, "%s is_quit_=%d is_pause_=%d \n", __FUNCTION__, is_quit_, is_pause_);

    while (1) {
        if (is_quit_) {
            //停止播放了
            break;
        }

        if (seek_req_) {
            int stream_index = -1;
            int64_t seek_target = seek_pos_;

            if (videoStream >= 0) {
                stream_index = videoStream;
            } else if (audioStream >= 0) {
                stream_index = audioStream;
            }
            AVRational aVRational = {1, AV_TIME_BASE};

            if (stream_index >= 0) {
                seek_target = av_rescale_q(seek_target, aVRational, format_ctx_->streams[stream_index]->time_base);
            }

            if (av_seek_frame(format_ctx_, stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
                fprintf(stderr, "%s: error while seeking\n", format_ctx_->filename);
            } else {
                if (audioStream >= 0) {
                    AVPacket packet;
                    av_new_packet(&packet, 10);
                    strcpy((char*)packet.data, FLUSH_DATA);
                    clearAudioQuene(); //清除队列
                    inputAudioQuene(packet); //往队列中存入用来清除的包
                }

                if (videoStream >= 0) {
                    AVPacket packet;
                    av_new_packet(&packet, 10);
                    strcpy((char*)packet.data, FLUSH_DATA);
                    clearVideoQuene(); //清除队列
                    inputVideoQuene(packet); //往队列中存入用来清除的包
                    video_clock_ = 0;
                }
                video_start_time_ = av_gettime() - seek_pos_;
                pause_start_time_ = av_gettime();
            }
            seek_req_ = 0;
            seek_time_ = seek_pos_ / 1000000.0;
            seek_flag_audio_ = 1;
            seek_flag_video_ = 1;

            if (is_pause_) {
                is_need_pause_ = true;
                is_pause_ = false;
            }
        }
        //这里做了个限制  当队列里面的数据超过某个大小的时候 就暂停读取  防止一下子就把视频读完了，导致的空间分配不足
        //这个值可以稍微写大一些
        if (audio_pack_list_.size() > MAX_AUDIO_SIZE || video_pack_list_.size() > MAX_VIDEO_SIZE) {
            mSleep(10);
            continue;
        }

        if (is_pause_ == true) {
            mSleep(10);
            continue;
        }
        AVPacket packet;

        if (av_read_frame(format_ctx_, &packet) < 0) {
            is_read_finished_ = true;

            if (is_quit_) {
                break; //解码线程也执行完了 可以退出了
            }
            mSleep(10);
            continue;
        }

        if (packet.stream_index == videoStream) {
            inputVideoQuene(packet);
            //这里我们将数据存入队列 因此不调用 av_free_packet 释放
        } else if (packet.stream_index == audioStream) {
            if (is_audio_thread_finished_) { ///SDL没有打开，则音频数据直接释放
                av_packet_unref(&packet);
            } else {
                inputAudioQuene(packet);
                //这里我们将数据存入队列 因此不调用 av_free_packet 释放
            }
        } else {
            // Free the packet that was allocated by av_read_frame
            av_packet_unref(&packet);
        }
    }

    ///文件读取结束 跳出循环的情况
    ///等待播放完毕
    while (!is_quit_) {
        mSleep(100);
    }
end:
    clearAudioQuene();
    clearVideoQuene();
    //不是外部调用的stop 是正常播放结束
    if (player_state_ != VideoPlayer_Stop) {
        stop();
    }

    while ((video_stream_ != nullptr && !is_video_thread_finished_) ||
        (audio_stream_ != nullptr && !is_audio_thread_finished_)) {
        mSleep(10);
    } //确保视频线程结束后 再销毁队列
    closeSDL();

    if (swr_ctx_ != nullptr) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }

    if (audio_frame_ != nullptr) {
        av_frame_free(&audio_frame_);
        audio_frame_ = nullptr;
    }

    if (audio_frame_resample_ != nullptr) {
        av_frame_free(&audio_frame_resample_);
        audio_frame_resample_ = nullptr;
    }

    if (audio_codec_ctx_ != nullptr) {
        avcodec_close(audio_codec_ctx_);
        audio_codec_ctx_ = nullptr;
    }

    if (codec_ctx_ != nullptr) {
        avcodec_close(codec_ctx_);
        codec_ctx_ = nullptr;
    }
    avformat_close_input(&format_ctx_);
    avformat_free_context(format_ctx_);
    SDL_Quit();
    doPlayerStateChanged(VideoPlayer_Stop, video_stream_ != nullptr, audio_stream_ != nullptr);
    is_read_thread_finished_ = true;
    fprintf(stderr, "%s finished \n", __FUNCTION__);
}

bool VideoPlayer::inputVideoQuene(const AVPacket &pkt) {
    if (av_dup_packet((AVPacket*)&pkt) < 0) {
        return false;
    }
    condition_video_->Lock();
    video_pack_list_.push_back(pkt);
    condition_video_->Signal();
    condition_video_->Unlock();
    return true;
}

void VideoPlayer::clearVideoQuene() {
    condition_video_->Lock();
    for (AVPacket pkt : video_pack_list_) {
        av_packet_unref(&pkt);
    }
    video_pack_list_.clear();
    condition_video_->Unlock();
}

bool VideoPlayer::inputAudioQuene(const AVPacket &pkt) {
    if (av_dup_packet((AVPacket*)&pkt) < 0) {
        return false;
    }
    condition_audio_->Lock();
    audio_pack_list_.push_back(pkt);
    condition_audio_->Signal();
    condition_audio_->Unlock();
    return true;
}

void VideoPlayer::clearAudioQuene() {
    condition_audio_->Lock();
    for (AVPacket pkt : audio_pack_list_) {
        av_packet_unref(&pkt);
    }
    audio_pack_list_.clear();
    condition_audio_->Unlock();
}

///当使用界面类继承了本类之后，以下函数不会执行
///打开文件失败
void VideoPlayer::doOpenVideoFileFailed(const int &code) {
    fprintf(stderr, "%s \n", __FUNCTION__);

    if (mVideoPlayerCallBack != nullptr) {
        mVideoPlayerCallBack->onOpenVideoFileFailed(code);
    }
}

///打开sdl失败的时候回调此函数
void VideoPlayer::doOpenSdlFailed(const int &code) {
    fprintf(stderr, "%s \n", __FUNCTION__);

    if (mVideoPlayerCallBack != nullptr) {
        mVideoPlayerCallBack->onOpenSdlFailed(code);
    }
}

///获取到视频时长的时候调用此函数
void VideoPlayer::doTotalTimeChanged(const int64_t &uSec) {
    fprintf(stderr, "%s \n", __FUNCTION__);

    if (mVideoPlayerCallBack != nullptr) {
        mVideoPlayerCallBack->onTotalTimeChanged(uSec);
    }
}

///播放器状态改变的时候回调此函数
void VideoPlayer::doPlayerStateChanged(const VideoPlayerState &state, const bool &hasVideo, const bool &hasAudio) {
    fprintf(stderr, "%s \n", __FUNCTION__);

    if (mVideoPlayerCallBack != nullptr) {
        mVideoPlayerCallBack->onPlayerStateChanged(state, hasVideo, hasAudio);
    }
}

///显示视频数据，此函数不宜做耗时操作，否则会影响播放的流畅性。
void VideoPlayer::doDisplayVideo(const uint8_t *yuv420Buffer, const int &width, const int &height) {
    //    fprintf(stderr, "%s \n", __FUNCTION__);
    if (mVideoPlayerCallBack != nullptr) {
        VideoFramePtr videoFrame = std::make_shared<VideoFrame>();
        videoFrame->initBuffer(width, height);
        videoFrame->setYUVbuf(yuv420Buffer);
        mVideoPlayerCallBack->onDisplayVideo(videoFrame);
    }
}