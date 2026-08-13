#include "predefine_header.h"

using namespace std;
void XDecodeTask::set_time_base(AVRational* time_base)
{
    if (!time_base)return;
    unique_lock<mutex> lock(mux_);
    if (time_base_)
        delete time_base_;
    time_base_ = new AVRational();
    time_base_->den = time_base->den;
    time_base_->num = time_base->num;
}

AVCodecContext * XDecodeTask::GetCodecContext() const {
    return decode_.get_codec_context();
}

bool XDecodeTask::EndOfDecode() {
  return end_decode_;
}

bool XDecodeTask::ignoreMaxPkts(bool ignore) {
  pkt_list_.IgnoreMaxPackets(ignore);
  return true;
}

int XDecodeTask::get_Current_decode_frame_count() {
  return decode_frame_count_;
}

const char* XDecodeTask::decoder_name() {
  auto c = decode_.get_codec_context();
  return (c && c->codec) ? c->codec->name : "";
}

/// <summary>
/// 清理缓存
/// </summary>
void XDecodeTask::Clear()
{
    pkt_list_.Clear();
    unique_lock<mutex> lock(mux_);
    while (!frames_.empty())
    {
        av_frame_free(&frames_.front());
        frames_.pop_front();
    }
    cur_pts_ = -1;
    decode_.Clear();
}
void XDecodeTask::Stop()
{
    XThread::Stop();
    pkt_list_.Clear();
   
    unique_lock<mutex> lock(mux_);
    decode_.set_c(nullptr);
    is_open_ = false;
    if (time_base_)
        delete time_base_;
    time_base_ = nullptr;
    while (!frames_.empty())
    {
        av_frame_free(&frames_.front());
        frames_.pop_front();
    }
}
/// <summary>
/// 打开解码器
/// </summary>
bool XDecodeTask::Open(AVCodecParameters* para)
{
    if (!para)
    {
        LOGERROR("para is null!");
        return false;
    }
    unique_lock<mutex> lock(mux_);
    is_open_ = false;
    gpu_used_ = false;
    auto c = decode_.Create(para->codec_id, false, gpu_decode_);
    if (!c)
    {
        LOGERROR("decode_.Create failed!");
        return false;
    }
    //复制视频参数
    avcodec_parameters_to_context(c, para);
    decode_.set_c(c);

    if (gpu_decode_) {
        decode_.set_gpu_direct_render(gpu_direct_render_);
        decode_.InitHW();
    }

    if (!decode_.Open())
    {
        if (gpu_decode_) {
            // GPU 解码失败(如无QSV设备), 回退软件解码
            LOGERROR("gpu decode open failed! fallback to software");
            decode_.set_c(nullptr);
            c = decode_.Create(para->codec_id, false, false);
            if (!c) {
                LOGERROR("decode_.Create failed!");
                return false;
            }
            avcodec_parameters_to_context(c, para);
            decode_.set_c(c);
            if (!decode_.Open()) {
                LOGERROR("decode_.Open() failed!");
                return false;
            }
        } else {
            LOGERROR("decode_.Open() failed!");
            return false;
        }
    }
    LOGINFO("Open decode success!");
    gpu_used_ = gpu_decode_;
    is_open_ = true;
    return true;
}

//责任链处理函数
void XDecodeTask::Do(AVPacket* pkt)
{
    cout << "D" << flush;

    if (!pkt || pkt->stream_index != stream_index_) //判断是否是视频
    {
        return;
    }
    pkt_list_.Push(pkt);
    recv_packet_count_ += 1;
    if (block_size_ <= 0)return;
    while (!is_exit_)
    {
        if (pkt_list_.Size() > block_size_)
        {
            MSleep(1);
            continue;
        }
        break;
    }
}

AVFrame* XDecodeTask::GetFrame()
{
    unique_lock<mutex> lock(mux_);
    if (frame_cache_)
    {
        if (frames_.empty())return nullptr;
        auto f = frames_.front();
        frames_.pop_front();
        return f;
    }

    if (!need_view_ || !frame_ || !frame_->buf[0])return nullptr;
    auto f = av_frame_alloc();
    auto re = av_frame_ref(f, frame_);//引用加1
    if (re != 0)
    {
        av_frame_free(&f);
        PrintErr(re);
        return nullptr;
    }
    need_view_ = false;
    return f;
}
bool XDecodeTask::IsVideoFinish() {
  return frame_cache_ && frames_.empty();
}
void XDecodeTask::ClearFinish() {
  frame_cache_ = false;
  frames_.clear();
}
//线程主函数
void XDecodeTask::Main()
{
    {
    unique_lock<mutex> lock(mux_);
    if(!frame_)
        frame_ = av_frame_alloc();
    }


    while (!is_exit_)
    {
        if (is_pause()) //暂停
        {
            MSleep(1);
            continue;
        }
        
        //同步
        while (!is_exit_)
        {
            if (syn_pts_ >= 0 && cur_pts_ > syn_pts_)
            {
                MSleep(1);
                continue;
            }
            break;
        }

        auto pkt = pkt_list_.Pop();
        if (!pkt) {
          MSleep(1);
          continue;
        }

        if (pkt->size == 0 && !has_next()) {
          auto list = decode_.End();
          decode_frame_count_ += list.size();

          if (!list.empty()) {
            cur_pts_ = list.back()->pts;  // 转换成毫秒
            if (time_base_)
              cur_ms_ = av_rescale_q(list.back()->pts, *time_base_, {1, 1000});
          }
          frames_.insert(frames_.end(), list.begin(), list.end());
          frame_cache_ = true;
          continue;
        }
        // 发送到解码线程
        auto re = decode_.Send(pkt);
        
        av_packet_free(&pkt);
        if (!re) {
          MSleep(1);
          continue;
        }
        {
            unique_lock<mutex> lock(mux_);
            decode_.set_gpu_direct_render(gpu_decode_ && gpu_direct_render_);

            if (frame_) {
              av_frame_unref(frame_);
            }

            if(decode_.Recv(frame_))
            { 
                cout << "@" << flush;
                need_view_ = true;
                cur_pts_ = frame_->pts;
                //转换成毫秒
                if(time_base_)
                    cur_ms_ = av_rescale_q(frame_->pts,*time_base_, 
                    { 1,1000 });;

                if (has_next()) {
                  decode_frame_count_ += 1;
                  Next(av_frame_clone(frame_));
                  continue;
                }
            }
            if (frame_cache_)
            {
                frames_.push_back(av_frame_clone(frame_));
            }
        }
        MSleep(1);
    }

    if (has_next()) {
      while (auto pkt = pkt_list_.Pop()) {
        decode_.Send(pkt);
        MSleep(1);
        if (frame_) {
          av_frame_unref(frame_);
        }

        if (!decode_.Recv(frame_)) {
          continue;
        }
        decode_frame_count_ += 1;
        cur_pts_ = frame_->pts;
        // 转换成毫秒
        if (time_base_)
          cur_ms_ = av_rescale_q(frame_->pts, *time_base_, {1, 1000});
        Next(av_frame_clone(frame_));
        MSleep(1);
      }
      auto list = decode_.End();
      decode_frame_count_ += list.size();
      for (auto frame : list) {
        cur_pts_ = frame->pts;
        // 转换成毫秒
        if (time_base_)
          cur_ms_ = av_rescale_q(frame->pts, *time_base_, {1, 1000});
        Next(frame);
        MSleep(1);
      }
    }
    {
    unique_lock<mutex> lock(mux_);
    if(frame_)
        av_frame_free(&frame_);
    }
    cout << endl
         << "decode frame count(" << recv_packet_count_ << "->"
         << decode_frame_count_ << ")" << endl
         << flush;
    end_decode_ = true;
}
