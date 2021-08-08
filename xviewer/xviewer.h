/*/*******************************************************************************
**                                                                            **
**                     Jiedi(China nanjing)Ltd.                               **
**	               创建：丁宋涛 夏曹俊，此代码可用作为学习参考                **
*******************************************************************************/

/*****************************FILE INFOMATION***********************************
**
** Project       :FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课

** Contact       : xiacaojun@qq.com
**  博客   : http://blog.csdn.net/jiedichina
**	视频课程 : 网易云课堂	http://study.163.com/u/xiacaojun		
			   腾讯课堂		https://jiedi.ke.qq.com/				
			   csdn学院               http://edu.csdn.net/lecturer/lecturer_detail?lecturer_id=961	
**             51cto学院              http://edu.51cto.com/lecturer/index/user_id-12016059.html	
** 			   老夏课堂		http://www.laoxiaketang.com 
**                              更多资料请在此网页下载            http://ffmpeg.club
**  FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课  课程群 ：639014264加入群下载代码和学员交流
**                           微信公众号  : jiedi2007
**		头条号	 : 夏曹俊
**
*****************************************************************************
//！！！！！！！！！FFmpeg 4.2 从基础实战-多路H265监控录放开发 实训课 课程  QQ群：639014264下载代码和学员交流*/

#pragma once

#include <QtWidgets/QWidget>
#include "ui_xviewer.h"
#include <QMenu>
class XViewer : public QWidget
{
    Q_OBJECT

public:
    XViewer(QWidget *parent = Q_NULLPTR);

    //鼠标事件 用于拖动窗口
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

    //窗口大小发生编码
    void resizeEvent(QResizeEvent* ev) override;
    //右键菜单
    void contextMenuEvent(QContextMenuEvent* event) override;

    //预览视频窗口
    void View(int count);

    //刷新左侧相机列表
    void RefreshCams();

    //编辑摄像机
    void SetCam(int index);

    //定时器渲染视频 回调函数
    void timerEvent(QTimerEvent* ev) override;
public slots:
    void MaxWindow();
    void NormalWindow();
    void View1();
    void View4();
    void View9();
    void View16();
    void AddCam();  //新增摄像机配置
    void SetCam();  //
    void DelCam();  //

    void StartRecord(); //开始全部摄像头录制
    void StopRecord();  //停止全部摄像头录制
    void Preview();//预览界面
    void Playback();//回放界面

    void SelectCamera(QModelIndex index);//选择摄像机
    void SelectDate(QDate date);        //选择日期
    void PlayVideo(QModelIndex index);  //选择时间播放视频
private:
    Ui::XViewerClass ui;
    QMenu left_menu_;
};
