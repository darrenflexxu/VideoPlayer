#include "predefine_header.h"

extern "C" {
#include <libavutil/time.h>
}

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

    // 收下当前已就绪的包(到EAGAIN为止)
    auto drain_ready = [&]() {
        for (;;) {
            auto pkt = av_packet_alloc();
            auto re = avcodec_receive_packet(c_, pkt);
            if (re == 0) {
                res.push_back(pkt);
                continue;
            }
            av_packet_free(&pkt);
            return;
        }
    };
    drain_ready();

    // 冲刷编码器。send(NULL)在QSV等异步编码器上可能返回EAGAIN
    // (硬件仍有在途帧), 若就此放弃会静默丢尾帧(实测90帧只剩63),
    // 需交替收包腾出缓冲后重试send(NULL)。
    int retry = 0;
    while (retry < 100) {
        auto re = avcodec_send_frame(c_, NULL);
        if (re == 0) break;
        if (re == AVERROR(EAGAIN)) {
            drain_ready();
            ++retry;
            continue;
        }
        break;  // 其他错误放弃
    }

    // 冲刷后持续收包: 异步编码器返回EAGAIN可能是暂态(硬件在途),
    // 短暂等待直到EOF/出错/超时, 确保所有帧都取回
    int idle = 0;
    for (;;) {
        auto pkt = av_packet_alloc();
        auto re = avcodec_receive_packet(c_, pkt);
        if (re == 0) {
            res.push_back(pkt);
            idle = 0;
            continue;
        }
        av_packet_free(&pkt);
        if (re == AVERROR(EAGAIN)) {
            if (++idle > 2000) break;  // ~2s仍无新包, 视为冲刷完成
            av_usleep(1000);
            continue;
        }
        break;  // EOF或其他错误
    }
    return res;
}
