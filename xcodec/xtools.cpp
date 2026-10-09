#include "predefine_header.h"

#ifndef WIN32
#include <unistd.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <stdint.h>
#endif
#endif

using namespace std;

#include <chrono>

//根据时间基数计算
XCODEC_API long long XRescale(long long pts,
                              AVRational* src_time_base,
                              AVRational* des_time_base) {
    return av_rescale_q(pts, *src_time_base, *des_time_base);
}

void PrintErr(int err) {
    char buf[1024] = {0};
    av_strerror(err, buf, sizeof(buf) - 1);
    cerr << buf << endl;
}

void XFreeFrame(AVFrame** frame) {
    if (!frame || !(*frame)) { 
        return; 
    }
    av_frame_free(frame);
}

void XFreePacket(AVPacket ** packet) {
    if (!packet || !(*packet)) {
        return;
    }
    av_packet_free(packet);
}

void MSleep(unsigned int ms) {
    //注意: 不能用 clock() —— macOS 上它是进程CPU时间, 会让本函数随进程负载变长/变短;
    //     用稳态时钟(wall time)保证按真实毫秒睡眠。
    auto beg = chrono::steady_clock::now();
    auto dur = chrono::milliseconds(ms);
    while (chrono::steady_clock::now() - beg < dur) {
        this_thread::sleep_for(chrono::milliseconds(1));
    }
}
long long NowMs() {
    return chrono::duration_cast<chrono::milliseconds>(
               chrono::steady_clock::now().time_since_epoch())
        .count();
}

//启动线程
void XThread::Start() {
    unique_lock<mutex> lock(m_);
    static int i = 0;
    i++;
    index_ = i;
    is_exit_ = false;
    //启动线程
    th_ = thread(&XThread::Main, this);
    stringstream ss;
    ss << "XThread::Start()" << index_;
    LOGINFO(ss.str());
}

//虚析构: 线程仍挂起时先等其退出, 避免 std::thread 析构触发 terminate
//(子类析构应调用各自的 Stop() 完成资源清理, 这里仅兜底 join)
XThread::~XThread() {
    if (th_.joinable())
        th_.join();
}
void XThread::Exit() {
    stringstream ss;
    ss << "XThread::Exit() begin" << index_;
    LOGINFO(ss.str());
    is_exit_ = true;
}
//等待线程退出
void XThread::Wait() {
    stringstream ss;
    if (th_.joinable()) //判断子线程是否可以等待
        th_.join();     //等待子线程退出
    ss.str("");
    ss << "XThread::Wait() end" << index_;
    LOGINFO(ss.str());
}

//停止线程（设置退出标志，等待线程退出）
void XThread::Stop()
{
    Exit();
    Wait();
}


//创建对象
XPara* XPara::Create() {
    return new XPara();
}

XPara::~XPara() {
    if (para) {
        avcodec_parameters_free(&para);
    }
    if (time_base) {
        delete time_base;
        time_base = nullptr;
    }
}
//私有是禁止创建栈中对象
XPara::XPara() {
    para = avcodec_parameters_alloc();
    time_base = new AVRational();
}

AVPacket* XAVPacketList::Pop() {
    unique_lock<mutex> lock(mux_);
    if (pkts_.empty())return nullptr;
    auto pkt = pkts_.front();
    pkts_.pop_front();
    return pkt;
}

int XAVPacketList::Size() {
    unique_lock<mutex> lock(mux_);
    return pkts_.size();
}

void XAVPacketList::Clear() {
    unique_lock<mutex> lock(mux_);
    while (!pkts_.empty()) {
        av_packet_free(&pkts_.front());
        pkts_.pop_front();
    }
}

void XAVPacketList::SortByPts() {
  unique_lock<mutex> lock(mux_);
  pkts_.sort([](AVPacket* a, AVPacket* b) { return a->pts < b->pts; });
}

void XAVPacketList::IgnoreMaxPackets(bool ignore) {
  ignore_max_packets_ = ignore;
}

// 保留最长递增子序列
std::list<AVPacket*> LIS(const std::list<AVPacket*>& input) {
  std::vector<AVPacket*> nums(input.begin(), input.end());
  int n = nums.size();
  std::vector<int> dp(n, 1), prev(n, -1);
  int max_len = 0, max_idx = 0;
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < i; ++j) {
      if (nums[j]->pts < nums[i]->pts && dp[j] + 1 > dp[i]) {
        dp[i] = dp[j] + 1;
        prev[i] = j;
      }
    }
    if (dp[i] > max_len) {
      max_len = dp[i];
      max_idx = i;
    }
  }
  std::list<AVPacket*> lis;
  // 回溯并倒序插入
  std::vector<AVPacket*> tmp;
  while (max_idx != -1) {
    tmp.push_back(nums[max_idx]);
    max_idx = prev[max_idx];
  }
  for (auto it = tmp.rbegin(); it != tmp.rend(); ++it)
    lis.push_back(*it);
  return lis;
}

void XAVPacketList::ClearInvalidPackets() {
  unique_lock<mutex> lock(mux_);
  pkts_ = LIS(pkts_);
}

void XAVPacketList::Push(AVPacket* pkt) {
  if (!pkt) {
    return;
  }
    unique_lock<mutex> lock(mux_);
    //生成新的AVPacket 对象 引用计数+1;
    auto p = av_packet_alloc();
    av_packet_ref(p, pkt);//引用计数 减少数据复制，线程安全
    pkts_.push_back(p);

    if (ignore_max_packets_) {
      return;
    }

    //超出最大空间，清理数据，到关键帧位置
    if (pkts_.size() > max_packets_) {
        //处理第一帧
        if (pkts_.front()->flags & AV_PKT_FLAG_KEY) {
            //关键帧
            av_packet_free(&pkts_.front());//清理
            pkts_.pop_front();  //出队
            return;
        }
        //清理所有非关键帧之前的数据
        while (!pkts_.empty()) {
            if (pkts_.front()->flags & AV_PKT_FLAG_KEY) {
                //关键帧
                return;
            }
            av_packet_free(&pkts_.front());//清理
            pkts_.pop_front();  //出队
        }
    }
}

double XTools::get_rotation_from_frame(const AVFrame* frame) {
  double angle = 0.0;
  // 查找 DISPLAYMATRIX 类型
  const AVFrameSideData* sd =
      av_frame_get_side_data(frame, AV_FRAME_DATA_DISPLAYMATRIX);
  if (sd && sd->size >= sizeof(int32_t)) {
    angle = av_display_rotation_get((int32_t*)sd->data);
  }
  return angle;
}

void XTools::copy_side_data(const AVFrame* src,
                    AVFrame* dst,
                    int type) {
  const AVFrameSideData* src_sd =
      av_frame_get_side_data(src, (AVFrameSideDataType)type);
  if (src_sd) {
    AVFrameSideData* dst_sd =
        av_frame_new_side_data(dst, (AVFrameSideDataType)type, src_sd->size);
    if (dst_sd) {
      memcpy(dst_sd->data, src_sd->data, src_sd->size);
      dst_sd->size = src_sd->size;
      dst_sd->type = src_sd->type;
      av_dict_copy(&dst_sd->metadata, src_sd->metadata, 0);
    }
  }
}

std::string XTools::get_app_path() {
  char buffer[4096] = {0};
#ifdef WIN32
  GetModuleFileNameA(NULL, buffer, sizeof(buffer));
  std::string::size_type pos = std::string(buffer).find_last_of("\\/");
  return std::string(buffer).substr(0, pos);
#elif defined(__APPLE__)
  uint32_t size = sizeof(buffer);
  if (_NSGetExecutablePath(buffer, &size) == 0) {
    std::string::size_type pos = std::string(buffer).find_last_of("/");
    return std::string(buffer).substr(0, pos);
  }
  // Fallback to current working directory
  if (getcwd(buffer, sizeof(buffer))) {
    return std::string(buffer);
  }
  return ".";
#else
  ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (len != -1) {
    buffer[len] = '\0';
    std::string::size_type pos = std::string(buffer).find_last_of("/");
    return std::string(buffer).substr(0, pos);
  }
  // Fallback to current working directory
  if (getcwd(buffer, sizeof(buffer))) {
    return std::string(buffer);
  }
  return ".";
#endif
}
