# Implementation Plan: Volume Control for XVideoView

## Context
The current version of `XVideoView` allows for video playback and speed control but lacks a user-facing mechanism to adjust audio volume. The underlying audio engine, `XAudioPlay`, already supports volume adjustment via `SDL_MixAudio` using a value range of 0-128. This plan describes how to expose this functionality through the UI.

## Recommended Approach

We will bridge the UI layer (`XPlayVideo`) to the audio engine (`XAudioPlay`) by extending the playback controller (`XPlayer`).

### Critical Files to Modify

#### 1. Core Logic (`xcodec/`)
- [xcodec/xplayer.h](xcodec/xplayer.h): Add `SetVolume(float volume)` method.
- [xcodec/xplayer.cpp](xcodec/xplayer.cpp): Implement the mapping from 0.0-1.0 float to 0-128 integer for `XAudioPlay`.

#### 2. UI Layer (`xvideoview/`)
- [xvideoview/xplayvideo.ui](xvideoview/xplayvideo.ui): Add a `QSlider` for volume and an associated `QLabel`.
- [xvideoview/xplayvideo.h](xvideoview/xplayvideo.h): Declare a slot to handle volume changes.
- [xvideoview/xplayvideo.cpp](xvideoview/xplayvideo.cpp): Connect the slider signal to the player's volume method and manage the layout.

---

## Implementation Steps

### Step 1: Extend `XPlayer` Interface
- **Action**: Add `void SetVolume(float volume);` to the public section of `XPlayer` in `xplayer.h`.
- **Action**: Implement the method in `xplayer.cpp`:
  ```cpp
  void XPlayer::SetVolume(float volume) {
      int sdl_vol = (int)(volume * 128.0f);
      XAudioPlay::Instance()->set_volume(sdl_vol);
  }
  ```
- **Verification**: Project compiles successfully.

### Step 2: Update UI Layout
- **Action**: Modify `xplayvideo.ui` to include:
    - A `QSlider` (Horizontal) named `volume` with a range of `0` to `100`.
    - A `QLabel` named `vollabel` with the text "Volume:".
- **Action**: Add these widgets to the control bar in `xplayvideo.cpp` constructor, ensuring they are placed alongside the speed and position controls.
- **Verification**: Launch the app; the volume slider is visible in the control bar.

### Step 3: Connect UI to Core Logic
- **Action**: Add `void SetVolume();` as a slot in `XPlayVideo` in `xplayvideo.h`.
- **Action**: Implement the slot in `xplayvideo.cpp`:
  ```cpp
  void XPlayVideo::SetVolume() {
      float vol = ui.volume->value() / 100.0f;
      player.SetVolume(vol);
  }
  ```
- **Action**: In the `XPlayVideo` constructor, connect the slider's `valueChanged` signal to the `SetVolume` slot.
- **Verification**: Moving the slider triggers the `SetVolume` function.

---

## Verification Plan

### 1. Functional Testing
- **Action**: Open a video file with audio.
- **Test Cases**:
    - **Mute**: Slide the volume to `0`. Expected result: No audio output.
    - **Full Volume**: Slide the volume to `100`. Expected result: Audio at maximum level.
    - **Gradual Adjustment**: Move the slider slowly from 0 to 100. Expected result: Smooth increase in audio volume.

### 2. Boundary Analysis
- **Action**: Test values at the extreme ends (0, 1, 100).
- **Expected Result**: No crashes or audio clipping; `XAudioPlay` receives values strictly within the 0-128 range.

### 3. UI Consistency Check
- **Action**: Verify that the volume slider behaves consistently with the other controls (speed and position) in terms of layout and responsiveness.
- **Expected Result**: UI remains responsive and the slider is intuitively placed.
