#pragma once

//OpenGL 调试分组标记封装
//RenderDoc 的 Event Browser 会把同名 draw call 归到一组, 便于定位
//"纹理上传"/"绘制"等阶段。GL 4.3 KHR_debug 或 EXT_debug_marker 可用时生效,
//macOS 的 GL 4.1 等不支持的环境自动降级为空操作, 不影响渲染。

#include <GL/glew.h>

namespace xgl {

inline bool DebugMarkerAvailable()
{
    return (GLEW_KHR_debug && glPushDebugGroup && glPopDebugGroup) ||
           (GLEW_EXT_debug_marker && glPushGroupMarkerEXT && glPopGroupMarkerEXT);
}

inline void PushMarker(const char* name)
{
    if (GLEW_KHR_debug && glPushDebugGroup && glPopDebugGroup)
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
    else if (GLEW_EXT_debug_marker && glPushGroupMarkerEXT && glPopGroupMarkerEXT)
        glPushGroupMarkerEXT(0, name);
}

inline void PopMarker()
{
    //push/pop 必须用同一套扩展, 否则分组会错位
    if (GLEW_KHR_debug && glPushDebugGroup && glPopDebugGroup)
        glPopDebugGroup();
    else if (GLEW_EXT_debug_marker && glPushGroupMarkerEXT && glPopGroupMarkerEXT)
        glPopGroupMarkerEXT();
}

} // namespace xgl
