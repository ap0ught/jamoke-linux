#include "Assets.h"
#include "Drinks.h"
#include "Game.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cstdlib>
#include <cstdio>

#ifndef JAMOKE_DEFAULT_ASSETS
#define JAMOKE_DEFAULT_ASSETS ""
#endif

int main(int argc, char** argv) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
    std::fprintf(stderr, "IMG_Init: %s\n", IMG_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Window* win = SDL_CreateWindow("Jamoke - Linux Remake", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 800, 600, 0);
  if (!win) {
    std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  if (!ren) {
    std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    return 1;
  }
  // All game coordinates live in a fixed 800x600 space; the renderer scales
  // to the window and main.cpp maps mouse input back into this space.
  SDL_RenderSetLogicalSize(ren, 800, 600);

  // JAMOKE_ASSETS overrides the compile-time default baked in by CMake
  // (which points at the sibling unpacked Jamoke DATA directory).
  const char* root = std::getenv("JAMOKE_ASSETS");
  if (!root || !*root) root = JAMOKE_DEFAULT_ASSETS;

  Assets assets(ren);
  if (!assets.loadRoot(root)) {
    std::fprintf(stderr, "Could not load asset root: %s\n"
                         "Pass -DJAMOKE_DEFAULT_ASSETS=... or set JAMOKE_ASSETS.\n",
                 root);
    return 1;
  }

  DrinkDB db;
  if (!db.load(assets.root)) {
    std::fprintf(stderr, "Could not parse drinkmenu.txt/CustomerList.txt under %s\n",
                 assets.root.c_str());
    return 1;
  }
  std::printf("drinks=%zu customers=%zu\n", db.drinks.size(), db.customers.size());

  Game game(assets, db);

  int vsync = 1;
#if SDL_VERSION_ATLEAST(2, 0, 18)
  SDL_RenderSetVSync(ren, vsync);
#endif

  bool quit = false;
  bool doShot = false;
  bool autoShot = (argc > 1 && std::string(argv[1]) == "--shot");
  Uint32 prev = SDL_GetTicks();
  int frames = 0;
  while (!quit) {
    Uint32 now = SDL_GetTicks();
    double dt = (double)(now - prev) / 1000.0;
    if (dt > 0.1) dt = 0.1;  // clamp after stalls (debugger, alt-tab)
    prev = now;
    // Smoothed frame-rate readout for the F9 counter.
    double fpsInst = (dt > 0.0) ? 1.0 / dt : 0.0;
    game.fps = (game.fps == 0.0) ? fpsInst : game.fps * 0.9 + fpsInst * 0.1;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) {
        quit = true;
      } else if (e.type == SDL_KEYDOWN) {
        const Uint8* ks = SDL_GetKeyboardState(nullptr);
        bool ctrl = ks[SDL_SCANCODE_LCTRL] || ks[SDL_SCANCODE_RCTRL];
        bool alt = ks[SDL_SCANCODE_LALT] || ks[SDL_SCANCODE_RALT];
        SDL_Keycode k = e.key.keysym.sym;

        // Original manual hotkeys: Esc = in-place menu/options (run preserved),
        // F2 = reload all art, F5 = restart the game, F9 = fps counter,
        // Ctrl-Alt-C = clear high scores, Ctrl-D = update/vsync mode,
        // Enter/Space dismiss pop-ups (start here). F12 stays the capture key.
        if (k == SDLK_ESCAPE) {
          if (game.scene == Game::Scene::Play) game.togglePause();
          else quit = true;
        }
        if (k == SDLK_q) game.requestQuitToTitle();
        if (k == SDLK_F2) assets.reloadArt();
        if (k == SDLK_F5 && game.scene == Game::Scene::Play) game.reset();
        if (k == SDLK_F9) game.toggleFps();
        if (k == SDLK_F12) doShot = true;
        if (k == SDLK_c && ctrl && alt) game.clearScores();
#if SDL_VERSION_ATLEAST(2, 0, 18)
        if (k == SDLK_d && ctrl && !alt) {
          vsync = !vsync;
          if (SDL_RenderSetVSync(ren, vsync) != 0)
            std::fprintf(stderr, "SDL_RenderSetVSync: %s\n", SDL_GetError());
          std::printf("vsync %s\n", vsync ? "on" : "off");
        }
#else
        (void)vsync;
#endif
        if (k == SDLK_RETURN || k == SDLK_SPACE) {
          if (game.paused) {
            game.togglePause();  // menu overlay: Enter resumes
          } else if (game.scene == Game::Scene::Title ||
                     game.scene == Game::Scene::Over) {
            game.startRun();
          }
        }
      } else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);
        int lw = 0, lh = 0, ww = 0, wh = 0;
        SDL_RenderGetLogicalSize(ren, &lw, &lh);
        SDL_GetWindowSize(win, &ww, &wh);
        // Convert window coords to logical 800x600 coords so the game can
        // hit-test against hardcoded UI rectangles.
        if (lw > 0 && ww > 0) mx = (int)(mx * ((float)lw / ww));
        if (lh > 0 && wh > 0) my = (int)(my * ((float)lh / wh));
        game.handleClick(mx, my);
      }
    }

    game.update(dt);
    game.render(ren);
    ++frames;
    if (autoShot && frames == 30) doShot = true;  // grab the title frame once

    if (doShot) {
      doShot = false;
      int w = 0, h = 0;
      SDL_RenderGetLogicalSize(ren, &w, &h);
      // F12 screenshot: read the framebuffer back at logical size into an
      // RGBA surface (little-endian channel masks) and save as PNG.
      SDL_Surface* shot = SDL_CreateRGBSurface(
          0, w, h, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
      if (shot) {
        SDL_RenderReadPixels(ren, nullptr, shot->format->format, shot->pixels, shot->pitch);
        std::string path = "jamoke_capture.png";
        if (IMG_SavePNG(shot, path.c_str()) == 0)
          std::printf("captured %s (%dx%d)\n", path.c_str(), w, h);
        SDL_FreeSurface(shot);
        if (autoShot) quit = true;  // --shot: capture one frame and exit
      }
    }

    SDL_Delay(1);
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
  return 0;
}