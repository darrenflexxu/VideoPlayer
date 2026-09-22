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

    //后台预热: 解码出第一帧后自暂停, 供跨片段无缝切换
    void set_preroll_one(bool v) { video_decode_.set_preroll_one(v); }
    bool preroll_ready() const { return video_decode_.preroll_paused(); }

    //渲染视频 播放音频
    void Update();

    void SetSpeed(float s);

    //设置音量 0.0~1.0
    void SetVolume(float volume);

    //总时长 毫秒
    long long total_ms() { return total_ms_; }

    //当前播放的位置 毫秒
    long long pos_ms() {return pos_ms_;}

    //设置视频播放位置，毫秒
    bool Seek(long long ms);

    void Pause(bool is_pause) override;

    void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
    void set_gpu_direct_render_(bool gpu_render) { gpu_direct_render_ = gpu_render; }

    //注入外部创建的渲染对象(如 UI 层的 QOpenGLWidget), 生命周期由调用方管理
    void set_view(XVideoView* v) { view_ = v; own_view_ = false; }

    //获取视频渲染对象
    XVideoView* view() { return view_; }

    //多播放器混音: 是否负责打开/关闭SDL音频设备(仅主播放器应打开设备)
    void set_owns_audio(bool own) { own_audio_ = own; }
    //多播放器混音: 本播放器音频注入XAudioPlay的源id(默认0)
    void set_audio_source(int id) { audio_source_ = id; }
    //叠加层播放器: 不创建/不渲染到view(画面由外部合成器逐层取走)
    void set_no_view(bool v) { no_view_ = v; }

    //取最新解码视频帧(引用复制), 不渲染到view, 供合成器逐层使用; 无新帧返回false
    bool GetVideoFrameCopy(AVFrame** out);

protected:

    long long total_ms_ = 0;
    long long pos_ms_ = 0;
    XDemuxTask demux_;              //解封装
    XDecodeTask audio_decode_;      //音频解码
    XDecodeTask video_decode_;      //视频解码
    XVideoView* view_ = nullptr;    //视频渲染
    bool own_view_ = false;         //view_ 是否由 XPlayer 创建(负责释放)
    bool gpu_decode_ = false;
    bool gpu_direct_render_ = false;
    int audio_source_ = 0;          //混音源id
    bool own_audio_ = true;         //是否负责SDL音频设备生命周期
    bool no_view_ = false;          //叠加层播放器: 不创建/不渲染view
};

