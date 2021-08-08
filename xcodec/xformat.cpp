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

#include "xformat.h"
#include <iostream>
#include <thread>
#include "xtools.h"
using namespace std;
extern "C" { //指定函数是c语言函数，函数名不包含重载标注
//引用ffmpeg头文件
#include <libavformat/avformat.h>
}
//预处理指令导入库
#pragma comment(lib,"avformat.lib")
#pragma comment(lib,"avutil.lib")
using namespace std;

static int TimeoutCallback(void* para)
{
    auto xf = (XFormat*)para;
    if (xf->IsTimeout())return 1;//超时退出Read
    return 0; //正常阻塞
}

void XFormat::set_c(AVFormatContext* c)
{
    unique_lock<mutex> lock(mux_);
    if (c_) //清理原值
    {
        if (c_->oformat) //输出上下文
        {
            if (c_->pb)
                avio_closep(&c_->pb);
            avformat_free_context(c_);
        }
        else if (c_->iformat)  //输入上下文
        {
            avformat_close_input(&c_);
        }
        else
        {
            avformat_free_context(c_);
        }
    }
    c_ = c;
    if (!c_)
    {
        is_connected_ = false;
        return;
    }
    is_connected_ = true;

    //计时 用于超时判断
    last_time_ = NowMs();

    //设定超时处理回调
    if (time_out_ms_ > 0)
    {
        AVIOInterruptCB cb = { TimeoutCallback ,this };
        c_->interrupt_callback = cb;
    }

    //用于区分是否有音频或者视频流
    audio_index_ = -1; 
    video_index_ = -1;
    //区分音视频stream 索引
    for (int i = 0; i < c->nb_streams; i++)
    {
        //音频
        if (c->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_index_ = i;
            audio_time_base_.den = c->streams[i]->time_base.den;
            audio_time_base_.num = c->streams[i]->time_base.num;
        }
        else if (c->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_index_ = i;
            video_time_base_.den = c->streams[i]->time_base.den;
            video_time_base_.num = c->streams[i]->time_base.num;
            video_codec_id_ = c->streams[i]->codecpar->codec_id;
        }
    }
}

std::shared_ptr<XPara> XFormat::CopyAudioPara()
{
    int index = audio_index();
    shared_ptr<XPara> re;
    unique_lock<mutex> lock(mux_);
    if (index < 0 || !c_)return re;

    re.reset(XPara::Create());
    *re->time_base = c_->streams[index]->time_base;
    avcodec_parameters_copy(re->para, c_->streams[index]->codecpar);
    return re;
}

std::shared_ptr<XPara> XFormat::CopyVideoPara()
{
    int index = video_index();
    shared_ptr<XPara> re;
    unique_lock<mutex> lock(mux_);
    if (index < 0 || !c_)return re;

    re.reset(XPara::Create());
    *re->time_base = c_->streams[index]->time_base;
    avcodec_parameters_copy(re->para, c_->streams[index]->codecpar);
    return re;
}

bool XFormat::CopyPara(int stream_index, AVCodecContext* dts)
{
    unique_lock<mutex> lock(mux_);
    if (!c_)
    {
        return false;
    }
    if (stream_index<0 || stream_index>c_->nb_streams)
        return false;
    auto re = avcodec_parameters_to_context(dts, c_->streams[stream_index]->codecpar);
    if (re < 0)
    {
        return false;
    }
    return true;
}

/// <summary>
/// 复制参数 线程安全
/// </summary>
/// <param name="stream_index">对应c_->streams 下标</param>
/// <param name="dst">输出参数</param>
/// <returns>是否成功</returns>
bool XFormat::CopyPara(int stream_index, AVCodecParameters* dst)
{
    unique_lock<mutex> lock(mux_);
    if (!c_)
    {
        return false;
    }
    if (stream_index<0 || stream_index>c_->nb_streams)
        return false;
    auto re = avcodec_parameters_copy(dst, c_->streams[stream_index]->codecpar);
    if (re < 0)
    {
        return false;
    }
    return true;
}
//把pts dts duration 值转为毫秒
long long XFormat::RescaleToMs(long long pts, int index)
{
    unique_lock<mutex> lock(mux_);
    if (!c_ || index <0 || index > c_->nb_streams)return 0;
    auto in_timebase = c_->streams[index]->time_base;

    AVRational out_timebase = { 1,1000 };//输出timebase 毫秒
    return av_rescale_q(pts, in_timebase, out_timebase);
}
bool XFormat::RescaleTime(AVPacket* pkt, long long offset_pts, AVRational* time_base)
{
    if (!pkt || !time_base)return false;
    unique_lock<mutex> lock(mux_);
    if (!c_)return false;
    auto out_stream = c_->streams[pkt->stream_index];
    pkt->pts = av_rescale_q_rnd(pkt->pts - offset_pts, *time_base,
        out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
    );
    pkt->dts = av_rescale_q_rnd(pkt->dts - offset_pts, *time_base,
        out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX)
    );
    pkt->duration = av_rescale_q(pkt->duration, *time_base, out_stream->time_base);
    pkt->pos = -1;
    return true;
}
bool XFormat::RescaleTime(AVPacket* pkt, long long offset_pts, XRational time_base)
{
    AVRational in_time_base;
    in_time_base.num = time_base.num;
    in_time_base.den = time_base.den;
    return RescaleTime(pkt, offset_pts,&in_time_base);
}

//设定超时时间
void XFormat::set_time_out_ms(int ms)
{
    unique_lock<mutex> lock(mux_);
    this->time_out_ms_ = ms;
    //设置回调函数，处理超时退出
    if (c_)
    {
        AVIOInterruptCB cb = { TimeoutCallback ,this };
        c_->interrupt_callback = cb;
    }
}
