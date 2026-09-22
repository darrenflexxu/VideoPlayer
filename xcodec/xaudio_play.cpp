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
    vector<unsigned char> buf;
    int sample_size = av_get_bytes_per_sample((AVSampleFormat)frame->format);
    int channels = frame->ch_layout.nb_channels;
    unsigned char* L = frame->data[0];
    unsigned char* R = frame->data[1];
    if (channels == 1)
    {
        PushTo(id, frame->data[0], frame->nb_samples* sample_size, frame->pts);
        return;
    }
    //暂时支持双通道
    switch (frame->format)
    {
        //case AV_SAMPLE_FMT_S16P:        ///< signed 16 bits, planar
    case AV_SAMPLE_FMT_S32P:        ///< signed 32 bits, planar
    case AV_SAMPLE_FMT_FLTP:        ///< float, planar
        buf.resize(frame->linesize[0]);
        // LLLL RRRR  32
        // LR LR LR LR 4
        for (int i = 0; i < frame->nb_samples; i++)
        {
            memcpy(buf.data() + i * sample_size * channels,
                L + i * sample_size, sample_size);
            memcpy(buf.data() + i * sample_size * channels + sample_size,
                R + i * sample_size, sample_size);
        }
        PushTo(id, buf.data(), (int)buf.size(), frame->pts);
        return;
        break;
    default:
        break;
    }
    PushTo(id, frame->data[0], frame->linesize[0], frame->pts);
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
