# Meow Player

A lightweight, free and open-source media player for Windows. Plays almost any video or audio format (powered by [libmpv](https://mpv.io)), with a dark navy + pink/blue gradient UI.

**Developed by Gr8Meow**

## Features
- Plays any video / audio format libmpv supports (MP4, MKV, AVI, WEBM, MP3, FLAC, WAV, OGG, M4A ...)
- GPU hardware decoding (`hwdec=auto-safe`)
- Subtitle support (`.srt .ass .ssa .sub .vtt`) - auto-loads matching files, or load manually with **Subs**
- Drag & drop files, or open from the command line: `MeowPlayer.exe "movie.mkv"`
- Seek bar, volume, fullscreen
- In-app "Update available" banner (checks the latest GitHub release)
- Single small exe, no installer

## Keyboard / mouse
| Key | Action |
|---|---|
| Space / click video | Play / Pause |
| Left / Right | Seek -5s / +5s |
| Up / Down / Mouse wheel | Volume |
| F / F11 / double-click video | Fullscreen |
| Esc | Leave fullscreen |
| O | Open file |
| S | Load subtitle |

## Run
1. Download `MeowPlayer.zip` from the [Releases](../../releases) page.
2. Extract and run `MeowPlayer.exe` (`libmpv-2.dll` must stay in the same folder).

## Build from source
Requirements: CMake 3.16+ and either Visual Studio 2022 (MSVC) or MinGW-w64.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Then put `libmpv-2.dll` next to `MeowPlayer.exe`. Get a 64-bit libmpv build from
<https://sourceforge.net/projects/mpv-player-windows/files/libmpv/> (the `mpv-dev-x86_64` archive contains `libmpv-2.dll`).
The code loads libmpv at runtime, so no mpv headers or import library are needed to compile.

## Publishing a release (so the update banner works)
1. Set `GH_REPO` in `src/main.cpp` to `YourUser/MeowPlayer`.
2. Bump `APP_VERSION` (e.g. `1.1.0`), rebuild.
3. On GitHub: **Releases -> Draft a new release**, tag `v1.1.0`, attach `MeowPlayer.zip` (exe + `libmpv-2.dll`).
4. Older versions will show "Update available: v1.1.0" and open the release page on click.

## License
MIT - see [LICENSE](LICENSE). libmpv is licensed under LGPL/GPL; ship its license with the DLL.
