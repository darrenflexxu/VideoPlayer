#pragma once
#include <QString>
#include <vector>

//片段类型: 决定该片段由哪条渲染/混合管线处理
enum class ClipType
{
    VIDEO,      //视频画面(主画面/画中画)
    AUDIO,      //音频(背景音乐等, 只混声音不显示画面)
    SUBTITLE,   //字幕(透明文字叠加层)
    STICKER,    //贴纸(PNG 图片叠加层)
};

//片段在画布上的矩形(归一化坐标 0~1, 原点在左上角)
struct ClipRect
{
    double x = 0.0;
    double y = 0.0;
    double w = 1.0;
    double h = 1.0;
};

//时间轴上的一个片段
struct Clip
{
    ClipType type = ClipType::VIDEO;
    QString url;                //源文件
    long long source_start_ms = 0;   //源截取起点(毫秒)
    long long source_end_ms = 0;     //源截取终点(毫秒), 0=到文件尾
    long long source_total_ms = 0;   //源文件原片总长(毫秒), 剪切时不可调整
    long long pos_ms = 0;            //片段在时间轴上的偏移(毫秒)
    int trackIndex = 0;              //所在轨道(层索引, 越大越靠上)
    ClipRect rect;                   //画面矩形(仅视频/贴纸/字幕有意义)
    double alpha = 1.0;              //透明度 0~1
    QString text;                    //字幕文本(仅SUBTITLE有意义)

    long long duration_ms() const
    {
        return source_end_ms > source_start_ms ? source_end_ms - source_start_ms : 0;
    }

    //原片总长(优先使用记录值, 旧数据回退到截取终点); 剪切时保持固定
    long long source_length_ms() const
    {
        long long t = source_total_ms > 0 ? source_total_ms
                                          : (source_end_ms > source_start_ms ? source_end_ms
                                                                             : duration_ms());
        return t > 0 ? t : duration_ms();
    }

    //该片段在 timeline_ms 时刻是否处于激活区间
    bool activeAt(long long timeline_ms) const
    {
        return timeline_ms >= pos_ms && timeline_ms < pos_ms + duration_ms();
    }
};

//逻辑轨道: 相同类型内容的层容器, index 即 Z 序(越大越靠上)
struct Track
{
    int index = 0;
    ClipType type = ClipType::VIDEO;
    bool visible = true;
    bool muted = false;
    double volume = 1.0;   //轨道音量(仅音频)
    QString name;
};

//时间轴数据模型(Model): 多轨道分层, 同一时刻可叠加多个片段
class TimelineModel
{
public:
    //添加一条轨道(索引即Z序); 已存在相同索引时按类型/名称更新
    void addTrack(int index, ClipType type, const QString& name);
    const std::vector<Track>& tracks() const { return tracks_; }
    int trackCount() const { return (int)tracks_.size(); }
    const Track* track(int index) const;

    //通用添加片段(可指定类型/轨道/矩形/透明度)
    void addClip(const Clip& c);
    //兼容旧接口: 追加全屏视频片段到指定轨道末尾(trackIndex 默认0)
    void addClip(const QString& url, long long src_start, long long src_end,
                 int trackIndex = 0);
    const std::vector<Clip>& clips() const { return clips_; }
    int clipCount() const { return (int)clips_.size(); }
    bool empty() const { return clips_.empty(); }
    Clip& clip(int i) { return clips_[i]; }
    const Clip& clip(int i) const { return clips_[i]; }

    //包含 timeline_ms 的片段索引(按时间轴偏移, 优先最前面的), 找不到返回-1
    //兼容旧接口: 逻辑上是轨道0上的全屏视频序列, 用于旧顺序播放器
    int clipAt(long long timeline_ms, long long* local_ms = nullptr) const;
    //时间轴总时长(所有轨道最后一个片段的终点)
    long long total_ms() const;
    //某时刻所有激活片段, 按Z序排序(底层在前, 上层在后): 合成器按序绘制即得正确叠加
    std::vector<const Clip*> getActiveClips(long long timeline_ms) const;
    //某片段在其轨道上应放置的时间轴位置(轨道末尾); 无该轨道时返回0
    long long nextPosition(int trackIndex) const;
    void clear() { clips_.clear(); tracks_.clear(); }

private:
    std::vector<Track> tracks_;
    std::vector<Clip> clips_;
};