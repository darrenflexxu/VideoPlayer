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

#include "xcamera_record.h"
#include "xdemux_task.h"
#include "xmux_task.h"
#include <chrono>
#include <iomanip>
#include <sstream>
using namespace std;
using namespace chrono;

/// 生成存储的视频文件名
static std::string GetFileName(string path)
{
    stringstream ss;
    //获取当前时间
    auto t = system_clock::to_time_t(system_clock::now());
    auto time_str = put_time(localtime(&t), "%Y_%m_%d_%H_%M_%S");
    ss << path << "/" << "cam_"<<time_str<<".mp4";
    return ss.str();
}

void XCameraRecord::Main()
{
    XDemuxTask demux;
    XMuxTask mux;
    if (rtsp_url_.empty())
    {
        LOGERROR("open rtsp url failed!");
        return;
    }

    //自定重连
    while (!is_exit_)
    {
        if (demux.Open(rtsp_url_)) //最坏情况阻塞1秒
        {
            break;
        }
        MSleep(3000);
        continue;
    }
    //音视频参数
    auto vpara = demux.CopyVideoPara();
    if (!vpara)
    {
        LOGERROR("demux.CopyVideoPara failed!");
        //需要考虑demux 资源释放的问题
        demux.Stop();
        return;
    }
    //启动解封装线程，提前启动，防止超时
    demux.Start();

    auto apara = demux.CopyAudioPara();

    AVCodecParameters* para = nullptr;  //音频参数
    AVRational* timebase = nullptr;     //音频时间基数
    if (apara)
    {
        para = apara->para;
        timebase = apara->time_base;
    }

    //打开了封装
    if (!mux.Open(GetFileName(save_path_).c_str(),
        vpara->para, vpara->time_base,//视频参数
        para, timebase))            //音频参数
    {
        LOGERROR("mux.Open rtsp_url_ failed!");
        demux.Stop();
        mux.Stop();
        return;
    }
    demux.set_next(&mux);
    mux.Start();

    //当前时间
    auto cur = NowMs();

    while (!is_exit_)
    {
        //定时创建新的文件
        if (NowMs() - cur > file_sec_ * 1000)
        {
            cur = NowMs();
            mux.Stop(); //停止存储，写入索引
            //打开了封装
            if (!mux.Open(GetFileName(save_path_).c_str(),
                vpara->para, vpara->time_base,//视频参数
                para, timebase))            //音频参数
            {
                LOGERROR("mux.Open rtsp_url_ failed!");
                demux.Stop();
                mux.Stop();
                return;
            }
            mux.Start();
        }

        MSleep(10);
    }
    
    mux.Stop();
    demux.Stop();
}