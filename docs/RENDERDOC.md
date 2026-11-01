# RenderDoc 集成说明

对应 `PLAN.md` 的 RenderDoc 集成计划。当前代码已完成**应用内 API 对接、捕获入口、OpenGL 调试标记**，
并验证了"已注入 / 动态加载 / 未安装"三种运行状态。

## 1. 代码落点

计划中提到的 `Form/Widget/ShowVideoWidget.cpp` 是遗留代码，不参与当前构建。
实际渲染路径为：

| 位置 | 作用 |
| :--- | :--- |
| `xcodec/xrenderdoc.h/.cpp` | RenderDoc in-application API 封装（无 Qt 依赖，随 XCodec 提供） |
| `xcodec/renderdoc_app.h` | 官方 API 头文件（MIT，v1.47） |
| `xcodec/xgldebug.h` | `KHR_debug` / `EXT_debug_marker` 分组标记封装，不支持时自动空操作 |
| `xvideoview/x_qt_opengl_widget.cpp` | `paintGL` 中为"基础视频层 / 叠加层 N"打分组 |
| `xcodec/xshader_yv12.cpp` | "YUV420P 纹理上传 / YUV420P 绘制"分组 |
| `xcodec/xshader_nv12.cpp` | "NV12 纹理上传 / NV12 绘制"分组 |
| `xcodec/xshader_rgba.cpp` | "RGBA 纹理上传 / RGBA 绘制"分组 |
| `xvideo_editor/XVideoEditorWindow.cpp` | "GPU 捕获"按钮 + `Ctrl+F12` |
| `xvideoview/xplayvideo.cpp` | 播放器 `Ctrl+F12` |
| `xvideo_editor/main.cpp`、`xvideoview/main.cpp` | 启动时尽早 `XRenderDoc::Init()` |

## 2. 环境准备（Step 1）

### Windows（推荐，官方支持最完整）

1. 安装 RenderDoc：<https://renderdoc.org/>（Windows 安装包）。
2. 用 **Debug** 或 **RelWithDebInfo** 配置构建，保留符号：
   ```
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config RelWithDebInfo --target XVideoEditor XVideoView
   ```
3. 可执行文件位于 `Bin/RelWithDebInfo/`（多配置生成器会自动加配置子目录），
   `Shader*.fsh/.vsh` 与 DLL 已由 POST_BUILD 复制到同目录。

### macOS

官方**不提供 macOS 预编译包**，只能源码构建，且仅支持 OpenGL（本项目正好使用 OpenGL 4.1）。
源码构建时不需要 qrenderdoc 也能得到捕获运行库：

```bash
cmake -S renderdoc -B rdbuild -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_QRENDERDOC=OFF -DENABLE_PYRENDERDOC=OFF
cmake --build rdbuild --target renderdoc -j8
# 产物: rdbuild/lib/librenderdoc.dylib
```

> 本机验证时因源码包下载速度约 20–30 KB/s（80 MB）未能完成安装；
> 代码侧的加载/降级路径已用桩库验证（见第 6 节）。

## 3. 启动与捕获（Step 2–3）

### 方式 A：由 RenderDoc 启动（推荐）

RenderDoc → **Launch Application**：

- Executable Path：`Bin/Debug/XVideoView.exe`（或 `XVideoEditor.exe`）
- Working Directory：可执行文件所在目录
- 启动后播放视频，按 **F12**（RenderDoc 自带全局热键）捕获。

由 RenderDoc 启动时运行库已注入，程序启动日志会打印：

```
[RenderDoc] API 就绪, 版本 1.xx.x
```

### 方式 B：应用内捕获

- 编辑器：工具栏 **GPU 捕获** 按钮，或 `Ctrl+F12`
- 播放器：`Ctrl+F12`

捕获文件默认写到 **可执行文件旁的 `rdoc_captures/`**：

```
rdoc_captures/editor_frame123.rdc
rdoc_captures/viewer_frame456.rdc
```

捕获完成会弹出气泡提示实际路径（编辑器）。

### 方式 C：手动指定运行库

未在 RenderDoc 中启动、但想动态加载运行库时：

```bash
# macOS
RENDERDOC_LIB=/path/librenderdoc.dylib ./Bin/XVideoEditor video.mp4
# Windows (PowerShell)
$env:RENDERDOC_LIB="C:\Program Files\RenderDoc\renderdoc.dll"
.\Bin\Debug\XVideoEditor.exe video.mp4
```

注意：部分平台要求在**创建 GL 上下文之前**完成加载才能挂钩，程序已在 `main()` 最开始调用 `Init()`。
最稳妥的方式仍是方式 A。

## 4. 调试标记（Step 4）

`xgldebug.h` 优先使用 `GL_KHR_debug` 的 `glPushDebugGroup`，其次使用
`GL_EXT_debug_marker` 的 `glPushGroupMarkerEXT`；两者都没有时是空操作。

启动日志会显示实际能力，例如：

```
XQtOpenGLWidget: RenderDoc debug markers=on (KHR_debug=0, EXT_debug_marker=1)
```

在 RenderDoc 的 **Event Browser** 中应看到如下层级：

```
基础视频层
  YUV420P 纹理上传     (3 次 glTexImage2D)
  YUV420P 绘制         (glDrawArrays)
叠加层 0
  RGBA 纹理上传
  RGBA 绘制
```

macOS 的 OpenGL 4.1 实测支持 `GL_EXT_debug_marker`，标记可用；Windows 上若驱动支持
`KHR_debug` 则使用更标准的 `glPushDebugGroup`。

## 5. 性能分析清单（Step 5）

打开 `.rdc` 后建议按以下顺序检查：

1. **Event Browser**
   - 每帧的 `glTexImage2D` 次数应与图层数 × 平面数一致（YUV420P 为 3，NV12 为 2，RGBA 为 1）。
   - 纹理上传与绘制是否分别落在对应分组内，排除多余 draw call。
2. **Texture Viewer**
   - 分别查看 Y / U / V 纹理，确认分辨率、通道格式（`GL_RED` / `GL_RG`）和内容正确。
   - 检查是否存在整帧重复上传（同一纹理每帧内容一致）。
3. **Pipeline State**
   - 视频层不应开启深度测试；叠加层只应有 `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA` 混合。
   - 确认没有多余的 `glEnable/glDisable` 状态切换。
4. **Performance / GPU Duration**
   - 对比不同分辨率的帧耗时，确认瓶颈在纹理上传还是片元着色。

### 代码审阅已发现的候选瓶颈

| 现象 | 位置 | 建议 |
| :--- | :--- | :--- |
| 每帧调用 `glTexImage2D` 重新分配纹理存储 | `XShaderYV12/NV12/RGBA::Draw` | 尺寸不变时改为首次 `glTexImage2D`、后续 `glTexSubImage2D`，可减少驱动分配/同步开销 |
| 每帧重复 `glBindBuffer` + `glVertexAttribPointer` | 同上 | 顶点属性在核心模式下可保存在 VAO 中，初始化时设置一次即可 |
| 多叠加层重复 `glEnable(GL_BLEND)` | `paintGL` | 可在循环外统一开启/关闭，减少状态切换 |

以上优化需先用 RenderDoc 的 GPU Duration 确认确有收益，再逐项落地并对比帧时间，
避免在未测量的情况下改变渲染行为。

### 预期验证结果（对应 PLAN.md 表格）

| 步骤 | 验证方法 | 预期 |
| :--- | :--- | :--- |
| 启动 | RenderDoc Launch Application | 日志出现 `[RenderDoc] API 就绪`，应用正常播放 |
| 捕获 | F12 / GPU 捕获按钮 | `rdoc_captures/*.rdc` 生成 |
| 分析 | Event Browser | 看到 `glTexImage2D` 与 `glDrawArrays`，顺序与分组正确 |
| 标记 | Event Browser 分组 | 出现"基础视频层 / 叠加层 N / 纹理上传 / 绘制" |
| 优化 | 对比帧时间 | 应用候选优化后 GPU 每帧耗时下降 |

## 6. 已验证内容

用官方 `renderdoc_app.h` 编译了一个桩运行库（导出 `RENDERDOC_GetAPI`），验证结果：

| 场景 | 结果 |
| :--- | :--- |
| `RENDERDOC_LIB` 指向桩库 | 加载成功，版本 `1.47.0`，`TriggerCapture` / `StartCapture` / `GetCapture` 均正常 |
| `DYLD_INSERT_LIBRARIES` 模拟 RenderDoc 启动注入 | 直接取到 API，无需 dlopen |
| 运行库不存在 | `Init()` 返回 false，错误信息明确，应用继续正常运行 |
| 编辑器 + 播放器实际启动 | GUI 稳定；日志确认 API 就绪、`EXT_debug_marker` 可用 |
| 完整构建 | `cmake --build build --target XVideoEditor XVideoView` 通过 |

## 7. 注意事项

- 捕获只对**当前 GL 上下文**生效；播放器/编辑器各只有一个 `QOpenGLWidget`，无需额外指定 device/window。
- `.rdc` 文件体积可能较大，建议只在高负载/掉帧场景捕获单帧。
- 发布给最终用户的二进制不依赖 RenderDoc；未安装时所有相关调用自动空操作。
- RenderDoc 运行库是可选依赖，**不要**把它提交进仓库。
