#pragma once
#include <thread>
#include <mutex>
#include <vector>
#include <QImage>
#include "xplayer.h"
#include "TimelineModel.h"

class XQtOpenGLWidget;

//时间轴播放控制器(Controller): 多轨道并行播放 + 画中画合成
// - 基片 = 播放头处最低轨(Z序底层)的激活视频片段, 负责SDL音频与全屏画面
// - 其余激活片段(视频/音频/贴纸/字幕)作叠加层, 按播放头位置按需打开播放器
// - 每个播放器音频注入XAudioPlay的不同混音源, 由混音器求和输出
class TimelinePlayer
{
public:
    TimelinePlayer() = default;
    ~TimelinePlayer();
    TimelinePlayer(const TimelinePlayer&) = delete;
    TimelinePlayer& operator=(const TimelinePlayer&) = delete;

    void setModel(TimelineModel* m) { model_ = m; }
    void setView(XQtOpenGLWidget* w);

    //从当前播放头继续播放
    bool play();
    void pause() { paused_ = true; pauseAll(); }
    bool paused() const { return paused_; }

    //定位到时间轴某个位置并开始播放
    bool seekTo(long long timeline_ms);

    //剪切/移动片段后重定位基片(防止被裁掉的源区间仍被播放)
    void resyncBase();

    //每帧调用: 渲染合成帧并处理片段边界切换
    void Update();

    long long pos_ms() const { return timeline_pos_ms_; }
    long long total_ms() const { return model_ ? model_->total_ms() : 0; }
    int activeIndex() const { return active_idx_; }
    void set_volume(float v);
    void set_speed(float s);

private:
    //主轨底层播放(轨0): 保留原有预热换装机制
    bool switchToBase(int idx);   //打开并开始第idx段(基片, 任意轨道最低层视频)
    void releaseBase();           //停掉并释放当前主播放器
    XPlayer* buildBasePlayer(const Clip& c, int audio_source = 0, bool owns_audio = true);
    void startPrefetch(int idx);      //后台预热下一段主播放器
    void joinPrefetch();              //回收并等待预热线程结束
    bool takePrefetched();            //把预热好的播放器换成当前主播放器
    int nextBaseClip(int idx) const;  //idx 之后(含)下一个视频片段索引, 无则-1
    int activeBaseClipIndex(long long ms) const;  //ms处最低轨激活的视频/音频片段索引, 无则-1

    //叠加层播放(轨>=1)
    struct XOverlayPlay {
        int clip_index = -1;   //模型clips_索引
        int track = 0;         //轨道index
        XPlayer* player = nullptr;
    };
    XPlayer* buildOverlayPlayer(const Clip& c);  //新建叠加层播放器(无view, 音频独立源)
    void destroyOverlay(int i);                  //停掉并释放
    void destroyOverlays();
    void reconcileOverlays();                    //按播放头收敛叠加层集合

    //静态叠加层: 贴纸(图片) / 字幕(文字)
    struct XStaticOverlay {
        int clip_index = -1;
        int track = 0;
        AVFrame* rgba_frame = nullptr; //owned RGBA 帧
    };
    void reconcileStatics();                    //按播放头收敛静态叠加层(贴纸/字幕)
    void destroyStatics();                      //释放所有静态层
    static AVFrame* loadRgbaFromQImage(const QImage& img); //QImage → RGBA帧
    AVFrame* staticFrameFor(const Clip& c) const; //按clip类型生成静态层RGBA帧(贴纸载图/字幕渲文)

    void pauseAll();

    XPlayer* player_ = nullptr;   //主轨播放器
    XPlayer* next_player_ = nullptr;  //后台预热的下一段主播放器
    std::thread prefetch_th_;         //预热线程
    std::mutex next_mtx_;             //保护 next_player_与prefetch_gen_
    int prefetch_gen_ = 0;            //代际: 用于作废过期预热
    std::vector<XOverlayPlay> overlays_;
    std::vector<XStaticOverlay> static_layers_;

    TimelineModel* model_ = nullptr;
    XQtOpenGLWidget* view_ = nullptr;
    int active_idx_ = -1;            //当前打开的轨0片段索引
    bool paused_ = true;
    long long timeline_pos_ms_ = 0;  //时间轴上的位置
};