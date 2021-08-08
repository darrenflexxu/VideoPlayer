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
#include "xdemux.h"
enum XSYN_TYPE
{
    XSYN_NONE = 0,  //不做同步
    XSYN_VIDEO = 1, //根据视频同步，不处理音频
};
class XCODEC_API XDemuxTask :public XThread
{
public:
    int audio_index() { return demux_.audio_index(); }
    int video_index() { return demux_.video_index(); }

    void Main();
    /// <summary>
    /// 打开解封装
    /// </summary>
    /// <param name="url">rtsp地址</param>
    /// <param name="timeout_ms">超时时间 单位毫秒</param>
    /// <returns></returns>
    bool Open(std::string url,int timeout_ms = 1000);

    //复制视频参数
    std::shared_ptr<XPara> CopyVideoPara()
    {
        return demux_.CopyVideoPara();
    }
    std::shared_ptr<XPara> CopyAudioPara()
    {
        return demux_.CopyAudioPara();
    }
    void set_syn_type(XSYN_TYPE t) { syn_type_ = t; }

    void Stop();
private:
    XDemux demux_;
    std::string url_;
    int timeout_ms_ = 0;//超时时间
    XSYN_TYPE syn_type_ = XSYN_NONE;
};

