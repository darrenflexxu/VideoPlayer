# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Run Commands

The project uses CMake and depends on Qt5, FFmpeg, SDL2, GLEW, and OpenCV.

### Building
```bash
mkdir build && cd build
cmake ..
# Use a build tool like make or ninja
make
```

### Common Targets
- `XCodec`: Shared library providing core video/audio decoding and playback logic.
- `XVideoView`: Video viewer application.
- `XVideoTranscode`: Video transcoding application with a UI.
- `xvideotranscode_cli`: Command-line interface for video transcoding.

## Architecture Overview

VideoProc is a video processing suite centered around the `XCodec` library.

### Core Components

- **XCodec (Core Library)**:
    - Handles low-level demuxing, decoding, and synchronization using **FFmpeg**.
    - Uses **SDL2** specifically for audio playback.
    - Implements a multi-threaded pipeline:
        - **Read Thread**: Reads packets from the file into queues.
        - **Decode Thread**: Decodes video frames and manages synchronization.
        - **Audio Thread**: Managed by SDL callbacks for audio output.
    - Provides a callback-based interface (`IVideoPlayer`) to remain platform-agnostic and decouple core logic from the UI.

- **VideoPlayer**:
    - The primary implementation of `IVideoPlayer`.
    - Manages `AVFormatContext`, `AVCodecContext`, and handles the A/V sync clock.
    - Implements packet queuing (`video_pack_list_`, `audio_pack_list_`) and conditional synchronization.

- **UI Layers**:
    - **Form/**: Qt-based widgets and main window implementation.
    - **qml/**: QML files for custom window decorations and UI components.
    - **FramelessHelper**: Utility for creating frameless windows with native-like behavior on different platforms.

- **Transcoding**:
    - `xvideotranscode` provides functionality to convert video files, utilizing FFmpeg's encoding capabilities.

### Project Structure
- `/VideoPlayer`: Core C++ playback implementation.
- `/xcodec`: Library wrapper and platform-specific glue.
- `/Form`: Qt GUI components.
- `/thirdparty`: External dependencies (FFmpeg, SDL2, OpenCV, GLEW).
- `/res`: UI assets and icons.
- `/translate`: Internationalization files (Qt `.ts`/`.qm`).

## Development Notes
- **C++ Standard**: The project requires C++14.
- **Encoding**: Source files are UTF-8. On Windows, `/utf-8` is used to ensure correct character handling.
- **Synchronization**: The player uses a custom `Mutex` and `Cond` implementation for thread synchronization.
