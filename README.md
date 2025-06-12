
<p align=center>
<img alt="logo" src="assets/icon.png" height="128" width="128">
</p>
<h1 align=center>mus2</h1>

Simple and fast music player. Rewrite of [mus](https://github.com/thisisignitedoreo/mus).

Features:
- MP3 and FLAC tag support.
- Album scanning in directory trees.
- Simple browse tab and a search box.

## Quickstart
Download a binary from releases, or build from source:
```console
$ python bundle.py  # todo: integrate this into build.zig
...
$ zig build [-Doptimize=ReleaseFast]
```

Currently does not support cross-compiling because of raylibs system library dependency on X11 and wayland stuff.
Probably should crosscompile from Linux to Windows, not tested.

## Gallery
![Screenshot 1](screenshots/screenshot1.png)
![Screenshot 2](screenshots/screenshot2.png)
![Screenshot 3](screenshots/screenshot3.png)
![Screenshot 4](screenshots/screenshot4.png)

## Thanks to
- @raysan5 for the amazing raylib library
- @googlefonts for a font for this app (OpenSans)
- @google for all the icons for this app
- authors of adapted themes

