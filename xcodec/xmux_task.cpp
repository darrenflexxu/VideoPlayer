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

#include "xmux_task.h"
extern "C"
{
#include <libavformat/avformat.h>
}
using namespace std;
void XMuxTask::Do(AVPacket* pkt)
{
    pkts_.Push(pkt);
    Next(pkt);
}
void XMuxTask::Main()
{
    xmux_.WriteHead();

    //找到关键帧
    while (!is_exit_)
    {
        unique_lock<mutex> lock(mux_);
        auto pkt = pkts_.Pop();
        if (!pkt)
        {
            MSleep(1);
            continue;
        }
        if(pkt->stream_index == xmux_.video_index()
            && pkt->flags & AV_PKT_FLAG_KEY) //关键帧
        {
            xmux_.Write(pkt);
            av_packet_free(&pkt);
            break;
        }
        //丢掉非视频关键帧
        av_packet_free(&pkt);
    }


    while (!is_exit_)
    {
        unique_lock<mutex> lock(mux_);
        auto pkt = pkts_.Pop();
        if (!pkt)
        {
            MSleep(1);
            continue;
        }

        xmux_.Write(pkt);
        cout << "W"<< flush;

        av_packet_free(&pkt);
    }
    xmux_.WriteEnd();
    xmux_.set_c(nullptr);
}
bool XMuxTask::Open(const char* url,
    AVCodecParameters* video_para,
    AVRational* video_time_base,
    AVCodecParameters* audio_para ,
    AVRational* audio_time_base 
)
{
    auto c = xmux_.Open(url,video_para,audio_para);
    if (!c)return false;
    xmux_.set_c(c);
    xmux_.set_src_video_time_base(video_time_base);
    xmux_.set_src_audio_time_base(audio_time_base);
    return true;
}