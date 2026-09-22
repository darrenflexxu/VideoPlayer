#pragma once
#include <string>
#include <vector>

enum class XClipType { VIDEO, AUDIO, STICKER, SUBTITLE };

struct XClipRect {
    double x = 0.0, y = 0.0, w = 1.0, h = 1.0;
};

struct XExportClip {
    XClipType type = XClipType::VIDEO;
    std::string url;                //源文件路径
    long long source_start_ms = 0;  //源截取起点
    long long source_end_ms = 0;    //源截取终点, 0=到尾
    long long pos_ms = 0;           //时间轴偏移
    int track_index = 0;            //轨道索引(Z序)
    XClipRect rect;                 //画面矩形
    double alpha = 1.0;             //透明度
    std::string text;               //字幕文本(SUBTITLE用)

    long long duration_ms() const {
        return source_end_ms > source_start_ms ? source_end_ms - source_start_ms : 0;
    }
    bool activeAt(long long timeline_ms) const {
        return timeline_ms >= pos_ms && timeline_ms < pos_ms + duration_ms();
    }
};

struct XExportModel {
    std::vector<XExportClip> clips;

    long long total_ms() const {
        long long mx = 0;
        for (auto& c : clips) {
            long long end = c.pos_ms + c.duration_ms();
            if (end > mx) mx = end;
        }
        return mx;
    }

    //返回 timeline_ms 时刻所有激活片段, 按 track_index 升序(底层在前)
    std::vector<const XExportClip*> getActiveClips(long long timeline_ms) const {
        std::vector<const XExportClip*> act;
        for (const auto& c : clips)
            if (c.activeAt(timeline_ms))
                act.push_back(&c);
        std::stable_sort(act.begin(), act.end(),
                         [](const XExportClip* a, const XExportClip* b) {
                             return a->track_index < b->track_index;
                         });
        return act;
    }
};