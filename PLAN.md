# RenderDoc Integration Plan for Video Frame Rate Debugging and Optimization

## Goal
Integrate RenderDoc into the VideoPlayer project to analyze OpenGL rendering pipelines, identify bottlenecks in frame processing, and optimize the video frame rate.

## Analysis
The project uses `QOpenGLWidget` in `ShowVideoWidget.cpp` for video rendering. It implements a YUV to RGB conversion using custom GLSL shaders. To debug this with RenderDoc, we need to ensure the application can be launched and captured by the tool, and potentially add markers to the rendering loop to identify specific frames or draw calls.

## Implementation Plan

### 1. Involved Files
- No source code files require immediate modification for basic RenderDoc integration (RenderDoc is primarily an external instrumentation tool).
- However, for advanced debugging, the following file may be modified to add debug markers:
    - `Form/Widget/ShowVideoWidget.cpp`

### 2. Execution Steps

#### Step 1: Environment Setup
- Install RenderDoc on the development machine.
- Verify that the build is configured as `Debug` or `RelWithDebInfo` to ensure symbols are available for better analysis.

#### Step 2: Application Launch via RenderDoc
- Open RenderDoc.
- Use the "Launch Application" tab:
    - **Executable Path**: Path to `Bin/Debug/XVideoView` (or the relevant target binary).
    - **Working Directory**: The project root or `Bin/Debug`.
- Launch the application.

#### Step 3: Capture Frame Data
- Once the video playback starts in `XVideoView`, use the RenderDoc overlay (default `F12` or `PrintScreen`) or the "Capture Frame" button in the UI to trigger a capture.
- Specifically capture frames during high-load scenarios to analyze frame rate drops.

#### Step 4: (Optional) Adding OpenGL Debug Markers
- If the rendering pipeline becomes complex, modify `Form/Widget/ShowVideoWidget.cpp` to include `glPushDebugGroup` and `glPopDebugGroup` (or the RenderDoc specific API) around the YUV texture upload and `glDrawArrays` calls.
- This will group related calls in the RenderDoc event browser.

#### Step 5: Performance Analysis
- Use the **Event Browser** to check for redundant state changes.
- Use the **Texture Viewer** to verify that Y, U, and V textures are being uploaded correctly and not causing excessive bandwidth usage.
- Analyze the **Pipeline State** to ensure no unnecessary blending or depth tests are enabled during video rendering.

### 3. Verification Methods

| Step | Verification Method | Expected Result |
| :--- | :--- | :--- |
| Launch | RenderDoc "Launch Application" | Application starts normally and is listed as "captured" in RenderDoc. |
| Capture | Trigger capture during playback | A `.rdc` capture file is generated containing the OpenGL command stream. |
| Analysis | Inspect Event Browser | All `glTexImage2D` and `glDrawArrays` calls are visible and correctly sequenced. |
| Markers | Check Event Browser groups | Rendered blocks are logically grouped by the markers added in Step 4. |
| Optimization | Compare frame times | After applying optimizations based on RenderDoc findings, the GPU time per frame should decrease. |
