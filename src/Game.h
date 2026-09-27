#ifndef JAMOKE_GAME_H
#define JAMOKE_GAME_H

#include "Assets.h"
#include "Drinks.h"
#include <SDL.h>

#include <string>
#include <utility>
#include <vector>

struct Game {
  enum class Scene { Title, Play, Over };

  Assets& a;
  DrinkDB& db;

  Scene scene = Scene::Title;

  std::vector<const Customer*> roster;
  size_t idx = 0;

  // Current customer state.
  const Drink* target = nullptr;
  std::string head;         // base head tex key (lowercase rel path)
  std::string headB;        // angry variant ("" if none)
  double timeLeft = 0.0;
  double timeTotal = 1.0;
  bool voiced = false;

  // Build state.
  std::string size = "short";
  int shots = 1;
  std::string bean = "regular";  // regular / decaf / split
  std::string milk = "whole";    // whole / nonfat
  bool steamed = false;
  std::string flavor = "none";

  // Feedback timers.
  double resolveT = 0.0;      // >0 -> showing angry/ok face before advancing
  bool resolveResult = false; // true = was correct that round
  std::string resolveMsg;

  // Run-wide progress across the 10-customer roster.
  int score = 0;
  int served = 0;
  int bestScore = 0;  // session record; cleared with Ctrl-Alt-C

  // Debug readouts (original hotkeys): F9 toggles the FPS counter, Ctrl-D
  // toggles vsync (the manual's "continuous vs double-buffer update").
  bool showFps = false;
  double fps = 0.0;

  // Esc during play opens the original's in-place menu/options overlay. The
  // run is preserved (score, customer, timer) so quitting back to the title
  // stays an explicit choice rather than a stray keypress.
  bool paused = false;
  bool quitPending = false;  // first Q press arms it; a second Q confirms

  bool overWav = false;

  explicit Game(Assets& a, DrinkDB& db);

  void reset();
  void startRun();           // Title/Over -> begin a fresh run
  void togglePause();        // Esc during play: menu overlay on/off
  void requestQuitToTitle(); // Q on the menu overlay (two-step confirm)
  void toTitle();            // abandon the run and return to the title menu
  void newRound();
  void selectBean(const std::string& b);
  void selectMilk(const std::string& m);
  void selectSize(const std::string& s);
  void selectFlavor(const std::string& f);
  void serve();

  void handleClick(int x, int y);
  void update(double dt);
  void render(SDL_Renderer* r);

  // Every clickable control's rectangle, in the fixed 800x600 logical space.
  //
  // This is deliberately ONE table read by both the hit test (handleClick) and
  // the drawing (drawControls). They used to be two independent sets of
  // numbers that merely sat near each other -- the drawn cup was 50x98 while
  // the clickable box was 54x130, the syrup shelf was 40x110 drawn against
  // 44x115 clicked, and the serve control was clickable in a region with no
  // art at all. Nothing tied them together, the unit tests cannot see art, and
  // that is exactly how "clicking the tall cup does nothing" becomes possible.
  struct Controls {
    SDL_Rect serve;         // ORDER button, left sidebar (see controls.cpp)
    SDL_Rect beanRegular;
    SDL_Rect beanDecaf;
    SDL_Rect milkWhole;
    SDL_Rect milkNonfat;
    SDL_Rect cupShort;
    SDL_Rect cupTall;
    SDL_Rect cupGrande;
    SDL_Rect shot1;
    SDL_Rect shot2;
    SDL_Rect shot3;
    SDL_Rect steamer;
    std::vector<std::pair<std::string, SDL_Rect>> syrups;  // flavor -> rect
  };
  static const Controls& controls();

  // Syrup bottle rectangles keyed by flavor name, for dev tooling that needs
  // to aim at the real controls (the --script play mode) rather than keep its
  // own copy of the coordinates.
  static const std::vector<std::pair<std::string, SDL_Rect>>& syrupRects();

  void toggleFps() { showFps = !showFps; }
  void clearScores() { bestScore = 0; }

 private:
  void pickRoster();
  void drawBoard(SDL_Renderer* r);
  void drawHead(SDL_Renderer* r);
  void drawControls(SDL_Renderer* r);
  void drawHud(SDL_Renderer* r);
  SDL_Rect boardRect() const;
  SDL_Rect headRect() const;
};

#endif