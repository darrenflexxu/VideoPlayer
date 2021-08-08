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

#include "xcamera_widget.h"
#include <QStyleOption>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDebug>
#include <QListWidget>
#include "xdemux_task.h"
#include "xdecode_task.h"
#include "xvideo_view.h"
#include "xcamera_config.h"
//渲染
void XCameraWidget::paintEvent(QPaintEvent* p)
{
    //渲染样式表
    QStyleOption opt;
    opt.init(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}
XCameraWidget::XCameraWidget(QWidget* p):QWidget(p)
{
    //接收拖拽
    this->setAcceptDrops(true);
}

//拖拽进入
void XCameraWidget::dragEnterEvent(QDragEnterEvent* e)
{
    //接收拖拽进入
    e->acceptProposedAction();
}
//渲染视频
void XCameraWidget::Draw()
{
    if (!demux_ || !decode_ || !view_)return;
    auto f = decode_->GetFrame();
    if (!f)return;
    view_->DrawFrame(f);
    XFreeFrame(&f);
}

//清理资源
XCameraWidget::~XCameraWidget()
{
    if (demux_)
    {
        demux_->Stop();
        delete demux_;
        demux_ == nullptr;
    }
    if (decode_)
    {
        decode_->Stop();
        delete decode_;
        decode_ == nullptr;
    }
    if (view_)
    {
        view_->Close();
        delete view_;
        view_ = nullptr;
    }
}

bool XCameraWidget::Open(const char* url)
{
    if (demux_)
        demux_->Stop();
    if (decode_)
        decode_->Stop();
    //打开解封装线程
    demux_ = new XDemuxTask();
    if (!demux_->Open(url))
    {
        return false;
    }
    //打开视频解码器线程
    decode_ = new XDecodeTask();
    auto para = demux_->CopyVideoPara();
    if (!decode_->Open(para->para))
    {
        return false;
    }
    //设定解码线程接收解封装数据
    demux_->set_next(decode_);

    //初始化渲染参数
    view_ = XVideoView::Create();
    view_->set_win_id((void*)winId());
    view_->Init(para->para);

    //启动解封装和解码线程
    demux_->Start();
    decode_->Start();
    return true;
}
//拖拽松开
void XCameraWidget::dropEvent(QDropEvent* e)
{
    //拿到url
    qDebug()<<e->source()->objectName();
    auto wid = (QListWidget*)e->source();
    qDebug() << wid->currentRow();
    auto cam = XCameraConfig::Instance()->GetCam(wid->currentRow());
    Open(cam.sub_url);

}