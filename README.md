# CRT-84 Tetris

A small Windows terminal Tetris game written in C++17, with a CRT-inspired look and optional display effects designed to stay comfortable to read.

## Getting started

1. Install a Windows C++ toolchain such as [MSYS2](https://www.msys2.org/) with MinGW-w64 `g++` available on `PATH`.
2. Run `run.bat` from the project folder. It builds `main.cpp` and starts the game.

The included `CRT84_Tetris.exe` is a prebuilt copy. Running `run.bat` rebuilds it from source.

## Controls

| Key | Action |
| --- | --- |
| Left / Right arrows | Move piece |
| Up arrow | Rotate piece |
| Down arrow | Soft drop |
| Space | Hard drop |
| P | Pause or resume |
| R | Restart |
| Esc | Exit |
| F1 | Green phosphor palette |
| F2 | Amber CRT palette |
| F3 | Ice blue palette |
| F4 | Monochrome palette |
| F5 | Toggle scanlines (off by default) |
| F6 | Toggle ghost piece (off by default) |

## Build manually

From the project folder, run:

```sh
g++ -std=c++17 -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra main.cpp -o CRT84_Tetris.exe
```

The program uses the Windows console API and is intended for Windows.

## License

This project is distributed under the MIT License. See [LICENSE](LICENSE).
