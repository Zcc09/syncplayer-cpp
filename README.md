# SyncPlayer 2.0

A native Windows rebuild of SyncPlayer: two mpv players side by side, kept in lock step
with each other, so you can watch a movie and the reaction to it at the same moment.

This is the C++ version. The original Python build lives at
https://github.com/Zcc09/SyncPlayer and continues to be maintained there; this repository
is the native rewrite, which exists mainly for the interface, because a native window can
look and behave like the rest of Windows in a way a Tk window cannot.

## Requirements

- Windows 10 or 11, 64-bit.
- To build: Visual Studio Build Tools with the C++ workload, CMake, and Ninja.
- To run: mpv. The installer bundles it, but the app also finds it on PATH or in a
  standard install directory.

## Building

```
tools\build.bat            app and core
tools\build.bat OFF        core and tests only, no application
tools\build.bat ON test    build, then run the smoke test
```

The build writes to `build\`. `SyncPlayer.exe` is the application; `sp_smoke.exe` starts
two real mpv processes and checks the synchronisation against them.

The equivalent commands by hand, after running `vcvars64.bat`:

```
cmake -S . -B build -G Ninja -DSP_BUILD_APP=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Using it

1. Open the app on the **Sources** tab, choose your movie and the reaction file, then
   press **Start**. Both load paused, and the two video windows are arranged side by side.
2. Press **Play**. From then on the app watches both players and keeps them together: a
   real gap is corrected with a seek, and small drift is absorbed by trimming the
   reaction's playback rate by a few percent, which is inaudible.
3. If the reaction is out of step by a set amount - a commentary track that starts late,
   for instance - type the difference into **Offset** on the **Sync** tab and press
   Enter. Positive means the reaction runs ahead. It is remembered for that pair of
   files, so next time it is applied before anything plays, and it can also be typed as
   a timecode such as `1:05`.
4. Drag either timeline to move one side, or lock sync and use the master bar to move
   both while keeping the offset.

### The groups

The groups run down one column, the way the Python build arranges them, and the column
scrolls with the wheel or the scrollbar:

- videos, and the buttons that start them
- playback, with the timelines and the alignment offset
- volume for each side and overall
- the window arrangement and floating picture-in-picture
- appearance, the status bar, and the paths the app is actually using

If you would rather see one group at a time, turn on **Split the groups into tabs** under
Arrangement; the same groups then become four tabs, and the window's minimum height rises
to fit a whole tab. The choice is remembered.

### Keyboard

| Key | Action |
| --- | --- |
| `Space` | Play or pause both videos |
| `Left` / `Right` | Jump back or forward by the jump amount |
| `Ctrl+1` to `Ctrl+4` | Switch tabs |
| `Ctrl+Tab` | Next tab |

### Timeline dragging

Seek bars use precise scrubbing: dragging sideways along the bar moves the position at a
fixed, slow rate, so a short drag is a few seconds rather than a large jump. Lifting the
cursor away from the bar while still dragging makes it finer still.

## Settings

The app reads and writes the same settings file as the Python version,
`%APPDATA%\SyncPlayer\syncplayer_config.json`, so your sources and remembered
alignments carry over between the two. The theme and the last tab are stored there too;
the Python build ignores those keys.

## Where things live

- `%APPDATA%\SyncPlayer` - settings and remembered alignments.
- `%USERPROFILE%\Pictures\SyncPlayer` - screenshots and mpv logs.

## Status

The core, the interface and the four tabs are in place: launching both players, the sync
loop, seeking and scrubbing, the typed offset, volume, window arrangement, floating
picture-in-picture, and the settings.

Not done yet:

- The crop and capture tool, the download window, and the in-app updater are still in the
  Python version. Until they are ported, this build cannot download a link or crop a
  frame.
- Subtitles and audio track selection are not exposed yet.
- The Linux build of this version has not been done.

`PORTING.md` records the state of the port and why the interface is built the way it is.
`PORTING-REFERENCE.md` holds the original Python source of every function the port has to
reproduce, so the behaviour can be compared rather than guessed at.

## Scaling

The interface is laid out in device-independent pixels and follows the display's scaling,
so text and controls are the same apparent size whether Windows is at 100%, 150% or 200%.
On a high-resolution screen that means the captions stay readable rather than shrinking
with the pixel pitch.

## Performance

The application draws nothing when nothing is happening: an idle window sits at 0.00% of
a CPU core, roughly seven frames a second are drawn while playing, and there is no timer
running at all until videos are loaded. The executable is under 500 KB and imports no
Visual C++ runtime, so it has no redistributable to install.

## Layout

```
src/core/        JSON, settings, the sync policy, mpv discovery and the mpv transport
src/platform/    Windows window finding, arrangement and picture-in-picture
src/ui/          the Fluent theme, the Direct2D renderer, and the control set
src/app/         the panel: layout, drawing and the actions
src/main.cpp     the window, its chrome, and the message loop
tests/           the smoke test, against real mpv processes
tools/           the build script and the asset embedder
assets/          the Lua beacon and input.conf, extracted from the Python build
testmedia/       the two short clips the smoke test uses
```
