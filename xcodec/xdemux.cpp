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

#include "xdemux.h"
#include <iostream>
#include <thread>
#include "xtools.h"
using namespace std;
extern "C" { //指定函数是c语言函数，函数名不包含重载标注
//引用ffmpeg头文件
#include <libavformat/avformat.h>
}
void PrintErr(int err);
#define BERR(err) if(err!= 0){PrintErr(err);return 0;}
AVFormatContext* XDemux::Open(const char* url)
{
    AVFormatContext* c = nullptr;

    AVDictionary* opts = nullptr;
    //av_dict_set(&opts, "rtsp_transport", "tcp", 0);//传输媒体流为tcp协议，默认udp
    av_dict_set(&opts, "stimeout", "1000000", 0);//连接超时1秒

    //打开封装上下文
    auto re = avformat_open_input(&c, url, nullptr, &opts);
    if (opts)
        av_dict_free(&opts);
    BERR(re);
    //获取媒体信息
    re = avformat_find_stream_info(c, nullptr);
    BERR(re);
    //打印输入封装信息
    av_dump_format(c, 0, url, 0);

    return c;
}

bool XDemux::Read(AVPacket* pkt)
{
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    auto re = av_read_frame(c_, pkt);
    BERR(re);
    //计时 用于超时判断
    last_time_ = NowMs();
    return true;
}

bool XDemux::Seek(long long pts, int stream_index)
{
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    auto re = av_seek_frame(c_, stream_index, pts,
               AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);
    BERR(re);
    return true;
}