#pragma once
#include <QWidget>
#include <QMutex>
#include <atomic>
#include "TimelineModel.h"
#include "TimelineWidget.h"
#include "TimelinePlayer.h"
#include "xcomposite_exporter.h"

class QPushButton;
class QLabel;
class QSlider;
class QProgressDialog;
class XQtOpenGLWidget;

//视频编辑器主窗口: 预览 + 时间轴 + 播放控制 + 导出
class XVideoEditorWindow : public QWidget
{
    Q_OBJECT
public:
    explicit XVideoEditorWindow(QWidget* parent = nullptr);
    //导入文件(自动补全源时长), 供argv与按钮共用
    bool importFiles(const QStringList& files);

protected:
    void timerEvent(QTimerEvent* ev) override;

private:
    void onFrameTimer();
    void addClip(const QString& url);
    void onAddClicked();
    void togglePlay();
    void doExport();
    void exportWorker(const QString& out, XCompositeExporter::AudioOpts ao);
    void onExportCanceled();
    void updateLabels();

    TimelineModel model_;
    TimelineWidget* timeline_ = nullptr;
    TimelinePlayer player_;
    XQtOpenGLWidget* preview_ = nullptr;
    int frame_timer_ = 0;   //高频拉帧定时器(仅播放时有效)

    QPushButton* play_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* zoom_in_btn_ = nullptr;
    QPushButton* zoom_out_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* time_overlay_ = nullptr;   //预览画面上的时间水印
    QSlider* volume_slider_ = nullptr;

    std::atomic<bool> exporting_{false};
    std::atomic<bool> export_done_{false};
    bool export_ok_ = false;
    QString export_msg_;
    QMutex export_mutex_;
    //导出进度对话框(UI线程)与工作线程之间的状态
    QProgressDialog* export_dialog_ = nullptr;
    std::atomic<bool> export_cancel_{false};
    std::atomic<float> export_progress_{0.0f};
    //导出音频设置(对话框记住上次选择)
    int exp_sample_rate_ = 44100;
    int exp_channels_ = 2;
    int exp_bitrate_ = 128000;
    bool exp_denoise_ = true;
    //导入文件对话框记住上次目录
    QString last_import_dir_;
};