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

//兼容Linux  _WIN32 windows 32为和64位
#ifdef _WIN32
#ifdef XCODEC_EXPORTS
#define XCODEC_API __declspec(dllexport)
#else
#define XCODEC_API __declspec(dllimport)
#endif
#else
#define XCODEC_API
#endif

#include <thread>
#include <iostream>
#include <mutex>
#include <list>
struct AVPacket;
struct AVCodecParameters;
struct AVRational;
struct AVFrame;
struct AVCodecContext;




//日志级别 DEBUG INFO ERROR FATAL
enum XLogLevel
{
    XLOG_TYPE_DEBUG,
    XLOG_TYPE_INFO,
    XLOG_TPYE_ERROR,
    XLOG_TYPE_FATAL
};
#define LOG_MIN_LEVEL XLOG_TYPE_DEBUG
#define XLOG(s,level) \
    if(level>=LOG_MIN_LEVEL) \
    std::cout<<level<<":"<<__FILE__<<":"<<__LINE__<<":\n"\
    <<s<<std::endl;
#define LOGDEBUG(s) XLOG(s,XLOG_TYPE_DEBUG)
#define LOGINFO(s) XLOG(s,XLOG_TYPE_INFO)
#define LOGERROR(s) XLOG(s,XLOG_TPYE_ERROR)
#define LOGFATAL(s) XLOG(s,XLOG_TYPE_FATAL)


XCODEC_API void MSleep(unsigned int ms);

//获取当前时间戳 毫秒
XCODEC_API long long NowMs();

XCODEC_API void XFreeFrame(AVFrame** frame);

XCODEC_API void PrintErr(int err);

//根据时间基数计算
XCODEC_API long long XRescale(long long pts,
    AVRational* src_time_base,
    AVRational* des_time_base);
class XCODEC_API XThread
{
public:
    //启动线程
    virtual void Start();

    //停止线程（设置退出标志，
    virtual void Stop();

    //等待线程退出
    virtual void Wait();

    //执行任务 需要重载
    virtual void Do(AVPacket* pkt) {}

    //传递到下一个责任链函数
    virtual void Next(AVPacket* pkt)
    {
        std::unique_lock<std::mutex> lock(m_);
        if (next_)
            next_->Do(pkt);

    }

    //设置责任链下一个节点（线程安全）
    void set_next(XThread* xt)
    {
        std::unique_lock<std::mutex> lock(m_);
        next_ = xt;
    }
protected:
    
    //线程入口函数
    virtual void Main() = 0;

    //标志线程退出
    bool is_exit_ = false;
    
    //线程索引号
    int index_ = 0;


private:
    std::thread th_;
    std::mutex m_;
    XThread *next_ = nullptr;//责任链下一个节点

};
class XTools
{
};



//音视频参数
class XCODEC_API XPara
{
public:
    AVCodecParameters* para = nullptr;  //音视频参数
    AVRational* time_base = nullptr;    //时间基数

    //创建对象
    static XPara* Create();
    ~XPara();
private:
    //私有是禁止创建栈中对象
    XPara();
};


/// <summary>
/// 线程安全avpacket list
/// </summary>
class XCODEC_API XAVPacketList
{
public:
    AVPacket* Pop();
    void Push(AVPacket* pkt);
    int Size();
    void Clear();
private:
    std::list<AVPacket*> pkts_;

    int max_packets_ = 1000;//最大列表数量，超出清理
    std::mutex mux_;
};