#include "TimelineModel.h"
#include <algorithm>

void TimelineModel::addClip(const QString& url, long long src_start, long long src_end)
{
    long long pos = 0;
    for (const Clip& c : clips_)
        pos = std::max(pos, c.pos_ms + c.duration_ms());
    Clip c;
    c.url = url;
    c.source_start_ms = src_start;
    c.source_end_ms = src_end;
    c.pos_ms = pos;
    clips_.push_back(c);
}

int TimelineModel::clipAt(long long timeline_ms, long long* local_ms) const
{
    for (int i = 0; i < (int)clips_.size(); ++i) {
        const Clip& c = clips_[i];
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