#pragma once

#include <string>
#include "xtools.h"

//RenderDoc 应用内 API 封装(可选依赖)
//
//使用方式:
//  1) 用 RenderDoc 启动本程序(推荐): 库已注入进程, Init() 直接取到 API
//  2) 设置环境变量 RENDERDOC_LIB 指向 renderdoc.dll / librenderdoc.dylib,
//     Init() 会动态加载(部分平台需在创建 GL 上下文前加载才能挂钩)
//
//未检测到 RenderDoc 时, 所有接口安全降级为空操作, 不影响播放/导出。
//捕获请求只是"下一帧捕获", 真正的 .rdc 文件由 RenderDoc 运行库写出。
class XCODEC_API XRenderDoc
{
public:
    static XRenderDoc& Instance();

    //加载并获取 API(幂等; 建议在 main 开头、创建 GL 上下文之前调用)
    //返回当前是否可用
    bool Init();

    //是否已成功取得 RenderDoc API
    bool Available() const;

    //RenderDoc 版本, 如 "1.47"; 未加载返回空串
    std::string Version() const;

    //捕获文件输出模板(目录不存在会自动创建)
    //最终文件名: <dir>/<prefix>_frameNNN.rdc
    bool SetCaptureDir(const std::string& dir,
                       const std::string& prefix = "capture");

    //请求捕获"下一帧"(由渲染线程在下一帧呈现时落盘)
    bool TriggerCapture();

    //显式捕获一段帧区间(OpenGL 下 device/window 均可传 nullptr 表示通配)
    bool StartCapture();
    bool EndCapture();
    bool IsCapturing() const;

    //已捕获数量 / 最近一次捕获文件绝对路径(没有则返回空串)
    unsigned int CaptureCount() const;
    std::string LastCapturePath();

    //最近一次 Init 失败原因(用于界面提示)
    std::string LastError() const;

private:
    XRenderDoc() = default;
    XRenderDoc(const XRenderDoc&) = delete;
    XRenderDoc& operator=(const XRenderDoc&) = delete;
};
