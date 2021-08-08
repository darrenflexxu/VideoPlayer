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
#include "xdecode.h"
class XCODEC_API XDecodeTask :public XThread
{
public:
    /// <summary>
    /// 打开解码器
    /// </summary>
    bool Open(AVCodecParameters* para);

    //责任链处理函数
    void Do(AVPacket* pkt) override; 

    //线程主函数
    void Main() override;

    // 线程安全，返回当前需要渲染的AVFrame，如果没有返回nullptr
    // need_view_控制渲染
    // 返回结果需要用 XFreeFrame 释放
    AVFrame* GetFrame();

    void set_stream_index(int i) { stream_index_ = i; }

    void set_frame_cache(bool is) { frame_cache_ = is; }
    bool is_open() { return is_open_; }

    //设置同步时间
    void set_syn_pts(long long p) { syn_pts_ = p; }
    void set_block_size(int s) { block_size_ = s; }

    void Stop();
private:
    int block_size_ = 0;  //阻塞大小
    long long syn_pts_ = -1;//同步时间 -1不同步
    bool is_open_ = false;
    int stream_index_ = 0;
    std::mutex mux_;
    XDecode decode_;
    XAVPacketList pkt_list_;
    AVFrame* frame_ = nullptr;      //解码后存储
    bool need_view_ = false;        //是否需要渲染，每帧只渲染一次，通过GetFrame
    std::list<AVFrame*> frames_;    //存储音频缓冲
    bool frame_cache_ = false;      //是否缓冲frame队列
};

