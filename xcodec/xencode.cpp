#include "predefine_header.h"

using namespace std;

//////////////////////////////////////////////////////////////
/// 编码数据 线程安全 每次新创建AVPacket
/// @para frame 空间由用户维护
/// @return 失败范围nullptr 返回的AVPacket用户需要通过av_packet_free 清理
AVPacket* XEncode::Encode(const AVFrame* frame)
{
    if (!frame)return nullptr;
    unique_lock<mutex>lock(mux_);
    if (!c_)return nullptr;
    av_frame_make_writable((AVFrame*)frame);
    //发送到编码线程
    auto re = avcodec_send_frame(c_, frame);
    if (re != 0) {
      return nullptr;
    }
    auto pkt = av_packet_alloc();
    //接收编码线程数据
    re = avcodec_receive_packet(c_, pkt);
    if (re == 0)
    {
        return pkt;
    }
    av_packet_free(&pkt);
    if (re == AVERROR(EAGAIN) || re == AVERROR_EOF)
    {
        return nullptr;
    }
    if (re < 0)
    {
        PrintErr(re);
    }
    return nullptr;

}

bool XEncode::Send(AVFrame* frame) {
  if (!frame)
    return false;
  unique_lock<mutex> lock(mux_);
  if (!c_)
    return false;  // 失败不释放frame, 由调用方决定重试或放弃
  av_frame_make_writable(frame);
  auto re = avcodec_send_frame(c_, frame);
  if (re != 0) {
    // 失败(常见为EAGAIN输入缓冲满)不释放frame, 由调用方重试保持帧序,
    // 避免在qsv等异步硬件编码器上静默丢帧
    return false;
  }
  av_frame_free(&frame);
  return true;
}

bool XEncode::Recv(AVPacket* pkt) {
  if (!pkt) {
    return false;
  }
  unique_lock<mutex> lock(mux_);
  if (!c_)
    return false;
  auto re = avcodec_receive_packet(c_, pkt);
  if (re == 0) {
    return true;
  }
  return false;
}

//////////////////////////////////////////////////////////////
//返回所有编码缓存中AVPacket
std::vector<AVPacket*> XEncode::End()
{
    std::vector<AVPacket*> res;
    unique_lock<mutex>lock(mux_);
    if (!c_)return res;
    auto re = avcodec_send_frame(c_, NULL); //发送NULL 获取缓冲
    if (re != 0)return res;
    while (re >= 0)
    {
        auto pkt = av_packet_alloc();
        re = avcodec_receive_packet(c_, pkt);
        if (re != 0)
        {
            av_packet_free(&pkt);
            break;
        }
        res.push_back(pkt);
    }
    return res;
}
