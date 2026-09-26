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

- `ESC`: quit (Title/Over); during play, back to the title menu
  (`game.toTitle()` — the original opened the options overlay, which the
  remake doesn't have).
- `F2`: `assets.reloadArt()` — drops the lazy texture cache so all art is
  re-decoded from disk (the original "restarts the entire application,
  reloading in all the art").
- `F5`: restart the current game mid-play (`game.reset()`).
- `F9`: toggle the FPS readout (`game.showFps`, drawn in the HUD).
- `CTRL+ALT+C`: clear the session high score (`game.clearScores()`).
- `CTRL+D`: toggle vsync (`SDL_RenderSetVSync`, guarded by
  `SDL_VERSION_ATLEAST(2,0,18)` — the manual's "continuous vs double-buffer
  update" mode; unsupported drivers print but never crash).
- `ENTER` / `SPACE`: dismiss pop-ups — advances from the Title or Game Over
  screen (`game.reset()`).
- `F12`: framebuffer capture (dev tooling, below).
- Left mouse down: read `SDL_GetMouseState`, scale window coords into logical
  coords (`SDL_RenderGetLogicalSize` / `SDL_GetWindowSize` ratio), then
  `game.handleClick(x, y)`.

The smoothed FPS value is pushed into `game.fps` each frame (`0.9`/`0.1`
exponential average) for the F9 counter.

## Dev tooling

- `F12`: capture the framebuffer via `SDL_RenderReadPixels` into an RGBA
  surface (little-endian channel masks) and `IMG_SavePNG` to
  `jamoke_capture.png` at the logical size.
- `./build/jamoke --shot`: auto-captures once (frame 30, mid-game — the
  `Game` constructor calls `reset()`, which forces `Scene::Play`, so the
  capture is the live order board, not the title menu) and exits — used for
  offscreen pixel verification, e.g.
  `SDL_VIDEODRIVER=dummy ./build/jamoke --shot`.