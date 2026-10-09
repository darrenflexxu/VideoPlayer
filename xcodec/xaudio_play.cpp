#include "predefine_header.h"

using namespace std;

//从单个源消费最多 len 字节到 out, 返回实际混入的字节数(持锁时调用)
static int ConsumeSource(XAudioSource& s, unsigned char* out, int len, long long& cur_pts)
{
    if (s.datas.empty())
        return 0;
    cur_pts = s.datas.front().pts;
    int mixed = 0;
    while (mixed < len && !s.datas.empty())
    {
        XData& buf = s.datas.front();
        int avail = (int)buf.data.size() - buf.offset;
        if (avail > len - mixed)
            avail = len - mixed;
        if (avail > 0)
        {
            memcpy(out + mixed, buf.data.data() + buf.offset, avail);
            mixed += avail;
            buf.offset += avail;
        }
        if (buf.offset >= (int)buf.data.size())
            s.datas.pop_front();
    }
    return mixed;
}

XAudioSource& XAudioPlay::source(int id)
{
    for (auto& s : sources_)
        if (s.id == id)
            return s;
    sources_.push_back(XAudioSource());
    sources_.back().id = id;
    return sources_.back();
}

void XAudioPlay::addSource(int id)
{
    std::lock_guard<std::mutex> lock(mux_);
    source(id);
}

void XAudioPlay::removeSource(int id)
{
    std::lock_guard<std::mutex> lock(mux_);
    for (auto it = sources_.begin(); it != sources_.end(); ++it)
    {
        if (it->id == id)
        {
            sources_.erase(it);
            break;
        }
    }
}

void XAudioPlay::setSourceVolume(int id, int v)
{
    if (v < 0) v = 0;
    if (v > 128) v = 128;
    std::lock_guard<std::mutex> lock(mux_);
    source(id).volume = v;
}

void XAudioPlay::ClearBuffers()
{
    std::lock_guard<std::mutex> lock(mux_);
    for (auto& s : sources_)
        s.datas.clear();
}

long long XAudioPlay::mixedBytes(int id)
{
    std::lock_guard<std::mutex> lock(mux_);
    for (auto& s : sources_)
        if (s.id == id)
            return s.mixed_bytes;
    return 0;
}

class CXAudioPlay :public XAudioPlay
{
public:
    
    //暂停
    void Pause(bool is_pause)
    {
        if (is_pause)
        {
            SDL_PauseAudio(1);
            pause_begin = NowMs();
        }
        else
        {
            //去掉暂停的事件
            if(pause_begin>0)
                last_ms_ += (NowMs() - pause_begin);
            SDL_PauseAudio(0);
        }
            
    }
    bool Open(XAudioSpec& spec)
    {
        this->spec_ = spec;
        //退出上一次音频
        SDL_QuitSubSystem(SDL_INIT_AUDIO);

        SDL_AudioSpec sdl_spec;
        sdl_spec.freq = spec.freq;
        sdl_spec.format = spec.format;
        sdl_spec.channels = spec.channels;
        sdl_spec.samples = spec.samples;
        sdl_spec.silence = 0;
        sdl_spec.userdata = this;
        sdl_spec.callback = AudioCallback;
        if (SDL_OpenAudio(&sdl_spec, nullptr) < 0)
        {
            cerr << SDL_GetError() << endl;
            return false;
        }
        //开始播放
        SDL_PauseAudio(0);
        return true;
    }
    void Close()
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        unique_lock<mutex> lock(mux_);
        for (auto& s : sources_)
            s.datas.clear();
        cur_pts_ = 0; //当前播放位置
        last_ms_ = 0;  //上次的时间戳
        pause_begin = 0;//暂停开始时间戳
    }
    void Callback(unsigned char* stream, int len)
    {
        SDL_memset(stream, 0, len);
        unique_lock<mutex> lock(mux_);
        // 多源混音: 每个活跃源各取 len 字节, 用 SDL_MixAudio 累加进同一输出缓冲
        vector<unsigned char> tmp((size_t)len > 0 ? len : 1);
        bool any = false;
        for (auto& s : sources_)
        {
            if (s.datas.empty())
                continue;
            memset(tmp.data(), 0, len);
            if (ConsumeSource(s, tmp.data(), len, cur_pts_) > 0)
            {
                SDL_MixAudio(stream, tmp.data(), len, (Uint8)s.volume);
                s.mixed_bytes += len;   //累计混音字节数(调试/导出比对用)
                any = true;
            }
            if (!any)
                continue;
            last_ms_ = NowMs();
        }
        (void)any;
    }
    long long cur_pts() override
    {
        double ms = 0;
        if (last_ms_ > 0)
            ms = NowMs() - last_ms_;//距离上次写入缓冲的播放时间毫秒
        //pts 毫秒换算pts的时间基数
        if(time_base_ > 0)
            ms = ms / (double)1000 / (double)time_base_;
        return cur_pts_ + speed_*ms;
    }
private:
    long long cur_pts_ = 0; //当前播放位置
    long long last_ms_ = 0;  //上次的时间戳
    long long pause_begin = 0;//暂停开始时间戳
};

void XAudioPlay::PushTo(int id, AVFrame* frame)
{
    if (!frame || !frame->data[0])return;
    int sample_size = av_get_bytes_per_sample((AVSampleFormat)frame->format);
    int channels = frame->ch_layout.nb_channels;
    if (sample_size <= 0 || channels <= 0)
        return;
    vector<unsigned char> buf;
    //单声道: 平面即打包, 直接推送
    if (channels == 1)
    {
        PushTo(id, frame->data[0], frame->nb_samples * sample_size, frame->pts);
        return;
    }
    //平面(planar)格式 -> 交错(interleaved): 总字节数 = 样本数 * 通道数 * 每样本字节数
    //注意: 不能按 linesize[0](单个平面大小)分配, 否则只推送一半数据导致播放速度翻倍
    switch (frame->format)
    {
    case AV_SAMPLE_FMT_U8P:
    case AV_SAMPLE_FMT_S16P:
    case AV_SAMPLE_FMT_S32P:
    case AV_SAMPLE_FMT_FLTP:
    case AV_SAMPLE_FMT_DBLP:
        buf.resize((size_t)frame->nb_samples * channels * sample_size);
        for (int i = 0; i < frame->nb_samples; i++)
            for (int ch = 0; ch < channels; ch++)
                memcpy(buf.data() + ((size_t)i * channels + ch) * sample_size,
                       frame->data[ch] + (size_t)i * sample_size, sample_size);
        PushTo(id, buf.data(), (int)buf.size(), frame->pts);
        return;
    default:
        break;
    }
    //打包(interleaved)格式: 数据本已交错
    PushTo(id, frame->data[0], frame->nb_samples * channels * sample_size, frame->pts);
}
void XAudioPlay::Push(AVFrame* frame)
{
    PushTo(0, frame);
}
bool XAudioPlay::Open(XPara &para)
{
    if(para.time_base->num> 0)
        time_base_ = (double)para.time_base->den / (double)para.time_base->num;
    return Open(para.para);
}
bool XAudioPlay::Open(AVCodecParameters* para)
{
    XAudioSpec spec;
  spec.channels = para->ch_layout.nb_channels;
    spec.freq = para->sample_rate;

    switch (para->format)
    {
    case AV_SAMPLE_FMT_S16:         ///< signed 16 bits
    case AV_SAMPLE_FMT_S16P:        ///< signed 16 bits, planar
        spec.format = AUDIO_S16;
        break;
    case AV_SAMPLE_FMT_S32:         ///< signed 32 bits
    case AV_SAMPLE_FMT_S32P:        ///< signed 32 bits, planar
        spec.format = AUDIO_S32;
        break;
    case AV_SAMPLE_FMT_FLT:         ///< float
    case AV_SAMPLE_FMT_FLTP:        ///< float, planar
        spec.format = AUDIO_F32;
        break;
    default:
        break;
    }
    return Open(spec);
}
XAudioPlay* XAudioPlay::Instance()
{
    static CXAudioPlay cx;
    return &cx;
}

XAudioPlay::XAudioPlay()
{
    SDL_Init(SDL_INIT_AUDIO);
}
