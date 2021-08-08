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
#include "xformat.h"
//////////////////////////////////////
/// 媒体封装

class XCODEC_API XMux :public XFormat
{
public:
    //////////////////////////////////////////////////
    //// 打开封装
    static AVFormatContext* Open(const char* url,
        AVCodecParameters* video_para = nullptr,
        AVCodecParameters* audio_para = nullptr
    );

    bool WriteHead();

    bool Write(AVPacket* pkt);

    bool WriteEnd();

    //音视频时间基础
    void set_src_video_time_base(AVRational* tb);
    void set_src_audio_time_base(AVRational* tb);

    ~XMux();
private:
    AVRational* src_video_time_base_ = nullptr;
    AVRational* src_audio_time_base_ = nullptr;

    long long begin_video_pts_ = -1;//原视频开始时间
    long long begin_audio_pts_ = -1;//原音频开始时间
};

