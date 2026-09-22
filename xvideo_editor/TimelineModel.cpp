#include "TimelineModel.h"
#include <algorithm>

void TimelineModel::addTrack(int index, ClipType type, const QString& name)
{
    for (Track& t : tracks_) {
        if (t.index == index) {
            t.type = type;
            t.name = name;
            return;
        }
    }
    Track t;
    t.index = index;
    t.type = type;
    t.name = name;
    tracks_.push_back(t);
    std::sort(tracks_.begin(), tracks_.end(),
              [](const Track& a, const Track& b) { return a.index < b.index; });
}

const Track* TimelineModel::track(int index) const
{
    for (const Track& t : tracks_)
        if (t.index == index)
            return &t;
    return nullptr;
}

long long TimelineModel::nextPosition(int trackIndex) const
{
    long long pos = 0;
    for (const Clip& c : clips_) {
        if (c.trackIndex != trackIndex)
            continue;
        pos = std::max(pos, c.pos_ms + c.duration_ms());
    }
    return pos;
}

void TimelineModel::addClip(const Clip& c)
{
    //轨道0默认作为视频主轨, 若不存在则自动创建, 保证旧接口/旧视图行为一致
    if (trackCount() == 0) {
        addTrack(0, ClipType::VIDEO, "视频轨");
    }
    clips_.push_back(c);
}

void TimelineModel::addClip(const QString& url, long long src_start, long long src_end,
                            int trackIndex)
{
    Clip c;
    c.type = ClipType::VIDEO;
    c.url = url;
    c.source_start_ms = src_start;
    c.source_end_ms = src_end;
    c.trackIndex = trackIndex;
    c.rect = ClipRect{0.0, 0.0, 1.0, 1.0};
    c.alpha = 1.0;
    c.pos_ms = nextPosition(trackIndex);
    addClip(c);
}

int TimelineModel::clipAt(long long timeline_ms, long long* local_ms) const
{
    for (int i = 0; i < (int)clips_.size(); ++i) {
        const Clip& c = clips_[i];
        //旧语义: 只匹配轨道0的全屏视频(顺序播放序列)
        if (c.trackIndex != 0)
            continue;
        if (timeline_ms >= c.pos_ms && timeline_ms < c.pos_ms + c.duration_ms()) {
            if (local_ms) *local_ms = timeline_ms - c.pos_ms;
            return i;
        }
    }
    if (local_ms) *local_ms = 0;
    return -1;
}

long long TimelineModel::total_ms() const
{
    long long t = 0;
    for (const Clip& c : clips_)
        t = std::max(t, c.pos_ms + c.duration_ms());
    return t;
}

std::vector<const Clip*> TimelineModel::getActiveClips(long long timeline_ms) const
{
    std::vector<const Clip*> act;
    for (const Clip& c : clips_) {
        if (c.activeAt(timeline_ms))
            act.push_back(&c);
    }
    //Z序(绘制顺序): 轨道index从小到大(底层在前, 上层在后覆盖); 
    //同轨内保持插入顺序(稳定排序), 使重叠贴纸/字幕按加入顺序叠加
    std::stable_sort(act.begin(), act.end(),
                     [](const Clip* a, const Clip* b) {
                         return a->trackIndex < b->trackIndex;
                     });
    return act;
}