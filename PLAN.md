# Implementation Plan: Migrate XVideoView to QOpenGLWidget

## Context
Currently, `XVideoView` uses either SDL (`XSDL`) or a manual OpenGL context (`XShader`) to render video. This approach requires manual window handle management (`winId`) and manual GL context creation, which is fragile (especially on macOS) and conflicts with Qt's own rendering pipeline. 

The goal is to replace this with `QOpenGLWidget`, integrating the video rendering directly into the Qt framework. This will improve stability, provide better integration with the UI, and leverage Qt's managed OpenGL lifecycle.

## Recommended Approach

The core of the migration is the introduction of a new class, `XQtOpenGLWidget`, which inherits from both `QOpenGLWidget` (for rendering) and `XVideoView` (for interface compatibility). 

Instead of the current synchronous "Decode $\rightarrow$ Draw" flow, we will move to an asynchronous "Produce $\rightarrow$ Consume" flow:
1. **Producer (`XPlayer`)**: Pushes decoded `AVFrame` to the widget's internal buffer and triggers `update()`.
2. **Consumer (`XQtOpenGLWidget`)**: In `paintGL()`, uploads the buffered frame to textures and executes the YUV $\rightarrow$ RGB shader.

### Critical Files to Modify/Create

#### 1. New Files
- [xcodec/x_qt_opengl_widget.h](xcodec/x_qt_opengl_widget.h): Definition of the `QOpenGLWidget` implementation.
- [xcodec/x_qt_opengl_widget.cpp](xcodec/x_qt_opengl_widget.cpp): Implementation of GL lifecycle (`initializeGL`, `resizeGL`, `paintGL`) and `XVideoView` methods.

#### 2. Existing Files
- [xcodec/xvideo_view.h](xcodec/xvideo_view.h): Add `QtOpenGL` to `RenderType` enum.
- [xcodec/xvideo_view.cpp](xcodec/xvideo_view.cpp): Update `Create()` factory to instantiate `XQtOpenGLWidget`.
- [xvideoview/xplayvideo.cpp](xvideoview/xplayvideo.cpp): Replace the generic `ui.video` widget with an instance of `XQtOpenGLWidget`.
- [xcodec/xplayer.cpp](xcodec/xplayer.cpp): Update `Update()` to push frames to the widget instead of calling a synchronous `DrawFrame`.

---

## Implementation Steps

### Step 1: Define the Interface Extension
- **Action**: Add `QtOpenGL` to `XVideoView::RenderType` in `xvideo_view.h`.
- **Action**: Update `XVideoView::Create` in `xvideo_view.cpp` to return `new XQtOpenGLWidget()` when `QtOpenGL` is requested.
- **Verification**: Project compiles.

### Step 2: Implement `XQtOpenGLWidget`
- **Action**: Create the widget class.
- **Implementation Details**:
    - Implement `initializeGL()`: Load and compile shaders (reusing logic from `XShaderYV12`/`XShaderNV12`).
    - Implement `paintGL()`: 
        - Lock internal `AVFrame` buffer.
        - Upload Y, U, V planes to textures using `glTexSubImage2D`.
        - Draw a full-screen quad with the YUV shader.
    - Implement `XVideoView::DrawFrame()`: Store the frame in the buffer and call `this->update()`.
- **Verification**: Widget compiles and can be instantiated.

### Step 3: Integrate Widget into UI
- **Action**: In `XPlayVideo::XPlayVideo`, replace the `ui.video` widget in the layout with the new `XQtOpenGLWidget`.
- **Action**: Update `XPlayVideo::Open` to ensure the `XPlayer` uses the `QtOpenGL` render type.
- **Verification**: The application launches, and the video area is occupied by the `XQtOpenGLWidget`.

### Step 4: Update Playback Loop
- **Action**: In `XPlayer::Update`, ensure `view_->DrawFrame(f, ...)` is called. Since `XQtOpenGLWidget::DrawFrame` now just triggers `update()`, this effectively shifts the rendering to the GUI thread.
- **Verification**: Video playback starts, and frames are updated on screen.

---

## Verification Plan

### 1. Build and Launch
- **Action**: Run the full build and launch `XVideoView`.
- **Expected Result**: No compilation errors; the app opens normally.

### 2. Rendering Verification
- **Action**: Play a variety of video formats (YUV420P, NV12).
- **Expected Result**: 
    - Video is displayed correctly without flickering.
    - Colors are accurate (correct YUV $\rightarrow$ RGB conversion).
    - Performance is smooth (no GUI thread blocking).

### 3. Integration Verification
- **Action**: Resize the window.
- **Expected Result**: `resizeGL` is called, and the video scales correctly to fill the widget area.

### 4. Resource Leak Check
- **Action**: Open and close multiple videos.
- **Expected Result**: No memory leaks or OpenGL context errors in the console.
