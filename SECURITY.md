# Security

Meow Player is developed by Gr8Meow.

## What the app does and does not do

- Makes **one optional network request**: once a day it asks `github.com` for the latest release of this project (HTTPS, certificate checks on) so it can show "Update available". Nothing is downloaded or installed automatically, and no personal data is sent. You can turn it off in Settings > Check for updates. The repository it asks about is fixed inside official builds.
- Never launches other programs (the media engine's youtube-dl hook and scripts are disabled)
- Runs as a normal user and does **not** need administrator rights
- Loads only system DLLs plus `libmpv-2.dll` from its own folder
- Stores settings only in `%APPDATA%\MeowPlayer\settings.txt`

## Reporting a problem

Please open a private security advisory on the GitHub repository (Security tab,
"Report a vulnerability"). Include your Windows version and steps to reproduce.

## Verifying a download

Each release includes a `.sha256` file. Compare it with:

    certutil -hashfile MeowPlayer-windows-x64.zip SHA256

The exe is not code-signed, so Windows SmartScreen may warn the first time.
