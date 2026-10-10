# Implementation Plan: Pure Audio Format Export

## Context
Currently, the project's export pipeline in `XCompositeExporter` is tightly coupled to video rendering. It requires a video encoder and an OpenGL FBO to be initialized, and the export loop is driven by video frame counts. This makes it impossible to export a project as a pure audio file (e.g., `.mp3`, `.m4a`).

The goal is to make the video path optional in the `XCompositeExporter`, allowing it to perform only audio mixing and encoding when a pure audio format is requested.

## Recommended Approach

We will introduce an `audio_only` mode to the `XCompositeExporter` that bypasses all video-related initialization and processing while maintaining the audio mixing pipeline.

### 1. `XCompositeExporter` Modifications
The exporter needs to be modified to make the video encoding path conditional.

- **Configuration**: Add a `bool audio_only_` flag and a corresponding `setAudioOnly(bool on)` method.
- **Conditional Initialization**:
    - **FBO**: In `Start()`, skip the initialization of the off-screen Framebuffer Object (FBO) if `audio_only_` is true.
    - **Encoders**: In `InitEncoders()`, skip the initialization of the video encoder and its parameters if `audio_only_` is true.
    - **Muxer**: Pass `nullptr` for video parameters (`vout`, `vtb`) to `mux_->Open()` when in audio-only mode.
- **Loop Refactoring**:
    - Refactor `ExportThread` to avoid relying on `total_frames` (which depends on FPS) for its loop termination.
    - Change the loop to be driven by the total duration of the timeline in milliseconds.
    - Inside the loop:
        - Call `CompositeAndEncodeVideo()` only if `!audio_only_`.
        - Always call `MixAndEncodeAudio()`.
- **Cleanup**: Ensure `video_enc_->Exit()` and `video_enc_->Wait()` are only called if the video encoder was actually initialized.

### 2. UI Integration (`XVideoEditorWindow`)
The UI must allow the user to choose an audio-only output format.

- **File Dialog**: Update `QFileDialog::getSaveFileName` in `doExport()` to include audio extensions: `"MP4 文件 (*.mp4);;音频文件 (*.mp3 *.m4a *.wav)"`.
- **Export Flag**: In `exportWorker()`, check the file extension of the output path. If it matches an audio format, call `exporter.setAudioOnly(true)`.

---

## Implementation Steps

### Step 1: Update `XCompositeExporter` Header
- **Action**: Add `bool audio_only_ = false;` and `void setAudioOnly(bool on);` to `xcodec/xcomposite_exporter.h`.
- **Verification**: Project compiles.

### Step 2: Modify `XCompositeExporter` Implementation
- **Action**: Update `xcodec/xcomposite_exporter.cpp`:
    - Wrap FBO initialization in `Start()` with `if (!audio_only_)`.
    - Wrap video encoder setup in `InitEncoders()` with `if (!audio_only_)`.
    - Update `mux_->Open()` call to pass `nullptr` for video parameters if `audio_only_` is true.
    - Refactor the `ExportThread` loop to be time-driven (0 to `total_duration_ms`) rather than frame-driven.
- **Verification**: Verify that `XCompositeExporter` can be initialized with `setAudioOnly(true)` without attempting to create a video encoder or FBO.

### Step 3: Update Export UI
- **Action**: Modify `xvideo_editor/XVideoEditorWindow.cpp`:
    - Update the save dialog filter to include `.mp3`, `.m4a`, and `.wav`.
    - In `exportWorker`, detect the extension and set the `audio_only` flag on the exporter.
- **Verification**: Launch the app, click "Export", and verify that `.m4a` or `.mp3` can be selected as the output extension.

---

## Verification Plan

### 1. Pure Audio Export Test
- **Action**: Create a timeline with mixed audio/video clips and export as `.m4a`.
- **Expected Result**: 
    - The export completes successfully.
    - The resulting `.m4a` file is playable and contains the correctly mixed audio from all tracks.
    - Console logs confirm that no video encoder was initialized.

### 2. Video Export Regression Test
- **Action**: Export a timeline as `.mp4`.
- **Expected Result**: 
    - Video and audio are both present.
    - Playback is synchronized and the quality is consistent with previous versions.

### 3. Edge Case Testing
- **Action**: Export a project containing only audio clips as an audio file.
- **Expected Result**: Successful export with no "missing video" errors.
- **Action**: Attempt to export a video-heavy project as a pure audio file.
- **Expected Result**: The system ignores the video layers and exports only the mixed audio.
