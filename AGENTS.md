# Jamoke Linux — Agent Index

This file is an index only. Each entry points to a short doc that explains one
subsystem (its purpose, structure, and key invariants). Read the doc for the
part you are about to touch, not this file.

| Doc | Explains |
|-----|----------|
| [`docs/build.md`](docs/build.md) | CMake build (targets `jamoke_core`, `jamoke`, `jamoke_tests`), CMake presets, CLion, SDL2/SDL2_image deps, asset-root resolution |
| [`docs/app.md`](docs/app.md) | `src/main.cpp`: SDL init, 800x600 logical space, input scaling, main loop, F12/`--shot` capture, `--script` replay + `--window` |
| [`docs/assets.md`](docs/assets.md) | `src/Assets.h/.cpp`: case-insensitive media index, lazy textures, shared-device audio, keyed transparency |
| [`docs/data.md`](docs/data.md) | `src/Drinks.h/.cpp`: `drinkmenu.txt` / `CustomerList.txt` models + parsing, sprite tokens, pointer-safety rules |
| [`docs/game.md`](docs/game.md) | `src/Game.h/.cpp`: scenes, roster, round/build state, serve validation, scoring, feedback, face-by-index, HUD, hit rects |
| [`docs/font.md`](docs/font.md) | `src/Font.h/.cpp`: embedded 5x7 bitmap font, glyph table, outlines, width/advance rules |
| [`docs/tests.md`](docs/tests.md) | `tests/`: GTest/CTest setup, headless SDL fixtures, per-module suites, real-data tests and their skip guard, `tests/play/` scripted replays (`PlayScripts.*`) |
| [`docs/provenance.md`](docs/provenance.md) | Original RealArcade/GameHouse release: catalog ID, productID 7412, product GUID, wrapper layout, `.mez` inventory + decoded storefront content (cover/manual/overview/mezzanine.xml), what is lost, why RAWK can't unlock the exe |