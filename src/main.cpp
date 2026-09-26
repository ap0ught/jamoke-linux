#include "Assets.h"
#include "Drinks.h"
#include "Game.h"
#include "Input.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifndef JAMOKE_DEFAULT_ASSETS
#define JAMOKE_DEFAULT_ASSETS ""
#endif

namespace {

// One line of a --script play file. Ops: start | click X Y | key NAME |
// wait N | shot PATH | state | quit.
struct ScriptCmd {
  std::string op;
  int a = 0;
  int b = 0;
  std::string s;
};

std::vector<ScriptCmd> loadScript(const std::string& path, bool* ok) {
  std::vector<ScriptCmd> cmds;
  std::ifstream in(path);
  *ok = in.good();
  if (!in) return cmds;
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream ls(line);
    ScriptCmd c;
    ls >> c.op;
    if (c.op == "click") ls >> c.a >> c.b;
    else if (c.op == "wait" || c.op == "waitsec") ls >> c.a;
    // waitclear takes no argument.
    else if (c.op == "key" || c.op == "shot" || c.op == "expect") ls >> c.s;
    if (!c.op.empty()) cmds.push_back(c);
  }
  return cmds;
}

const char* sceneName(Game::Scene s) {
  switch (s) {
    case Game::Scene::Title: return "Title";
    case Game::Scene::Play: return "Play";
    case Game::Scene::Over: return "Over";
  }
  return "?";
}

}  // namespace

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

  // --window WxH overrides the window size. The logical space stays 800x600,
  // so this is how the input scaling gets exercised for real: a click aimed at
  // a logical position must land on the same control at any window size.
  int winW = 800, winH = 600;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--window" && i + 1 < argc) {
      int w = 0, h = 0;
      if (std::sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
        winW = w;
        winH = h;
      }
    }
  }

  SDL_Window* win = SDL_CreateWindow("Jamoke - Linux Remake", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, winW, winH, 0);
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
  std::string shotPath = "jamoke_capture.png";
  bool autoShot = (argc > 1 && std::string(argv[1]) == "--shot");

  // Scripted play mode: drive the real event path with synthetic events so a
  // session can be replayed and asserted on without a human at the mouse.
  std::vector<ScriptCmd> script;
  size_t scriptPc = 0;
  int waitFrames = 0;
  Uint32 waitUntil = 0;      // SDL_GetTicks deadline for waitsec
  bool waitUntilClear = false;  // hold until the feedback pause clears
  int scriptFails = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--script" && i + 1 < argc) {
      bool ok = false;
      script = loadScript(argv[++i], &ok);
      if (!ok) {
        std::fprintf(stderr, "Could not read script: %s\n", argv[i]);
        return 1;
      }
      std::printf("script: %zu commands\n", script.size());
      // Replay needs game time to track wall time. With vsync on, a headless
      // driver throttles frames, so the 1.6s serve pause has not elapsed by the
      // next command and every later click gets swallowed by the feedback
      // freeze -- which looks exactly like a dead control.
      vsync = 0;
#if SDL_VERSION_ATLEAST(2, 0, 18)
      SDL_RenderSetVSync(ren, vsync);
#endif
    }
  }

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

    // Run the script's next command. Exactly one per frame, so a command that
    // follows a click always observes the state *after* that click was polled
    // and handled -- otherwise `click` + `state` in one frame would report the
    // pre-click state and quietly hide a broken control.
    if (scriptPc < script.size()) {
      // Gate on both kinds of wait without skipping the rest of the frame:
      // a `continue` here would skip render() and flicker the window.
      bool ready = true;
      if (waitUntil != 0) {
        if ((int)(SDL_GetTicks() - waitUntil) < 0) {
          ready = false;
        } else {
          waitUntil = 0;
        }
      }
      if (waitFrames > 0) {
        --waitFrames;
        ready = false;
      }
      // waitclear: hold until the serve feedback pause has finished. Waiting on
      // game state instead of a guessed duration is what makes a replay
      // deterministic -- a `waitsec 1.9` against a 1.6s pause is a race, and
      // losing it makes every later click look like a dead control.
      if (waitUntilClear) {
        if (game.resolveT > 0.0) {
          ready = false;
        } else {
          waitUntilClear = false;
        }
      }
      if (ready) {
        const ScriptCmd& c = script[scriptPc++];
        int lw = 0, lh = 0, ww = 0, wh = 0;
        SDL_RenderGetLogicalSize(ren, &lw, &lh);
        SDL_GetWindowSize(win, &ww, &wh);
        if (c.op == "wait") {
          waitFrames = c.a;
        } else if (c.op == "waitsec") {
          waitUntil = SDL_GetTicks() + (Uint32)(c.a * 1000);
        } else if (c.op == "waitclear") {
          waitUntilClear = true;
        } else if (c.op == "start") {
          SDL_Event e;
          SDL_zero(e);
          e.type = SDL_KEYDOWN;
          e.key.keysym.sym = SDLK_RETURN;
          SDL_PushEvent(&e);
        } else if (c.op == "click") {
          // Aim at a logical position by converting to window pixels; the
          // event handler converts straight back, so the real translation is
          // what gets tested.
          SDL_Event e;
          SDL_zero(e);
          e.type = SDL_MOUSEBUTTONDOWN;
          e.button.button = SDL_BUTTON_LEFT;
          e.button.state = SDL_PRESSED;
          e.button.x = input::toWindow(c.a, lw, ww);
          e.button.y = input::toWindow(c.b, lh, wh);
          SDL_PushEvent(&e);
        } else if (c.op == "build") {
          // Build the drink the current customer actually ordered, by queueing
          // the same clicks a player would make. This is what lets a script
          // play the game correctly instead of guessing: the order board shows
          // flavor+type / size+shots+milk / steamed, and a drink with no syrup
          // ("flavor=none") must get NO bottle clicked -- which is exactly the
          // mistake a hand-written script makes.
          const Drink* t = game.target;
          if (!t) {
            std::fprintf(stderr, "build: no current target\n");
          } else {
            std::vector<SDL_Point> clicks;
            if (t->size == "tall") clicks.push_back({701, 531});
            else if (t->size == "grande") clicks.push_back({757, 523});
            else clicks.push_back({644, 535});
            if (t->shots == 1) clicks.push_back({340, 270});
            else if (t->shots == 2) clicks.push_back({455, 270});
            else clicks.push_back({565, 270});
            if (t->bean == "decaf") clicks.push_back({170, 185});
            else if (t->bean == "split") clicks.push_back({170, 185});  // cycles
            clicks.push_back(t->milk == "nonfat" ? SDL_Point{206, 310}
                                                 : SDL_Point{124, 310});
            if (t->flavor != "none") {
              for (const auto& kv : game.syrupRects()) {
                if (kv.first == t->flavor) {
                  clicks.push_back({kv.second.x + kv.second.w / 2,
                                    kv.second.y + kv.second.h / 2});
                  break;
                }
              }
            }
            if (t->type == "latte") clicks.push_back({610, 145});
            for (const SDL_Point& p : clicks) {
              SDL_Event e;
              SDL_zero(e);
              e.type = SDL_MOUSEBUTTONDOWN;
              e.button.button = SDL_BUTTON_LEFT;
              e.button.state = SDL_PRESSED;
              e.button.x = input::toWindow(p.x, lw, ww);
              e.button.y = input::toWindow(p.y, lh, wh);
              SDL_PushEvent(&e);
            }
            std::printf("build: %s (%zu clicks)\n", t->desc.c_str(), clicks.size());
            std::fflush(stdout);
          }
        } else if (c.op == "serve") {
          SDL_Event e;
          SDL_zero(e);
          e.type = SDL_MOUSEBUTTONDOWN;
          e.button.button = SDL_BUTTON_LEFT;
          e.button.state = SDL_PRESSED;
          e.button.x = input::toWindow(650, lw, ww);
          e.button.y = input::toWindow(200, lh, wh);
          SDL_PushEvent(&e);
        } else if (c.op == "key") {
          SDL_Event e;
          SDL_zero(e);
          e.type = SDL_KEYDOWN;
          e.key.keysym.sym = SDL_GetKeyFromName(c.s.c_str());
          if (e.key.keysym.sym == SDLK_UNKNOWN)
            std::fprintf(stderr, "script: unknown key '%s'\n", c.s.c_str());
          SDL_PushEvent(&e);
        } else if (c.op == "shot") {
          shotPath = c.s;
          doShot = true;
        } else if (c.op == "state" || c.op == "expect") {
          auto fields = [&]() {
            std::map<std::string, std::string> m{
                {"scene", sceneName(game.scene)},
                {"idx", std::to_string(game.idx)},
                {"served", std::to_string(game.served)},
                {"score", std::to_string(game.score)},
                {"size", game.size},
                {"shots", std::to_string(game.shots)},
                {"bean", game.bean},
                {"milk", game.milk},
                {"steamed", std::to_string((int)game.steamed)},
                {"flavor", game.flavor},
                {"paused", std::to_string((int)game.paused)},
                {"best", std::to_string(game.bestScore)},
                {"frozen", std::to_string(game.resolveT > 0.0)},
                {"msg", game.resolveMsg.empty() ? "-" : game.resolveMsg},
            };
            return m;
          };
          auto f = fields();
          if (c.op == "state") {
            std::string line = "state";
            for (auto& kv : f) line += " " + kv.first + "=" + kv.second;
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
          } else {
            // expect key=value -> a real assertion, so a play script doubles
            // as a regression test.
            std::string want = c.s;
            auto eq = want.find('=');
            std::string k = eq == std::string::npos ? want : want.substr(0, eq);
            std::string v = eq == std::string::npos ? "" : want.substr(eq + 1);
            auto it = f.find(k);
            if (it == f.end()) {
              std::printf("FAIL expect %s (unknown key)\n", want.c_str());
              ++scriptFails;
            } else if (it->second != v) {
              std::printf("FAIL expect %s but got %s=%s\n", want.c_str(),
                          k.c_str(), it->second.c_str());
              ++scriptFails;
            } else {
              std::printf("ok   %s\n", want.c_str());
            }
            std::fflush(stdout);
          }
        } else if (c.op == "quit") {
          quit = true;
        }
      }
    }

    // A replay ends when its script does. Without this, a script that omits an
    // explicit `quit` (the common case for a CI fixture) would leave the game
    // running forever. The inner poll loop still runs this frame, so events
    // pushed by the final command are handled before we exit.
    if (!script.empty() && scriptPc >= script.size()) quit = true;

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
        // Use the position carried by the event, not SDL_GetMouseState: the
        // pointer may already have moved by the time this is processed, which
        // mis-routes clicks on a fast drag or a jerky pointer. It also lets the
        // --script mode replay a click at an exact position.
        int lw = 0, lh = 0, ww = 0, wh = 0;
        SDL_RenderGetLogicalSize(ren, &lw, &lh);
        SDL_GetWindowSize(win, &ww, &wh);
        // Convert window pixels into the logical 800x600 space so the game can
        // hit-test against hardcoded UI rectangles.
        int mx = input::toLogical(e.button.x, lw, ww);
        int my = input::toLogical(e.button.y, lh, wh);
        game.handleClick(mx, my);
      }
    }

    game.update(dt);
    game.render(ren);
    ++frames;
    if (autoShot && frames == 30) doShot = true;  // grab the title frame once

    if (doShot) {
      doShot = false;
      // Size the capture from the renderer's real output, NOT the logical
      // 800x600 space: SDL_RenderReadPixels with a null rect reads the entire
      // render target, which is window-sized. Allocating at the logical size
      // happens to work only while the window is exactly 800x600 -- any
      // larger window (a resize, or --window) overruns the buffer and
      // segfaults.
      int w = 0, h = 0;
      SDL_GetRendererOutputSize(ren, &w, &h);
      if (w <= 0 || h <= 0) {
        SDL_GetWindowSize(win, &w, &h);
      }
      // F12 screenshot: read the framebuffer back into an RGBA surface
      // (little-endian channel masks) and save as PNG.
      SDL_Surface* shot = SDL_CreateRGBSurface(
          0, w, h, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
      if (shot) {
        SDL_RenderReadPixels(ren, nullptr, shot->format->format, shot->pixels, shot->pitch);
        std::string path = shotPath;
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
  if (scriptFails > 0) {
    std::printf("script: %d expectation(s) FAILED\n", scriptFails);
    return 1;  // so a play script can gate CI
  }
  return 0;
}