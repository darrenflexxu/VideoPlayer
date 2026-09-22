#pragma once
#include <QString>
#include <vector>

//时间轴上的一个片段
struct Clip
{
    QString url;             //源文件
    long long source_start_ms = 0;   //源截取起点(毫秒)
    long long source_end_ms = 0;     //源截取终点(毫秒), 0=到文件尾
    long long pos_ms = 0;            //片段在时间轴上的偏移(毫秒)

    long long duration_ms() const
    {
        return source_end_ms > source_start_ms ? source_end_ms - source_start_ms : 0;
    }
};

//时间轴数据模型(Model)
class TimelineModel
{
public:
    //追加片段, 位置接在当前最后一个片段之后
    void addClip(const QString& url, long long src_start, long long src_end);
    const std::vector<Clip>& clips() const { return clips_; }
    int clipCount() const { return (int)clips_.size(); }
    bool empty() const { return clips_.empty(); }

    //包含 timeline_ms 的片段索引(按时间轴偏移, 优先最前面的), 找不到返回-1
    int clipAt(long long timeline_ms, long long* local_ms = nullptr) const;
    //时间轴总时长(最后一个片段的终点)
    long long total_ms() const;
    void clear() { clips_.clear(); }
    Clip& clip(int i) { return clips_[i]; }

private:
    std::vector<Clip> clips_;
};