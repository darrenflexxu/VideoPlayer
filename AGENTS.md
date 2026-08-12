# VideoProc

Qt 5 (Widgets, C++14) video player + transcoder built on FFmpeg, SDL2, GLEW, OpenCV — all vendored under `thirdparty/`. Windows/MSVC only (root CMakeLists hardcodes the Qt prefix `C:/Qt/Qt5.12.12/5.12.12/msvc2017_64`).

## Build

- Configure with a multi-config generator, then build `RelWithDebInfo` (what the existing `build/` dir uses).
  ```
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config RelWithDebInfo
  ```
- Outputs go to `Bin/` (dll/exe) and `Lib/` (static archives) at the repo root, not `build/`. Runtime DLLs (`thirdparty/bin`, `thirdparty/opencv/bin`) are copied into the output dir via POST_BUILD.
- Sources are collected with `FILE(GLOB...)` in every subproject CMakeLists. **After adding/removing a file you must re-run `cmake` configure** (or the glob result is stale).
- `xcodec/` builds `XCodec` as a SHARED lib with a precompiled header (`predefine_header.h`) — keep FFmpeg includes in `extern "C"` blocks there.

## Binaries & structure

- `XCodec` (dll): everything under `xcodec/`. Transcode pipeline: `XDemuxTask` → `XDecodeTask` (video + audio) → `XEncodeTask` → `XMuxTask`, orchestrated by `XConvertor`. Playback: `XPlayer` (demux + decode + SDL/GL shader render, see `xvideo_view`, `xsdl`, `xshader*`).
- `XVideoView` (exe, `xvideoview/`): player window; takes a file path via argv or opens a file dialog.
- `XVideoTranscode` (exe, `xvideotranscode/`): transcode dialog; makes UI calls into `XConvertor`.
- The root CMakeLists builds ONLY `xcodec/`, `xvideoview/`, `xvideotranscode/`. `Form/`, `VideoPlayer/`, `qml/`, `FramelessHelper/`, `main.cpp`, `main_console.cpp` are legacy (each has its own standalone CMakeLists) and are NOT compiled into the current executables — don't edit them expecting changes to take effect.

## Git gotchas

- `.gitignore` is just `*`, so **everything is ignored by default** — new files don't show in `git status` and must be added with `git add -f`, including new source files.
- `thirdparty/` was force-added and is large (full FFmpeg/OpenCV/SDL2 SDKs) — don't delete or commit churn in it.

## Conventions

- Commit messages, comments, and in-source `cout` debugging are in Chinese; UI strings are hardcoded Chinese literals. Keep that style.
- No tests, no lint/format config. Verification is: build + run manually.