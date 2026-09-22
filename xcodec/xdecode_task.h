#pragma once
#include <atomic>
#include "xdecode.h"
class XCODEC_API XDecodeTask :public XThread
{
public:
    /// <summary>
    /// 打开解码器
    /// </summary>
    bool Open(AVCodecParameters* para);

    //责任链处理函数
    void Do(AVPacket* pkt) override; 

    //线程主函数
    void Main() override;

    // 线程安全，返回当前需要渲染的AVFrame，如果没有返回nullptr
    // need_view_控制渲染
    // 返回结果需要用 XFreeFrame 释放
    AVFrame* GetFrame();

    bool IsVideoFinish();
    void ClearFinish();

    //后台预热: 解码出第一帧后自身暂停(用于无缝跨片段切换)
    void set_preroll_one(bool v)
    {
        preroll_ = v;
        preroll_paused_ = false;
    }
    bool preroll_paused() const { return preroll_paused_; }

    void set_stream_index(int i) { stream_index_ = i; }
    int stream_index() { return stream_index_; }

    void set_frame_cache(bool is) { frame_cache_ = is; }
    bool is_open() { return is_open_; }

    void set_gpu_decode(bool gpu) { gpu_decode_ = gpu; }
    void set_gpu_direct_render(bool gpu_render) { gpu_direct_render_ = gpu_render; }

    //设置同步时间
    void set_syn_pts(long long p) { syn_pts_ = p; }
    void set_block_size(int s) { block_size_ = s; }

    //seek目标位置(毫秒), 定位后丢弃该时间之前的帧; <0表示无
    //同时把当前进度置为目标值, 避免UI进度条回跳到旧位置
    void set_seek_target_ms(long long ms)
    {
        seek_target_ms_ = ms;
        if (ms >= 0)
            cur_ms_ = ms;
    }
    long long seek_target_ms() { return seek_target_ms_; }
    /// <summary>
    /// 清理缓存
    /// </summary>
    void Clear();

    void Stop();

    //当前播放位置的毫秒
    long long cur_ms() { return cur_ms_; };

    void set_time_base(AVRational* time_base);

    AVCodecContext* GetCodecContext() const;

    bool EndOfDecode();

    bool ignoreMaxPkts(bool ignore);

    int get_Current_decode_frame_count();

    // 实际使用的硬件加速解码状态(失败回退软件后为 false)
    bool gpu_used() { return gpu_used_; }
    // 解码器收到的数据包数量
    int recv_packet_count() { return recv_packet_count_; }
    // 解码器名称(如 h264/libx264), 未打开返回空
    const char* decoder_name();

private:
    long long cur_pts_ = -1;//当前解码到的pts（以解码数据为准）
    AVRational* time_base_ = nullptr;
    long long cur_ms_ = 0;  //当前播放位置的毫秒
    std::atomic<long long> seek_target_ms_{-1}; //seek目标(毫秒), 解码侧丢弃目标前的帧
    int block_size_ = 0;    //阻塞大小
    long long syn_pts_ = -1;//同步时间 -1不同步
    bool is_open_ = false;
    int stream_index_ = 0;
    std::mutex mux_;
    XDecode decode_;
    XAVPacketList pkt_list_;
    AVFrame* frame_ = nullptr;      //解码后存储
    bool need_view_ = false;        //是否需要渲染，每帧只渲染一次，通过GetFrame
    std::list<AVFrame*> frames_;    //存储音频缓冲
    bool frame_cache_ = false;      //是否缓冲frame队列
    bool gpu_decode_ = false;
    bool gpu_direct_render_ = false;
    bool gpu_used_ = false;
    int decode_frame_count_ = 0;
    bool end_decode_ = false;
    int recv_packet_count_ = 0;
    std::atomic<bool> preroll_ {false};        // 预热模式: 出第一帧即暂停
    std::atomic<bool> preroll_paused_ {false}; // 预热已完成(已解析第一帧)
};

