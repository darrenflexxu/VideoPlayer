# Implementation Plan: Switch XVideoView to OpenGL Playback

## Context
The current implementation of `XVideoView` in the `XVideoView` application defaults to using SDL-based rendering (`XSDL`). The project already contains a complete OpenGL-based rendering implementation (`XShader`) within the `xcodec` library. This change aims to switch the default rendering engine to OpenGL to improve performance and potentially enable more advanced GPU-based rendering features.

## Recommended Approach
The transition is a drop-in replacement because both `XSDL` and `XShader` inherit from the `XVideoView` abstract base class. The core change involves updating the factory method call in `XPlayer` to request the `Shader` render type instead of `SDL`.

### Critical Files to Modify
- [xcodec/xplayer.cpp](xcodec/xplayer.cpp): Update the rendering engine selection.

### Implementation Steps

#### 1. Update Rendering Engine Selection
In `XPlayer::Open`, change the `RenderType` passed to the `XVideoView` factory.
- **Location**: `xcodec/xplayer.cpp:64`
- **Change**: Replace `XVideoView::Create(XVideoView::SDL)` with `XVideoView::Create(XVideoView::Shader)`.

#### 2. Verify Window ID Integration
Confirm that the `winId` passed from the Qt UI (`ui.video->winId()`) is correctly utilized by `XShader`.
- `XPlayer::Open` calls `view_->set_win_id(winid)`.
- `XShader::Init` uses `win_id_` to create the OpenGL context:
    - **Windows**: Uses `GetDC((HWND)win_id_)`.
    - **macOS/Linux**: Uses `SDL_CreateWindowFrom(win_id_)` or `SDL_CreateWindowFromNSView(win_id_)`.

## Verification Plan

### 1. Build Verification
- **Action**: Run the build process (e.g., `mkdir build && cd build && cmake .. && make`).
- **Expected Result**: Successful compilation without errors or warnings.

### 2. Initialization Verification
- **Action**: Launch `XVideoView` and open a video file.
- **Expected Result**: 
    - Check console logs for `XShader` initialization messages:
        - `"XShader: SDL window created: ..."` (on non-Windows)
        - `"XShader: GLContext setup success"`
        - `"OpenGL实现的版本号：..."`
    - Confirm the application does not crash during video initialization.

### 3. Playback Verification
- **Action**: Play various video files (YUV420P, NV12, etc.).
- **Expected Result**: 
    - Video frames are rendered correctly and smoothly.
    - A/V synchronization remains intact.
    - Window resizing (if applicable) is handled correctly by `glViewport` updates in `XShader::DrawFrame`.
