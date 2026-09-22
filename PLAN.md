# Implementation Plan: Video Timeline Editor

## Context
The user requires a new UI tool for video editing based on the `XCodec` and `XVideoTranscode` modules. This tool will transition the project from simple transcoding to a visual editing experience, featuring a timeline for clip manipulation (Cut & Splice) and a real-time playback preview.

## Recommended Approach

We will create a new module, `XVideoEditor`, that integrates the existing playback (`XPlayer`), rendering (`XQtOpenGLWidget`), and transcoding (`XConvertor`) components into a cohesive visual editor.

### Architecture Overview

The editor follows a Model-View-Controller (MVC) pattern:
1. **Model (`TimelineModel`)**: Stores the project state—a sequence of `Clip` objects, each containing the source file, source trim range (start/end), and the position/duration on the global timeline.
2. **View (`TimelineWidget`)**: A `QGraphicsView` that visually represents the timeline. It allows users to drag clips to change their offset and resize them to trim the content.
3. **Controller (`TimelinePlayer`)**: Orchestrates the playback of the timeline. It monitors the current playhead position and instructs `XPlayer` to switch source files and seek when crossing clip boundaries.

### Critical Components and Files

#### 1. New Components (XVideoEditor Module)
- **`TimelineModel.h/cpp`**: Data structures for `Clip` and the project timeline.
- **`TimelineWidget.h/cpp`**: Custom `QGraphicsView` for the visual track. Includes `ClipItem` (a draggable/resizable `QGraphicsRectItem`).
- **`TimelinePlayer.h/cpp`**: A wrapper around `XPlayer` that manages multi-clip playback and seamless transitions.
- **`XVideoEditorWindow.h/cpp/ui`**: The main application window combining the timeline, the `XQtOpenGLWidget` preview, and global controls.

#### 2. Leveraged Existing Components
- **`XPlayer`**: Used as the low-level playback engine.
- **`XQtOpenGLWidget`**: Used as the high-performance preview window.
- **`XConvertor`**: Used for the final "Export" process via `StartConcat`.

---

## Implementation Steps

### Phase 1: Data Model and Visuals
1. **Implement `TimelineModel`**: Define the `Clip` structure and logic for managing the sequence of clips.
2. **Build `TimelineWidget`**: 
    - Implement the time-to-pixel coordinate transformation.
    - Create `ClipItem` with interaction handles for moving and trimming.
    - Implement the playhead (a vertical line) that moves in sync with the current time.
3. **Verification**: Verify that clips can be added to the timeline, dragged to different positions, and resized without glitches.

### Phase 2: Preview Playback Integration
1. **Implement `TimelinePlayer`**:
    - Create a state machine to track the "active clip".
    - When `currentTimelinePos >= activeClip.end`, automatically call `XPlayer::Open(nextClip.url)` and `XPlayer::Seek(nextClip.sourceStart)`.
2. **Connect Preview**: Instantiate `XQtOpenGLWidget` and link it to the `TimelinePlayer`.
3. **Verification**: Play a sequence of 3+ clips. Verify that the video transitions automatically from one clip to the next at the correct timeline boundary.

### Phase 3: Export Pipeline
1. **Export Logic**: Implement a conversion function that transforms the `TimelineModel` into the arguments required by `XConvertor::StartConcat` (`std::vector<std::string> urls`, `std::vector<std::pair<long long, long long>> seg_trims`).
2. **Export Trigger**: Add an "Export" button to the UI that invokes the `XConvertor` pipeline.
3. **Verification**: Export a complex timeline with rearranged and trimmed clips. Verify the resulting video matches the visual sequence.

### Phase 4: Final UI Assembly
1. **Main Window**: Layout the preview window, the timeline widget, and the control bar (Play/Pause, Add File, Export).
2. **Interactivity**: Ensure that clicking the timeline seeks the `TimelinePlayer` to the correct position.
3. **Verification**: Conduct a full end-to-end test from file import $\rightarrow$ visual editing $\rightarrow$ preview $\rightarrow$ export.

---

## Verification Plan

| Test Case | Action | Expected Result |
| :--- | :--- | :--- |
| **Visual Editing** | Drag and resize a clip on the timeline. | Model offsets and durations update instantly. |
| **Preview Flow** | Play a timeline with multiple clips. | Seamless transition between files; playhead moves linearly. |
| **Precision Trim** | Set a precise cut point in the timeline. | The exported video starts/ends exactly at the specified millisecond. |
| **Export Accuracy** | Export a shuffled sequence of clips. | Output file matches the visual order and trimming of the timeline. |
| **UI Stability** | Add 20+ clips to the timeline. | `QGraphicsView` remains responsive; no memory leaks during playback. |
