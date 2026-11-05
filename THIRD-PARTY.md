# Third-party components

Meow Player is MIT licensed (see `LICENSE`). It uses the following third-party components.

## libmpv (media engine)
- Loaded at run time from `libmpv-2.dll`; not linked into the exe.
- Release zips ship an **LGPL** build from https://github.com/zhongfly/mpv-winbuild (pinned tag and SHA-256 are in `.github/workflows/build.yml` and `get-libmpv.ps1`).
- mpv: https://mpv.io  (LGPL-2.1-or-later for the LGPL build). Source code and license texts are available from the mpv project and the build repository above.

## Icons (`icons.h`)
- UI icons generated from an SVG icon set (Edge icons).
- Check the license of the icon set before publishing and keep its notice here.

## Fonts
- No font is bundled. Titles use an Ortoid font only if the user places one next to the exe. Check that font's license before redistributing it.
- Subtitle default: Nirmala UI (part of Windows).

## Microsoft Windows APIs
- Win32, GDI+, WinHTTP, DWM are part of Windows and are not redistributed.
