# Implementation Plan: Multi-track Editing (Layered Compositing)

## Context
The goal is to upgrade the video editor from a linear sequence of clips to a multi-track system. This enables advanced features like "Picture-in-Picture" (PiP), multi-stream audio mixing, and the addition of overlays (subtitles and stickers). This requires a fundamental shift from a "single-stream" pipeline to a "composite" pipeline for both video and audio.

## Recommended Approach

We will implement a layered architecture where the final output is a composite of multiple active sources at any given timestamp.

### 1. Multi-track Model
We will redefine the project structure to support layering.
- **`Track`**: A logical layer (e.g., Video Track 1, Audio Track 2).
- **`Clip`**: Now includes spatial and stylistic properties:
    - `trackIndex`: The layer index (higher index = top layer).
    - `rect {x, y, w, h}`: Normalized coordinates (0.0 - 1.0) for "Picture-in-Picture" positioning.
    - `alpha`: Transparency for overlays.
    - `type`: `VIDEO`, `AUDIO`, `SUBTITLE`, `STICKER`.
- **`TimelineModel`**: Manages multiple tracks and provides a method `getActiveClips(timestamp)` that returns all clips currently playing across all tracks, sorted by Z-order.

### 2. Video Compositor (OpenGL)
The rendering engine (`XShader`) will be modified to perform a composition pass instead of a single frame draw.
- **Implementation**:
    - **Alpha Blending**: Enable `GL_BLEND` with `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`.
    - **Composite Loop**: In `XShader::DrawFrame`, iterate through all active clips:
        - Upload the texture (YUV for video, RGBA for stickers/text).
        - Set the OpenGL viewport/quad coordinates based on the clip's `rect`.
        - Render the quad.
    - **Texture Support**: Expand `XShader` to handle standard RGBA textures for stickers and subtitles.

### 3. Audio Mixer (SDL)
The audio engine (`XAudioPlay`) will be refactored to support concurrent streams.
- **Implementation**:
    - **Multi-stream Mixer**: Replace the single-stream callback with a mixer that:
        1. Maintains a list of active audio clips.
        2. Decodes the required buffer for each active clip.
        3. Mixes them using `SDL_MixAudio` into a final output buffer.
    - **Volume Control**: Apply independent volume levels for each audio track.

### 4. Export Pipeline (`XConvertor`)
The transcoding process must mirror the real-time compositor to ensure the exported file matches the preview.
- **Implementation**:
    - **Off-screen Rendering**: Use a Framebuffer Object (FBO) to render the composite frame (all tracks) into a pixel buffer.
    - **Encoding**: Pass the FBO's RGB pixels to the video encoder.
    - **Audio Summation**: Sum all active audio streams into a single mixed track for the audio encoder.

---

## Implementation Steps

### Phase 1: Model & Infrastructure
1. **Update `TimelineModel`**: Redefine the `Clip` struct and implement `Track` management.
2. **Expand `XShader`**: Add support for RGBA textures and alpha blending.
3. **Verification**: Verify that the model correctly identifies all active clips at a specific timestamp.

### Phase 2: Real-time Playback (Preview)
1. **Implement Video Compositor**: Modify `XShader` to render multiple quads based on clip positions.
2. **Implement Audio Mixer**: Refactor `XAudioPlay` to blend multiple audio streams.
3. **Verification**: Play a project with overlapping videos (PiP) and mixed audio. Verify correct visual layering and audio summation.

### Phase 3: Overlays (Stickers & Subtitles)
1. **Sticker Support**: Implement PNG loading $\rightarrow$ GL Texture $\rightarrow$ Compositor.
2. **Subtitle Support**: Implement text-to-texture rendering (using Qt or a small library) and render as a transparent overlay.
3. **Verification**: Verify that stickers and subtitles appear correctly over the video content.

### Phase 4: Composite Export
1. **FBO Integration**: Update `XConvertor` to use an off-screen FBO for frame composition.
2. **Audio Mixing**: Integrate the multi-stream mixer into the transcoding pipeline.
3. **Verification**: Export a multi-track project. Verify the output file matches the real-time preview.

---

## Verification Plan

| Test Case | Action | Expected Result |
| :--- | :--- | :--- |
| **PiP Effect** | Place a small video clip over a larger one. | Small video renders on top of the larger one at the correct position. |
| **Audio Mix** | Play a video with background music on a separate track. | Both audio sources are heard clearly and mixed correctly. |
| **Layering** | Place a sticker over a subtitle, over a video. | Correct Z-order: Sticker $\rightarrow$ Subtitle $\rightarrow$ Video. |
| **Export Fidelity** | Export a composite project. | The exported video maintains the exact positioning, alpha, and audio mix of the preview. |
| **Performance** | Play a project with 5+ active layers. | Rendering remains smooth (no significant FPS drop). |
