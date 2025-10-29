#pragma once
#include "xdemux_task.h"
#include "xdecode_task.h"
#include "xvideo_view.h"

class XCODEC_API XPlayer :public XThread
{
public:
    //回调接收音视频包
    void Do(AVPacket* pkt) override;
    
    //打开音视频 初始化播放和渲染
    bool Open(const char* url, void* winid);
    void Stop();
    
    //主线程 处理同步
    void Main()override;
    
    //开启 解封装 音视频解码 和 处理同步的线程
    void Start();
    
    bool IsFinish();

    void ClearFinish();

    //渲染视频 播放音频
    void Update();

    void SetSpeed(float s);

    //总时长 毫秒
    long long total_ms() { return total_ms_; }

    //当前播放的位置 毫秒
    long long pos_ms() {return pos_ms_;}

    //设置视频播放位置，毫秒
    bool Seek(long long ms);

    void Pause(bool is_pause) override;

    void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
    void set_gpu_direct_render_(bool gpu_render) { gpu_direct_render_ = gpu_render; }

protected:

    long long total_ms_ = 0;
    long long pos_ms_ = 0;
    XDemuxTask demux_;              //解封装
    XDecodeTask audio_decode_;      //音频解码
    XDecodeTask video_decode_;      //视频解码
    XVideoView* view_ = nullptr;    //视频渲染
    bool gpu_decode_ = false;
    bool gpu_direct_render_ = false;
};

