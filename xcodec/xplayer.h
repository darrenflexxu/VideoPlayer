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
#include "xtools.h"
#include "xdemux_task.h"
#include "xdecode_task.h"
#include "xvideo_view.h"

class XCODEC_API XPlayer :public XThread
{
public:
    //回调接收音视频包
    void Do(AVPacket* pkt) override;
    
    //打开音视频 初始化播放和渲染
    bool Open(const char* url, void* winid);
    void Stop();
    
    //主线程 处理同步
    void Main()override;
    
    //开启 解封装 音视频解码 和 处理同步的线程
    void Start();
    

    //渲染视频 播放音频
    void Update();
protected:
    XDemuxTask demux_;              //解封装
    XDecodeTask audio_decode_;      //音频解码
    XDecodeTask video_decode_;      //视频解码
    XVideoView* view_ = nullptr;    //视频渲染
};

