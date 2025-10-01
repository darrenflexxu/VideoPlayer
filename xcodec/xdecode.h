#pragma once
#include "xcodec.h"
struct AVBufferRef;
class XCODEC_API XDecode :public XCodec
{
public:
    bool Send(const AVPacket* pkt);  //发送解码
    bool Recv(AVFrame* frame);       //获取解码
    std::vector<AVFrame*> End();    //获取缓存

    ////////////////////////////////////////////////////
    //// 初始化硬件加速  4 AV_HWDEVICE_TYPE_DXVA2
    bool InitHW();

    void set_gpu_direct_render(bool gpu_render) { gpu_direct_render_ = gpu_render; }

private:
    bool RecvFrame(AVFrame* frame);

    bool gpu_direct_render_ = false;
};

