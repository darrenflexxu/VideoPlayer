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
#include <QWidget>
class XDecodeTask;
class XDemuxTask;
class XVideoView;
class XCameraWidget :public QWidget
{
    Q_OBJECT

public:
    XCameraWidget(QWidget* p=nullptr);
    
    //拖拽进入
    void dragEnterEvent(QDragEnterEvent* e) override;

    //拖拽松开
    void dropEvent(QDropEvent* e) override;

    //渲染
    void paintEvent(QPaintEvent* p);

    //打开rtsp 开始解封装 解码
    bool Open(const char* url);


    //渲染视频
    void Draw();

    //清理资源
    ~XCameraWidget();
private:
    XDecodeTask* decode_ = nullptr;
    XDemuxTask* demux_ = nullptr;
    XVideoView* view_ = nullptr;
};

