# Tests (`tests/`)

GTest suite wired into CTest (`include(CTest)` + `add_test`) and linked against
`jamoke_core` so tests exercise the same code the game runs.

## Setup (`TestHelper.h`)

- `JamokeTestEnvironment` (registered via `tests/Main.cpp`) is the global
  fixture: it forces `SDL_VIDEODRIVER=dummy` and `SDL_AUDIODRIVER=dummy`,
  `SDL_Init`s, and inits `IMG_Init(JPG|PNG)` **before** any test constructs
  SDL objects.
- `HeadlessSDLTest` provides an 800x600 hidden window + software renderer and
  tears it down per test.
- `TempDirGuard` writes an ephemeral `drinkmenu.txt`/`CustomerList.txt` tree so
  parsing tests never touch the real data.
- `getAssetRoot()` resolves the data dir the same way the app does
  (`JAMOKE_ASSETS` env, else `JAMOKE_DEFAULT_ASSETS`).

## Suites

| Suite | Covers |
|-------|--------|
| `TestDrinks` | Parse failures, canonicalization (`parse` tests), shots/beans probing, drink types/flavors, customer list, sprite macro tokens, `byId` pointer validity, real DATA integrity (179 drinks / 11 customers / sprites 0..10 in order) |
| `TestFont` | Height + width metric math |
| `HeadlessSDLTest` (TestAssets.cpp) | Asset indexing on directories (empty / missing / case-insensitive), missing texture/audio no-crash, real-asset loading, draw-safety |
| `GameTestFixture` | Round init/reset, difficulty time limits, selection cycling (beans/milk/size/flavor), serve validation (latte-steam / mocha-no-steam / mismatches), feedback & timer progression, full roster → Over, click hit-testing, all scenes render without crashing, boot-on-title + click-to-start, Esc pause overlay (run preserved, timer frozen, clicks swallowed), two-step confirmed quit-to-title (session record survives), session-best record + `clearScores` (and that a lower run never overwrites the record) |
| `RealDataFaces` | Plays the full 10-order real roster and asserts every customer shows a distinct face (the by-index fix) |

## Real-data convention

Tests that need the original art/data (`RealAssetDatabaseIntegrity`,
`RealDataFaces.*`, asset-loading tests) start with a skip guard:

```cpp
if (!fs::is_directory(root)) GTEST_SKIP() << "Asset root not available at: " << root;
```

so the suite stays green on machines without the DATA tree (CI, fresh clones)
while still enforcing the checks when data is present.

## Running

```sh
cmake --build build
ctest --test-dir build --output-on-failure    # or: ./build/jamoke_tests
```