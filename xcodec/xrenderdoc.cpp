#include "xrenderdoc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

//官方 in-application API 头文件(MIT), 仅在本文件内部使用, 不暴露给调用方
#include "renderdoc_app.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(_WIN32)
#include <unistd.h>
#endif

namespace {

std::mutex g_mtx;                       //Init 只允许一次; 其余接口读取 g_api
void* g_lib = nullptr;                  //动态加载的库句柄(避免被卸载)
RENDERDOC_API_1_6_0* g_api = nullptr;   //取到的 API(1.6.0 字段足够本模块使用)
bool g_inited = false;
std::string g_error;
std::string g_version;

void* openLibrary(const std::string& path)
{
#ifdef _WIN32
    return reinterpret_cast<void*>(LoadLibraryA(path.c_str()));
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
}

//lib 为空: 在"已经注入进程"的库/全局符号里找; 否则在指定句柄里找
void* findSymbol(void* lib, const char* name)
{
#ifdef _WIN32
    if (lib)
        return reinterpret_cast<void*>(GetProcAddress(
            reinterpret_cast<HMODULE>(lib), name));
    //RenderDoc 启动时会把 renderdoc.dll 注入进程, 直接按模块名取
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    return mod ? reinterpret_cast<void*>(GetProcAddress(mod, name)) : nullptr;
#else
    return lib ? dlsym(lib, name) : dlsym(RTLD_DEFAULT, name);
#endif
}

std::string executableDir()
{
    char buf[4096] = {0};
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(nullptr, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf))
        return std::string();
    std::string p(buf, n);
    size_t cut = p.find_last_of("\\/");
    return cut == std::string::npos ? std::string() : p.substr(0, cut);
#elif defined(__APPLE__)
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return std::string();
    std::string p(buf);
    size_t cut = p.find_last_of('/');
    return cut == std::string::npos ? std::string() : p.substr(0, cut);
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return std::string();
    std::string p(buf, static_cast<size_t>(n));
    size_t cut = p.find_last_of('/');
    return cut == std::string::npos ? std::string() : p.substr(0, cut);
#endif
}

//常见安装位置(按优先级)
std::vector<std::string> libraryCandidates()
{
    std::vector<std::string> v;
    const std::string dir = executableDir();
    const char* env = std::getenv("RENDERDOC_LIB");
    if (env && *env)
        v.push_back(env);
    if (!dir.empty()) {
#ifdef _WIN32
        v.push_back(dir + "\\renderdoc.dll");
#elif defined(__APPLE__)
        v.push_back(dir + "/librenderdoc.dylib");
#else
        v.push_back(dir + "/librenderdoc.so");
#endif
    }
#if defined(_WIN32)
    v.push_back("renderdoc.dll");
    v.push_back("C:/Program Files/RenderDoc/renderdoc.dll");
#elif defined(__APPLE__)
    v.push_back("/Applications/RenderDoc.app/Contents/MacOS/librenderdoc.dylib");
    v.push_back("/usr/local/lib/librenderdoc.dylib");
    v.push_back("/opt/homebrew/lib/librenderdoc.dylib");
    v.push_back("librenderdoc.dylib");
#else
    v.push_back("librenderdoc.so");
    v.push_back("/usr/lib/librenderdoc.so");
    v.push_back("/usr/local/lib/librenderdoc.so");
#endif
    return v;
}

} // namespace

XRenderDoc& XRenderDoc::Instance()
{
    static XRenderDoc inst;
    return inst;
}

bool XRenderDoc::Init()
{
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_inited)
        return g_api != nullptr;
    g_inited = true;

    //1) 先看进程里是否已经有 RenderDoc(由 RenderDoc 启动/注入)
    void* sym = findSymbol(nullptr, "RENDERDOC_GetAPI");

    //2) 再按环境变量/常见路径逐个尝试动态加载
    if (!sym) {
        const std::vector<std::string> paths = libraryCandidates();
        for (const std::string& p : paths) {
            if (p.empty())
                continue;
            void* lib = openLibrary(p);
            if (!lib)
                continue;
            sym = findSymbol(lib, "RENDERDOC_GetAPI");
            if (sym) {
                g_lib = lib;
                fprintf(stderr, "[RenderDoc] 已加载运行库: %s\n", p.c_str());
                break;
            }
#ifndef _WIN32
            dlclose(lib);
#else
            FreeLibrary(reinterpret_cast<HMODULE>(lib));
#endif
        }
    }

    if (!sym) {
        g_error = "未找到 RenderDoc 运行库(renderdoc.dll / librenderdoc.dylib)。"
                  "请用 RenderDoc 启动本程序, 或设置环境变量 RENDERDOC_LIB。";
        return false;
    }

    auto get_api = reinterpret_cast<pRENDERDOC_GetAPI>(sym);

    //RenderDoc 的 API 结构体只向后追加字段, 低版本请求也能拿到可用前缀,
    //因此从新到旧尝试, 兼容未升级的旧运行库。
    static const RENDERDOC_Version kVersions[] = {
        eRENDERDOC_API_Version_1_6_0,
        eRENDERDOC_API_Version_1_5_0,
        eRENDERDOC_API_Version_1_4_2,
        eRENDERDOC_API_Version_1_4_0,
        eRENDERDOC_API_Version_1_3_0,
        eRENDERDOC_API_Version_1_2_0,
        eRENDERDOC_API_Version_1_1_2,
        eRENDERDOC_API_Version_1_0_2,
        eRENDERDOC_API_Version_1_0_0,
    };
    for (RENDERDOC_Version ver : kVersions) {
        void* ptr = nullptr;
        if (get_api(ver, &ptr) == 1 && ptr) {
            g_api = reinterpret_cast<RENDERDOC_API_1_6_0*>(ptr);
            break;
        }
    }

    if (!g_api) {
        g_error = "RenderDoc 运行库已找到, 但 API 版本不兼容。";
        return false;
    }

    //版本号仅用于显示/日志; GetAPIVersion 在所有版本都存在
    if (g_api->GetAPIVersion) {
        int major = 0, minor = 0, patch = 0;
        g_api->GetAPIVersion(&major, &minor, &patch);
        g_version = std::to_string(major) + "." + std::to_string(minor) +
                    "." + std::to_string(patch);
    }
    fprintf(stderr, "[RenderDoc] API 就绪, 版本 %s\n",
            g_version.empty() ? "unknown" : g_version.c_str());
    return true;
}

bool XRenderDoc::Available() const
{
    return g_api != nullptr;
}

std::string XRenderDoc::Version() const
{
    return g_version;
}

bool XRenderDoc::SetCaptureDir(const std::string& dir, const std::string& prefix)
{
    if (!g_api || !g_api->SetCaptureFilePathTemplate || dir.empty())
        return false;
    std::string tmpl = dir;
    if (tmpl.back() != '/' && tmpl.back() != '\\')
        tmpl += '/';
    tmpl += prefix.empty() ? "capture" : prefix;
    //RenderDoc 会自动创建不存在的目录
    g_api->SetCaptureFilePathTemplate(tmpl.c_str());
    return true;
}

bool XRenderDoc::TriggerCapture()
{
    if (!g_api || !g_api->TriggerCapture)
        return false;
    g_api->TriggerCapture();
    return true;
}

bool XRenderDoc::StartCapture()
{
    if (!g_api || !g_api->StartFrameCapture)
        return false;
    //OpenGL: device/window 传 nullptr 表示通配(当前唯一上下文/窗口)
    g_api->StartFrameCapture(nullptr, nullptr);
    return true;
}

bool XRenderDoc::EndCapture()
{
    if (!g_api || !g_api->EndFrameCapture)
        return false;
    return g_api->EndFrameCapture(nullptr, nullptr) == 1;
}

bool XRenderDoc::IsCapturing() const
{
    return g_api && g_api->IsFrameCapturing && g_api->IsFrameCapturing() == 1;
}

unsigned int XRenderDoc::CaptureCount() const
{
    if (!g_api || !g_api->GetNumCaptures)
        return 0;
    return g_api->GetNumCaptures();
}

std::string XRenderDoc::LastCapturePath()
{
    if (!g_api || !g_api->GetNumCaptures || !g_api->GetCapture)
        return std::string();
    const unsigned int n = g_api->GetNumCaptures();
    if (n == 0)
        return std::string();
    std::vector<char> buf(4096, 0);
    uint32_t len = static_cast<uint32_t>(buf.size());
    uint64_t ts = 0;
    if (g_api->GetCapture(n - 1, buf.data(), &len, &ts) != 1)
        return std::string();
    //pathlength 的包含 NUL 与否各版本描述略有差异, 直接按 C 字符串取长度最稳
    size_t sz = 0;
    while (sz < buf.size() && buf[sz] != '\0')
        ++sz;
    return std::string(buf.data(), sz);
}

std::string XRenderDoc::LastError() const
{
    return g_error;
}
