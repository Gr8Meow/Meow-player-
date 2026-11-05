# Meow Player

A fast, light, free and open-source media player for Windows 10/11, developed by Gr8Meow. It plays almost any audio or video format and has a clean, themeable interface.

## Features

- **Any format**: MP3, FLAC, WAV, OGG, OPUS, AAC, M4A, WMA, APE, MP4, MKV, AVI, MOV, WEBM, FLV, TS and many more (powered by libmpv)
- **Queue**: open files or whole folders, drag and drop, reorder, remove, shuffle, repeat (off / all / one)
- **Video**: fullscreen, audio track switching, playback speed (0.5x to 2x)
- **Subtitles**: Subtitles panel with all tracks, add any `.srt` `.ass` `.ssa` `.vtt` `.sub` `.sup` `.idx` `.smi` `.sbv` file (button or drag and drop), automatic loading of subtitle files next to the video, delay and size controls, font choice (Nirmala UI for Sinhala and Tamil), optional dark box
- **Music**: shows embedded cover art, title, artist and album
- **Resume**: reopens your queue and position, paused
- **Global media keys**: play/pause, next, previous, stop
- **Themes**: Mint, Pink, Purple, Ocean, Sunset, Mono
- **Update notice**: shows "Update v1.x" when you publish a new GitHub release (checks once a day, can be turned off)
- **"Open with" menu** integration, single instance (double-clicking a file adds it to the running player)

## Optimised for low resource use

- Hardware video decoding (GPU)
- No timers or polling: the app sleeps until something happens
- Video decoding stops while the window is minimized, music keeps playing
- Small demuxer cache, no scripts, no background services

## Install

1. Download `MeowPlayer-windows-x64.zip` from the Releases page and unzip it.
2. Run `meowplayer.exe`.

Building from source? Run `get-libmpv.ps1` once to download the pinned, checksum-verified `libmpv-2.dll` next to the exe.

## Keyboard

| Key | Action |
| --- | --- |
| Space | Play / pause |
| Left / Right | Seek 5 s (hold Shift for 30 s) |
| Up / Down | Volume |
| N / P | Next / previous |
| M | Mute |
| F or F11 | Fullscreen |
| S / A | Cycle subtitles / audio track |
| Z / X | Subtitle delay -0.1 s / +0.1 s |
| V | Show / hide subtitles |
| [ / ] | Slower / faster |
| O | Open files |
| Delete | Remove selected from queue |
| Esc | Leave fullscreen / close settings |

Subtitles: click **Subs** (top right) or the **Subtitles** tab, then **Add subtitle file**. You can also drop a subtitle file onto the window while a video plays, or drop it together with a video.

Mouse: click a video to pause, double-click for fullscreen, scroll for volume.

## Build from source

Files: `meowplayer.cpp`, `updatecheck.h`, `icons.h`.

MSVC:

```
cl /O2 /EHsc /MT /GS /guard:cf /DUNICODE /D_UNICODE meowplayer.cpp /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /guard:cf /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /DEPENDENTLOADFLAG:0x800 user32.lib gdi32.lib shell32.lib ole32.lib comdlg32.lib advapi32.lib shlwapi.lib dwmapi.lib gdiplus.lib winhttp.lib
```

MinGW:

```
g++ -O2 -fstack-protector-strong -municode -mwindows -static meowplayer.cpp -o meowplayer.exe -Wl,--dynamicbase,--nxcompat,--high-entropy-va -lgdiplus -lwinhttp -lole32 -lshell32 -luser32 -lgdi32 -lcomdlg32 -ladvapi32 -lshlwapi -ldwmapi -luuid
```

Or push to GitHub: the workflow builds the exe, downloads and verifies libmpv, and a tag like `v1.0` publishes a ready-to-run zip.

libmpv is loaded at run time, so the source needs no mpv headers or import libraries.

### Regenerating the icons

```
pip install cairosvg
python tools/make_icons.py path/to/Edge_Icons/SVG/Regular icons.h
```

## Custom font

Put an Ortoid font file (`ortoid*.ttf` or `*.otf`) next to `meowplayer.exe` or in a `fonts` folder and the titles use it. Check the font's license before redistributing it.

## Publishing updates ("Update available")

The app checks `https://github.com/<owner>/<repo>/releases/latest` once a day. To ship an update:

1. Push your changes to GitHub.
2. Create and push a new tag with a higher number, for example `git tag v1.1.0` then `git push origin v1.1.0`.
3. The workflow builds the zip and publishes the release.
4. Everyone running version 1.1.0 or newer of Meow Player sees **Update v1.2.0** (header and Settings). Clicking it opens the release page in the browser.

The version number and repository name are filled in by the workflow from your tag and repo (`version.h`), so there is nothing to edit. A local build without `version.h` has no repository built in; it then reads `owner/repo` from a file called `update-repo.txt` next to the exe (handy for testing and forks), and without that file update checks are off. Official builds ignore `update-repo.txt`. You can also bake it in by creating `version.h`:

```
#define APP_VERSION "1.0.0"
#define UPDATE_REPO "your-name/your-repo"
```

The notice only shows for a **published release that is not a draft or pre-release**. Nothing is downloaded or installed automatically.

## Security

The only network request is the optional daily update check described above. No admin rights, no external programs launched. Builds use stack protection, ASLR, DEP, Control Flow Guard and system-only DLL loading. See `SECURITY.md`.

## License

MIT, copyright Gr8Meow (see `LICENSE`). Third-party components are listed in `THIRD-PARTY.md`. Check the Craftwork icon license before publishing.
