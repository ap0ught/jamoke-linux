#ifndef JAMOKE_GAME_H
#define JAMOKE_GAME_H

#include "Assets.h"
#include "Drinks.h"
#include <SDL.h>

#include <string>
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

  void toggleFps() { showFps = !showFps; }
  void clearScores() { bestScore = 0; }

 private:
  void pickRoster();
  void drawBoard(SDL_Renderer* r);
  void drawHead(SDL_Renderer* r);
  void drawControls(SDL_Renderer* r);
  void drawHud(SDL_Renderer* r);
  SDL_Rect boardRect() const;
  SDL_Rect serveRect() const;
  SDL_Rect headRect() const;
};

#endif