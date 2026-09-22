#pragma once
#include <QWidget>
#include <QMutex>
#include <atomic>
#include "TimelineModel.h"
#include "TimelineWidget.h"
#include "TimelinePlayer.h"

class QPushButton;
class QLabel;
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
    long long probeDurationMs(const QString& url);
    void addClip(const QString& url);
    void onAddClicked();
    void togglePlay();
    void doExport();
    void exportWorker(const QString& out);
    void updateLabels();

    TimelineModel model_;
    TimelineWidget* timeline_ = nullptr;
    TimelinePlayer player_;
    XQtOpenGLWidget* preview_ = nullptr;

    QPushButton* play_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* status_ = nullptr;

    std::atomic<bool> exporting_{false};
    std::atomic<bool> export_done_{false};
    bool export_ok_ = false;
    QString export_msg_;
    QMutex export_mutex_;
};