#pragma once
#include "xexport_model.h"
#include "xencode_task.h"
#include "xmux_task.h"
#include "xdecode_task.h"
#include "xdemux_task.h"
#include "xresample.h"
#include "xfbo.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <map>
#include <functional>

class XCODEC_API XCompositeExporter {
public:
    struct VideoOpts {
        int width = 320;
        int height = 240;
        int fps = 25;
        std::string codec = "libx264";
        std::map<std::string, std::string> opts = {{"crf","18"},{"preset","medium"}};
    };
    struct AudioOpts {
        int sample_rate = 44100;
        int channels = 2;
        std::string codec = "aac";
        int bitrate = 128000;
        std::map<std::string, std::string> opts;
    };

    //回调: 应用层为贴纸/字幕提供 RGBA 帧(每帧调用, 返回 nullptr 表示无)
    using RgbaProvider = std::function<AVFrame*(const XExportClip*, long long timeline_ms)>;

    XCompositeExporter();
    ~XCompositeExporter();

    void setModel(const XExportModel& model) { model_ = model; }
    void setVideoOpts(const VideoOpts& v) { vopts_ = v; }
    void setAudioOpts(const AudioOpts& a) { aopts_ = a; }
    void setRgbaProvider(RgbaProvider cb) { rgba_provider_ = std::move(cb); }

    bool Start(const std::string& out_url);
    void Wait();
    void Stop();

    bool IsFinished() const { return finished_.load(); }
    bool HasError() const { return !error_.empty(); }
    std::string GetError() const { return error_; }
    float GetProgress() const { return progress_.load(); }

private:
    //单个片段解码器: 一个解封装分流到视频/音频解码器(均走 frame_cache 待取)
    //导出线程按时间轴顺序消费, 视频用 cur/next 前瞻取最近目标帧, 音频拉取后混音
    struct ClipDecoder {
        const XExportClip* clip = nullptr;
        std::shared_ptr<XPara> vp;   //视频流参数(含时间基), 无视频流为空
        std::shared_ptr<XPara> ap;   //音频流参数
        XDemuxTask demux;
        XDecodeTask video_dec;
        XDecodeTask audio_dec;

        //分包器: demux 只挂一个下游, 这里按流类型转发给音/视频解码器
        struct Router : XThread {
            ~Router() = default;   //不启动线程, 仅继承 XThread
            XDecodeTask* v = nullptr;
            XDecodeTask* a = nullptr;
            int v_idx = -1, a_idx = -1;
            void Main() override;   //不启动线程, 仅为继承 XThread
            void Do(AVPacket* pkt) override;  //按流路由 + 合成EOF标记
        } router;

        bool v_started = false;   //视频解码线程已启动
        bool a_started = false;   //音频解码线程已启动
        bool demux_started_ = false;   //解封装线程已启动

        //视频前瞻状态机: cur 为已取帧, next 为下一帧(首个 > cur 的帧)
        AVFrame* v_cur = nullptr;
        long long v_cur_ms = -1;   // v_cur 对应源毫秒
        AVFrame* v_next = nullptr;
        long long v_next_ms = -1;
        bool v_eof = false;        //源已读完, 无更多帧

        //音频混音缓冲(平面 float, 目标采样率), 内容在源时间轴绝对采样位置
        std::vector<float> abuf_l, abuf_r;
        long long abuf_abs = 0;    // abuf 首样本对应的源绝对采样位置
        bool a_eof = false;
        std::unique_ptr<XResample> a_rr;
    };

    void ExportThread(const std::string& out_url);
    bool InitEncoders();

    long long ClipIndex(const XExportClip* clip) const;
    ClipDecoder* GetDecoder(const XExportClip* clip);

    //取一帧(等待/背压控制), 返回帧需 av_frame_free, EOF 返回 nullptr
    AVFrame* ReadClipFrame(const XExportClip* clip, long long target_ms);
    void OpenVideoDecoder(ClipDecoder* d);
    void UpdateBackpressure(ClipDecoder* d);
    //弹出视频下一帧, false 表示源已读尽
    bool PopVideoFrame(ClipDecoder* d, AVFrame** out, long long* out_ms);

    //从片段音频缓冲中取 [need_lo, need_hi) 源采样 累加到左/右声道指针
    void PullAudio(const XExportClip* clip, long long need_lo, long long need_hi,
                   float* dstL, float* dstR);
    void OpenAudioDecoder(ClipDecoder* d);
    bool AppendAudioFrame(ClipDecoder* d);

    void CompositeAndEncodeVideo(int frame_idx, long long timeline_ms);
    void MixAndEncodeAudio(long long s0, long long s1);
    //结尾不足一帧的音频补零编码
    void FlushPendingAudio(int sr);
    void StopClipDecoder(ClipDecoder* d);

    XExportModel model_;
    VideoOpts vopts_;
    AudioOpts aopts_;
    RgbaProvider rgba_provider_;

    XFBO fbo_;
    std::unique_ptr<XEncodeTask> video_enc_;
    std::unique_ptr<XEncodeTask> audio_enc_;
    std::unique_ptr<XMuxTask> mux_;

    std::vector<std::unique_ptr<ClipDecoder>> decoders_;
    std::mutex decoders_mux_;

    AVCodecContext* mix_ctx_ = nullptr;  //混音目标参数(44100/stereo/FLTP), 不 open
    //音频待发缓冲: 混音按视频帧步进(~1765样本), 编码器要求固定1024, 这里拼切
    std::vector<float> pend_l_, pend_r_;
    long long pend_pts_ = 0;   //待发缓冲首样本的输出时间轴采样位置

    std::thread export_th_;
    std::atomic<bool> running_{false};
    std::atomic<bool> finished_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<float> progress_{0.0f};
    std::string error_;
    std::string out_url_;
};