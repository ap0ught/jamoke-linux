# jamoke-linux

A Linux-native, asset-based remake of the 2001 arcade coffee game **Jamoke**
(WizBang / Real Networks), written in C++17 + SDL2. It reuses the art, audio,
and recipe data extracted from the original game's `DATA` tree; the gameplay is
reimplemented from scratch (the original executables were DirectX 7, WizBang
-LibV3- engine, and cannot run on Linux).

## Status: Milestone 2

Playable Slice + original hot-key behavior:

- Title screen on boot (click, Enter or Space to start)
- 10-customer shift; each customer orders a drink
- Build on the machine: grind regular/decaf/split beans, 1-3 shots,
  whole/nonfat milk, steam toggle, 8 syrup flavors
- Serve to validate; correct = tip + cash-register jingle, wrong/too slow =
  angry face + buzzer
- Thermometer countdown per customer
- Game-over card with final score and session best (click / Enter to restart)
- Original 2001 hot keys from the recovered manual: Esc menu, F2 reload art,
  F5 restart, F9 fps counter, Ctrl-Alt-C clear scores, Ctrl-D vsync

## Build

Requires: CMake >= 3.16, a C++17 compiler, SDL2, SDL2_image.

```sh
cmake -B build
cmake --build build
./build/jamoke
```

## Assets

The default asset root is `../jamoke/src/Data/Program_Executable_Files/DATA`
(relative to this repo). Override at runtime:

```sh
JAMOKE_ASSETS=/path/to/DATA ./build/jamoke
```

The original game's media stays out of the repository, per the RealNetworks
EULA; only the source and data parsers are checked in.

## Controls

- Mouse: click machine parts to build, click the order board button to serve.
- Enter / Space: start / restart.
- Esc: menu overlay mid-game (run preserved); on Title/Over quits. Q quits to title (confirmed).
- F2 reload art, F5 restart, F9 fps counter, Ctrl-Alt-C clear scores,
  Ctrl-D vsync toggle, F12 screenshot.