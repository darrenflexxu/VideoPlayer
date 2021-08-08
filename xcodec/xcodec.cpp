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

#include "xcodec.h"
#include <iostream>
using namespace std;
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
//预处理指令导入库
#pragma comment(lib,"avcodec.lib")
#pragma comment(lib,"avutil.lib")


//////////////////////////////////////////
/// 创建编码上下文
/// @para codec_id 编码器ID号，对应ffmpeg
/// @return 编码上下文 ,失败返回nullptr
AVCodecContext* XCodec::Create(int codec_id,bool isencode)
{
    //1 找到编码器
    AVCodec *codec = nullptr;
    if(isencode)
        codec = avcodec_find_encoder((AVCodecID)codec_id);
    else
        codec = avcodec_find_decoder((AVCodecID)codec_id);
    if (!codec)
    {
        cerr << "avcodec_find_encoder failed!" << codec_id << endl;
        return nullptr;
    }
    //创建上下文
    auto c = avcodec_alloc_context3(codec);
    if (!c)
    {
        cerr << "avcodec_alloc_context3 failed!" << codec_id << endl;
        return nullptr;
    }
    //设置参数默认值
    c->time_base = { 1,25 };
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->thread_count = 16;
    return c;
}

//////////////////////////////////////////
/// 设置对象的编码器上下文 上下文传递到对象中，资源由XEncode维护
/// 加锁 线程安全
/// @para c 编码器上下文 如果c_不为nullptr，则先清理资源
void XCodec::set_c(AVCodecContext* c)
{
    unique_lock<mutex>lock(mux_);
    if (c_)
    {
        avcodec_free_context(&c_);
    }
    this->c_ = c;
}


bool XCodec::SetOpt(const char* key, const char* val)
{
    unique_lock<mutex>lock(mux_);
    if (!c_)return false;
    auto re = av_opt_set(c_->priv_data, key, val, 0);
    if (re != 0)
    {
        cerr << "av_opt_set failed!";
        PrintErr(re);
    }
    return true;
}

bool XCodec::SetOpt(const char* key, int val)
{
    unique_lock<mutex>lock(mux_);
    if (!c_)return false;
    auto re = av_opt_set_int(c_->priv_data, key, val, 0);
    if (re != 0)
    {
        cerr << "av_opt_set failed!";
        PrintErr(re);
    }
    return true;
}

//////////////////////////////////////////////////////////////
/// 打开编码器 线程安全
bool XCodec::Open()
{
    unique_lock<mutex>lock(mux_);
    if (!c_)return false;
    auto re = avcodec_open2(c_, NULL, NULL);
    if (re != 0)
    {
        PrintErr(re);
        return false;
    }
    return true;
}


///////////////////////////////////////////////////////////////
//根据AVCodecContext 创建一个AVFrame，需要调用者释放av_frame_free
AVFrame* XCodec::CreateFrame()
{
    unique_lock<mutex>lock(mux_);
    if (!c_)return nullptr;
    auto frame = av_frame_alloc();
    frame->width = c_->width;
    frame->height = c_->height;
    frame->format = c_->pix_fmt;
    auto re = av_frame_get_buffer(frame, 0);
    if (re != 0)
    {
        av_frame_free(&frame);
        PrintErr(re);
        return nullptr;
    }
    return frame;
}