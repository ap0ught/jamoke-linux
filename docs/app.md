# App entry point (`src/main.cpp`)

The thin shell that wires everything together and runs the frame loop. It does
not contain gameplay logic — only SDL lifecycle, input plumbing, and dev tooling.

## Startup order

1. `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)` then `IMG_Init(JPG | PNG)`.
2. Create an 800x600 window and a renderer, then
   `SDL_RenderSetLogicalSize(ren, 800, 600)`. **All game coordinates are a
   fixed 800x600 space**; the renderer scales to whatever the window is, and
   mouse input is mapped back into that space (see Input below).
3. Resolve the asset root: `JAMOKE_ASSETS` env var else the compiled
   `JAMOKE_DEFAULT_ASSETS`. Load `Assets`, then parse the drink DB for the
   game. Both are mandatory — the app exits with an error message if they fail.
4. Construct `Game` and enter the loop.

## Main loop

- Frame timing: `SDL_GetTicks()` delta as seconds, clamped to 0.1 s so a
  stall (debugger, alt-tab) doesn't nuke the timer on the next frame.
- Events are polled, then `game.update(dt)` and `game.render(ren)` are called
  each frame. `SDL_Delay(1)` keeps CPU usage sane at high refresh rates.

## Input

The remake implements the original game's hot-key table (recovered from the
2001 `manual.rtf` in the mezzanine archive — see `docs/provenance.md`):

- `ESC`: quit (Title/Over); during play, toggles the in-place menu overlay
  (`game.togglePause()`). The overlay **preserves the run** — score, customer
  and timer all survive — because the original's menu/options opened in place.
  `update()` and `handleClick()` both no-op while `game.paused`, so the
  customer timer cannot drain behind the menu.
- `Q`: on the menu overlay, quit to the title menu. Destructive, so it is
  **two-step**: the first press arms `quitPending` and shows a warning, the
  second abandons the run (`game.requestQuitToTitle()`). This is deliberate —
  a single stray Esc must never cost the player a run.
- `F2`: `assets.reloadArt()` — drops the lazy texture cache so all art is
  re-decoded from disk (the original "restarts the entire application,
  reloading in all the art"). Safe at any time: nothing caches an
  `SDL_Texture*` outside `Assets`, so there is nothing left dangling.
- `F5`: restart the current game mid-play (`game.reset()`).
- `F9`: toggle the FPS readout (`game.showFps`, drawn in the HUD).
- `CTRL+ALT+C`: clear the session high score (`game.clearScores()`).
- `CTRL+D`: toggle vsync (`SDL_RenderSetVSync`, guarded by
  `SDL_VERSION_ATLEAST(2,0,18)` — the manual's "continuous vs double-buffer
  update" mode; unsupported drivers print but never crash).
- `ENTER` / `SPACE`: resume when the menu overlay is up; otherwise start a
  fresh run from the Title or Game Over screen (`game.startRun()`).
- `F12`: framebuffer capture (dev tooling, below).
- Left mouse down: uses the position carried by the event
  (`e.button.x/y`), **not** `SDL_GetMouseState` — the pointer may already have
  moved by the time the event is processed, which mis-routes clicks on a fast
  drag. The coordinates are mapped window → logical by `input::toLogical`
  (`src/Input.h`), so a click lands on the same control at any window size.

The smoothed FPS value is pushed into `game.fps` each frame (`0.9`/`0.1`
exponential average) for the F9 counter.

## Scripted play mode

`--script FILE` replays a session by pushing synthetic events through the *same*
event loop a human's mouse drives, and asserts on game state. This is how the
input path gets tested end to end — hit rectangles, input scaling, the serve
freeze and the pause overlay are all things a unit test on `handleClick` alone
cannot reach.

```sh
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./build/jamoke --script tests/play/cups.txt
```

| Command | Effect |
|---------|--------|
| `start` | push Enter (title → run) |
| `click X Y` | left click at **logical** `X Y`, converted to window pixels and back so the real translation is exercised |
| `key NAME` | push a key by SDL name (`Escape`, `F5`, `Return`, `Q`) |
| `build` | queue the clicks that make the drink match the current order (from the drink db) |
| `serve` | click the serve button |
| `wait N` | wait N frames |
| `waitsec S` | wait S seconds of wall time |
| `waitclear` | wait until the serve feedback pause finishes — prefer this over `waitsec`; a guessed duration is a race against the 1.6 s pause and losing it silently swallows later clicks |
| `state` | print every game-state field |
| `expect k=v` | assert one field (`size`, `flavor`, `score`, `served`, `scene`, `paused`, `frozen`, `msg`, …); failures print `FAIL` |
| `shot PATH` | capture a PNG to `PATH` |

A failing `expect` makes the process exit non-zero, so a script doubles as a CI
fixture; a script that runs out exits by itself. `modifier` combinations
(Ctrl-Alt-C, Ctrl-D) cannot be synthesized — `SDL_PushEvent` does not update
the keyboard state — so those stay covered by unit tests.

`--window WxH` overrides the window size while the logical space stays
800x600. Replaying the same script at several sizes is what proves the input
scaling is correct, and it is also what caught the capture overflow below.

Fixtures live in `tests/play/` and run from CTest as `PlayScripts.*` at 800x600,
1024x768 and 1600x1200 (`docs/tests.md`).

## Dev tooling

- `F12`: capture the framebuffer via `SDL_RenderReadPixels` into an RGBA
  surface (little-endian channel masks) and `IMG_SavePNG` to
  `jamoke_capture.png`. The surface is sized from `SDL_GetRendererOutputSize`,
  **not** the logical size: a null read rect reads the whole render target,
  which is window-sized, so allocating at 800x600 only works while the window
  happens to be exactly that size — a larger window overran the buffer and
  segfaulted.
- `./build/jamoke --shot`: auto-captures once (frame 30, the title screen —
  the `Game` constructor holds `Scene::Title` until the player starts) and
  exits — used for offscreen pixel verification, e.g.
  `SDL_VIDEODRIVER=dummy ./build/jamoke --shot`.
