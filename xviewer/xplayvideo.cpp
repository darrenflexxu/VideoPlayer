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

#include "xplayvideo.h"
void XPlayVideo::timerEvent(QTimerEvent* ev)
{
    player.Update();
    //if (!view_)return;
    //auto f = decode_.GetFrame();
    //if (!f)return;
    //view_->DrawFrame(f);
    //XFreeFrame(&f);
}
void XPlayVideo::Close()
{
    player.Stop();
    ////关闭上次数据
    //demux_.Stop();
    //decode_.Stop();
    //if (view_)
    //{
    //    view_->Close();
    //    delete view_;
    //    view_ = nullptr;
    //}
}

void XPlayVideo::closeEvent(QCloseEvent* ev)
{
    Close();
}
bool XPlayVideo::Open(const char* url)
{
    if (!player.Open(url, (void*)winId()))
        return false;
    player.Start();
    startTimer(10);

    //if (!demux_.Open(url)) //解封装
    //{
    //    return false;
    //}
    //auto vp = demux_.CopyVideoPara();
    //if (!vp)
    //    return false;
    //if (!decode_.Open(vp->para))//解码
    //{
    //    return false;
    //}
    //demux_.set_next(&decode_);
    //    
    //if (!view_)
    //    view_ = XVideoView::Create();
    //view_->set_win_id((void*)winId());
    //if (!view_->Init(vp->para)) //SDL渲染
    //    return false;
    //demux_.set_syn_type(XSYN_VIDEO);
    //demux_.Start();
    //decode_.Start();
    return true;
}
XPlayVideo::XPlayVideo(QWidget *parent)
    : QDialog(parent)
{
    ui.setupUi(this);

}

XPlayVideo::~XPlayVideo()
{
    Close();
}
