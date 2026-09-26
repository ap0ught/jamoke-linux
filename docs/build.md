# Build

Two build layers, one source of truth: `CMakeLists.txt` at the repo root.

## Targets

| Target | Kind | Contents |
|--------|------|----------|
| `jamoke_core` | static lib | All gameplay/media/data code: `Assets`, `Font`, `Drinks`, `Game` (every file except `main.cpp`) |
| `jamoke` | executable | The game (`src/main.cpp`) linked against `jamoke_core` |
| `jamoke_tests` | test executable | GTest suite under `tests/` (built only when `BUILD_TESTING`) |

The core library exists so tests link the same objects the game binary uses —
there is no separate implementation for tests.

## Requirements

- CMake >= 3.16 (presets require >= 3.21; the installed CMake is 4.x)
- C++17 (enforced via `CMAKE_CXX_STANDARD`)
- SDL2 + SDL2_image found through **pkg-config** (`pkg_check_modules`); no
  SDL2_ttf or SDL2_mixer are used

## Asset root

`jamoke_core` bakes `JAMOKE_DEFAULT_ASSETS` as a compile definition pointing at
the sibling unpacked Jamoke DATA tree
(`../jamoke/src/Data/Program_Executable_Files/DATA`). The binary accepts a
runtime override via the `JAMOKE_ASSETS` environment variable, which wins over
the baked default. Media never ships in this repo (EULA).

## Presets

`CMakePresets.json` defines a `dev` preset: Ninja generator, binary dir
`build/`, `-DCMAKE_BUILD_TYPE=Debug`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`. The
CLI and the CLion CMake profile share this one directory.

```sh
cmake --preset dev            # configure
cmake --build build           # build all targets
./build/jamoke                # run
ctest --test-dir build --output-on-failure   # run tests
```

## CLion

The project is a native CMake project (`CMakeWorkspace` marker in
`.idea/misc.xml`, `CPP_MODULE` module). On first open CLion creates a Debug
profile for the `jamoke` target or offers to import the `dev` preset; either
path uses its bundled CMake/Ninja.