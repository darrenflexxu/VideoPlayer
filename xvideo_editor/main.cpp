#include <QApplication>
#include <QSurfaceFormat>
#include <QStringList>
#include "XVideoEditorWindow.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
}

#include "xcomposite_exporter.h"
#include "xexport_model.h"
#include "xtools.h"

//临时自测: 合成导出(两片段画中画 + 贴纸), 验证复合导出管线
static int runExportDemo(const QStringList& args)
{
    if (args.size() < 5) {
        fprintf(stderr, "[demo-export] usage: --demo export <base.mp4> <pip.mp4>\n");
        return -1;
    }
    std::string url0 = args[3].toStdString();
    std::string url1 = args[4].toStdString();

    XExportModel model;
    {
        XExportClip base;
        base.type = XClipType::VIDEO;
        base.url = url0;
        base.source_start_ms = 0;
        base.source_end_ms = 3000;
        base.pos_ms = 0;
        base.track_index = 0;
        base.rect = {0.0, 0.0, 1.0, 1.0};
        model.clips.push_back(base);
    }
    {
        XExportClip pip;
        pip.type = XClipType::VIDEO;
        pip.url = url1;
        pip.source_start_ms = 0;
        pip.source_end_ms = 3000;
        pip.pos_ms = 0;
        pip.track_index = 1;
        pip.rect = {0.55, 0.55, 0.4, 0.4};
        model.clips.push_back(pip);
    }
    {
        //贴纸层: 应用层回调提供 160x160 洋红色 RGBA
        XExportClip st;
        st.type = XClipType::STICKER;
        st.url = "sticker";
        st.source_start_ms = 0;
        st.source_end_ms = 3000;
        st.pos_ms = 0;
        st.track_index = 2;
        st.rect = {0.05, 0.05, 0.4, 0.4};
        st.alpha = 1.0;
        model.clips.push_back(st);
    }

    XCompositeExporter ex;
    ex.setModel(model);
    XCompositeExporter::VideoOpts vo;
    vo.width = 320;
    vo.height = 240;
    vo.fps = 25;
    vo.codec = "libx264";
    ex.setVideoOpts(vo);
    XCompositeExporter::AudioOpts ao;
    ao.sample_rate = 44100;
    ao.channels = 2;
    ao.codec = "aac";
    ex.setAudioOpts(ao);
    ex.setRgbaProvider([](const XExportClip*, long long) -> AVFrame* {
        AVFrame* f = av_frame_alloc();
        if (!f) return nullptr;
        f->format = AV_PIX_FMT_RGBA;
        f->width = 160;
        f->height = 160;
        if (av_frame_get_buffer(f, 32) < 0) {
            av_frame_free(&f);
            return nullptr;
        }
        for (int y = 0; y < f->height; ++y) {
            unsigned char* p = f->data[0] + (size_t)y * f->linesize[0];
            for (int x = 0; x < f->width; ++x) {
                p[x * 4 + 0] = 255;
                p[x * 4 + 1] = 0;
                p[x * 4 + 2] = 254;
                p[x * 4 + 3] = 255;
            }
        }
        return f;
    });

    bool ok = ex.Start("Bin/export_pip.mp4");
    if (!ok) {
        fprintf(stderr, "[demo-export] start failed: %s\n", ex.GetError().c_str());
        return -1;
    }
    while (!ex.IsFinished()) {
        MSleep(50);
    }
    if (ex.HasError()) {
        fprintf(stderr, "[demo-export] FAILED: %s\n", ex.GetError().c_str());
        return -1;
    }
    fprintf(stderr, "[demo-export] OK -> Bin/export_pip.mp4 (%.1f%%)\n",
            ex.GetProgress() * 100.0f);
    return 0;
}

int main(int argc, char* argv[])
{
    //视频着色器使用 GLSL 330 core, 需请求 OpenGL 3.3+ 核心模式(macOS最高4.1)
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 1);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication a(argc, argv);

    QStringList args = a.arguments();
    if (args.size() >= 3 && args[1] == "--demo" && args[2] == "export") {
        return runExportDemo(args);
    }

    XVideoEditorWindow w;
    w.importFiles(args.mid(1));
    w.show();
    return a.exec();
}