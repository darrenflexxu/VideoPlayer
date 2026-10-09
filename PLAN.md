# Implementation Plan: Multi-track Editing with m4a Support

## Context
The user wants to fully implement multi-track editing, specifically adding support for `.m4a` audio files and ensuring the "composite" nature of the project (overlapping layers) is preserved during both playback and export.

While the `TimelineModel` and `TimelinePlayer` already support layering and Z-order, the current export process uses a linear concatenation tool (`XConvertor::StartConcat`), and the UI restricts imports to video files.

## Recommended Approach

We will enable audio-only imports, refine the multi-track playback coordination, and migrate the export process to use the `XCompositeExporter` for true layered rendering.

### 1. Enable `.m4a` and Audio-Only Imports
The project uses FFmpeg, which natively supports AAC/MPEG-4 (`.m4a`). We need to update the UI to allow these files and the model to categorize them correctly.
- **UI Update**: Expand the `QFileDialog` filter in `XVideoEditorWindow::onAddClicked` to include `*.m4a`.
- **Type Detection**: In `XVideoEditorWindow::addClip`, use `XDemuxTask` to check for the presence of a video stream.
    - If video exists: `ClipType::VIDEO`, place on a "Video Track".
    - If video is absent: `ClipType::AUDIO`, place on an "Audio Track".

### 2. Refine Multi-track Playback
The `TimelinePlayer` manages a "Base" player and "Overlay" players.
- **Base Player Coordination**: Ensure the `TimelinePlayer` can handle timelines that may consist entirely of audio clips or start with an audio clip.
- **Audio Mixing**: Leverage the existing `XAudioPlay` mixer (which uses `SDL_MixAudio`) to ensure that the base audio and all overlay audio (including `.m4a` clips) are summed correctly.

### 3. Transition to Composite Export
The current `exportWorker` uses `XConvertor::StartConcat`, which is linear. We will migrate to `XCompositeExporter` to support multi-track composition.
- **Model Mapping**: Convert the `TimelineModel` (with its tracks and offsets) into a list of `XExportClip` objects.
- **Composite Rendering**: `XCompositeExporter` will render the layered video (using an FBO) and mix the audio streams before encoding the final file.
- **Export Workflow**: Update `XVideoEditorWindow::exportWorker` to instantiate `XCompositeExporter`, configure the output settings, and poll for completion.

---

## Implementation Steps

### Step 1: UI and Import Logic (`xvideo_editor/XVideoEditorWindow.cpp`)
- **Action**: Update the file filter: `"视频/音频文件 (*.mp4 *.mov *.mkv *.avi *.ts *.m4a);;所有文件 (*)"`.
- **Action**: Update `addClip()`:
    - Use `XDemuxTask` to probe the file.
    - If `CopyVideoPara()` returns `nullptr`, set the clip type to `ClipType::AUDIO` and create/use an "Audio Track".
- **Verification**: Import an `.m4a` file; verify it appears on the timeline as an audio clip.

### Step 2: Playback Synchronization (`xvideo_editor/TimelinePlayer.cpp`)
- **Action**: Ensure `TimelinePlayer::play()` and `seekTo()` can operate correctly when the first active clip is audio-only.
- **Action**: Verify that `buildOverlayPlayer` is triggered for all `ClipType::AUDIO` segments.
- **Verification**: Play a sequence where an `.m4a` clip overlaps a video clip; verify both are heard.

### Step 3: Export Migration (`xvideo_editor/XVideoEditorWindow.cpp`)
- **Action**: Rewrite `exportWorker()`:
    1. Replace `XConvertor` with `XCompositeExporter`.
    2. Map `TimelineModel` $\rightarrow$ `std::vector<XExportClip>`.
    3. Call `exporter->Start(out_url)`.
    4. Poll `exporter->finished()`.
- **Verification**: Export a project with overlapping video and `.m4a` audio. Verify the output file contains the mixed audio and layered video.

---

## Verification Plan

| Test Case | Action | Expected Result |
| :--- | :--- | :--- |
| **m4a Import** | Add an `.m4a` file to the timeline. | File is accepted and categorized as `ClipType::AUDIO`. |
| **Audio Overlap** | Place an `.m4a` clip over a video clip. | Both audio sources are mixed and played simultaneously in the preview. |
| **Multi-track Export** | Export a project with layered tracks. | The resulting file matches the visual and auditory layers of the timeline. |
| **Format Compatibility** | Export a project containing only `.m4a` clips. | The output file (e.g., .mp4) contains the mixed audio. |
| **Stability** | Export a complex multi-track timeline. | No crashes; the `XCompositeExporter` handles the layered rendering successfully. |
