#include "Game.h"
#include "Font.h"

#include <SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>

namespace {

struct BottleDef {
  std::string name;
  int x, y;
};

const std::vector<BottleDef>& bottlesList() {
  static const std::vector<BottleDef> list = {
      {"chocolate", 280, 44},
      {"almond", 328, 23},
      {"hazelnut", 373, 23},
      {"coconut", 418, 23},
      {"vanilla", 463, 23},
      {"orange", 508, 23},
      {"raspberry", 553, 23},
      {"mint", 598, 23},
  };
  return list;
}

const std::vector<std::string>& flavNames() {
  static const std::vector<std::string> v = {
      "chocolate", "almond", "hazelnut", "vanilla",
      "raspberry", "coconut", "orange", "mint"};
  return v;
}

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool inRect(const SDL_Rect& r, int x, int y) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

const char* shotWord(int n) {
  if (n == 2) return "double";
  if (n == 3) return "triple";
  return "single";
}

const std::string cap(std::string s) {
  if (s.empty()) return s;
  s[0] = (char)toupper((unsigned char)s[0]);
  return s;
}

std::string shotWordStr(int n) { return shotWord(n); }

// Index of this customer among the same-gender customers in the data file
// (data order == sprite order). Faces are picked from a per-gender pool by
// this index, so every roster slot shows a stable, distinct face that
// advances with the customer rather than with a global round counter.
int genderSeq(const DrinkDB& db, const Customer* c) {
  int n = 0;
  for (const auto& x : db.customers) {
    if (&x == c) return n;
    if (x.gender == c->gender) ++n;
  }
  return 0;
}

// Loads a face/cup BMP and derives its transparency key by sniffing the
// top-left pixel. The original artists baked any of several key colors
// (black, magenta, green) into the corner, so reading it back reproduces
// transparency without assuming one color; the plausibility check stops
// genuinely dark art from being keyed out entirely.
SDL_Texture* keyedTex(Assets& a, const std::string& rel) {
  return a.tex(rel);
}

}  // namespace

Game::Game(Assets& assets, DrinkDB& drinks) : a(assets), db(drinks) {
  reset();
}

void Game::reset() {
  idx = 0;
  score = 0;
  served = 0;
  resolveT = 0.0;
  size = "short";
  shots = 1;
  bean = "regular";
  milk = "whole";
  steamed = false;
  flavor = "none";
  overWav = false;
  pickRoster();
  newRound();
}

// Esc mid-game folds the original "options" behaviour into a trip back to
// the title menu (the remake has no options overlay to open).
void Game::toTitle() {
  reset();
  scene = Scene::Title;
  overWav = false;
}

void Game::pickRoster() {
  roster.clear();
  size_t n = db.customers.empty() ? 0 : db.customers.size();
  if (n == 0) return;
  // Prefer customers whose order resolves in the DB; if none do, fall back
  // to the raw list (newRound then guards the null target). The short pool
  // simply cycles to fill the 10-slot roster.
  std::vector<const Customer*> pool;
  for (const auto& c : db.customers)
    if (db.find(c.drinkId)) pool.push_back(&c);
  if (pool.empty()) {
    for (const auto& c : db.customers) pool.push_back(&c);
  }
  for (int i = 0; i < 10; ++i) {
    const Customer* c = pool[(size_t)i % pool.size()];
    roster.push_back(c);
  }
}

void Game::newRound() {
  if (idx >= roster.size()) {
    scene = Scene::Over;
    if (score > bestScore) bestScore = score;
    if (!overWav) {
      a.play("sounds/gameover.wav");
      overWav = true;
    }
    return;
  }
  const Customer* c = roster[idx];
  target = db.find(c->drinkId);
  if (!target) target = &db.drinks[0];

  scene = Scene::Play;
  timeTotal = 50.0;
  if (target->difficulty == "easy") timeTotal = 55.0;
  if (target->difficulty == "hard") timeTotal = 42.0;
  timeLeft = timeTotal;

  // Head art: one face per customer, chosen by the customer's own position
  // in the data (gender-ordered via genderSeq), not by a global round count,
  // so the same customer always shows the same face and the roster walks
  // through the face pool in order. Extras beyond the early roster are
  // appended so any pool size stays distinct.
  static const char* const M[] = {"M_DudeHead", "M_JimHead", "M_RyanHead",
                                  "M_ScottHead", "M_StuartHead", "M_BowtieHead",
                                  "M_DreadHead", "M_BlackHead", "M_ThomasHead",
                                  "M_SpikyHead", "M_SergioHead"};
  static const char* const F[] = {"F_RachelHead", "F_BlondeHead", "F_BlackHead",
                                  "F_RedHead", "F_MBHead", "F_AsianHead"};
  const char* const* list = (c->gender == "female") ? F : M;
  int count = (c->gender == "female") ? (int)(sizeof(F) / sizeof(F[0]))
                                      : (int)(sizeof(M) / sizeof(M[0]));
  std::string base = list[genderSeq(db, c) % count];
  head = lower("customers/" + base + ".bmp");
  headB = lower("customers/" + base + "B.bmp");
  // Not every head ships a "B" (angry) variant; check the index first and
  // clear the key so drawHead won't request a texture that doesn't exist.
  if (a.files.find(headB) == a.files.end()) headB.clear();

  voiced = false;
  resolveT = 0.0;
  size = "short";
  shots = 1;
  bean = "regular";
  milk = "whole";
  steamed = false;
  flavor = "none";
}

void Game::selectBean(const std::string& b) {
  // The decaf grinder toggle cycles decaf -> split -> regular so both the
  // "split" and "regular" beans stay reachable from the single button.
  if (b == "decaf" && bean == "decaf") bean = "split";
  else if (b == "decaf" && bean == "split") bean = "regular";
  else bean = b;
}

void Game::selectMilk(const std::string& m) { milk = m; }
void Game::selectSize(const std::string& s) { size = s; }
void Game::selectFlavor(const std::string& f) {
  // Tapping the bottle that's already active clears the syrup back to none.
  flavor = (f == flavor) ? "none" : f;
}

void Game::serve() {
  if (resolveT > 0.0) return;
  if (!target) return;
  // Grade the build against the order attribute by attribute. Steam is a
  // field only lattes require, so it participates in the comparison too.
  bool ok = true;
  if (target->size != size) ok = false;
  if (target->shots != shots) ok = false;
  if (target->bean != bean) ok = false;
  if (target->milk != milk) ok = false;
  if (target->flavor != flavor) ok = false;
  bool wantSteam = (target->type == "latte");
  if (steamed != wantSteam) ok = false;

  resolveResult = ok;
  resolveT = 1.6;
  if (ok) {
    int bonus = 0;
    if (timeLeft > 10.0) bonus = 25;  // fast service earns a tip
    int earned = 100 + bonus;
    score += earned;
    resolveMsg = "+" + std::to_string(earned) + "  CORRECT!";
    a.play("sounds/cashreg.wav");
  } else {
    resolveMsg = "WRONG - NO TIP";
    a.play("sounds/oops.wav");
  }
}

void Game::handleClick(int x, int y) {
  if (scene == Scene::Over) { reset(); return; }
  if (scene != Scene::Play || resolveT > 0.0) return;  // no input mid-feedback

  // x/y arrive already in the 800x600 logical space (main.cpp scales the
  // mouse), so every hit test below is a hardcoded UI rectangle.
  auto box = [&](SDL_Rect r) { return inRect(r, x, y); };

  // Serve.
  if (box(serveRect())) { serve(); return; }

  // Grinder beans: two buttons under the board, left column.
  if (box({92, 150, 64, 70})) { selectBean("regular"); a.play("sounds/grinder.wav"); return; }
  if (box({166, 150, 64, 70})) { selectBean("decaf"); a.play("sounds/grinder.wav"); return; }

  // Milk.
  if (box({92, 250, 68, 121})) { selectMilk("whole"); a.play("sounds/pourmilk.wav"); return; }
  if (box({172, 250, 68, 121})) { selectMilk("nonfat"); a.play("sounds/pourmilk.wav"); return; }

  // Cup size stacks (right counter).
  if (box({620, 480, 48, 110})) { selectSize("short"); a.play("sounds/cups.wav"); return; }
  if (box({672, 460, 54, 130})) { selectSize("tall"); a.play("sounds/cups.wav"); return; }
  if (box({730, 440, 60, 150})) { selectSize("grande"); a.play("sounds/cups.wav"); return; }

  // Espresso machine spigots and shot cups on drip tray (1, 2, or 3 shots).
  if (box({285, 230, 110, 80})) { shots = 1; a.play("sounds/grinder.wav"); return; }
  if (box({400, 230, 110, 80})) { shots = 2; a.play("sounds/grinder.wav"); return; }
  if (box({510, 230, 110, 80})) { shots = 3; a.play("sounds/grinder.wav"); return; }

  // Steamer control on espresso machine.
  if (box({580, 115, 60, 60})) { steamed = !steamed; a.play("sounds/steamer.wav"); return; }

  // Flavor syrup bottles on the top shelf.
  for (const auto& b : bottlesList()) {
    if (box({b.x, b.y, 44, 115})) { selectFlavor(b.name); return; }
  }
}

void Game::update(double dt) {
  if (scene != Scene::Play) return;

  // One order voice-over per customer: pick a file by gender and a rotation
  // of four recordings so repeats don't stall.
  if (!voiced) {
    voiced = true;
    int n = 1 + (int)(idx % 4);
    std::string g = (roster[idx]->gender == "female") ? "f" : "m";
    char vo[64];
    std::snprintf(vo, sizeof(vo), "voices/%s_order%02d.wav", g.c_str(), n);
    a.play(vo);
  }

  if (resolveT > 0.0) {
    // Feedback pause (correct/angry verdict) before the next customer.
    resolveT -= dt;
    if (resolveT <= 0.0) {
      ++served;
      ++idx;
      newRound();
    }
    return;
  }

  timeLeft -= dt;
  if (timeLeft <= 0.0) {
    timeLeft = 0.0;
    // A timeout is a miss too; reuse the same feedback-pause path.
    resolveResult = false;
    resolveMsg = "TOO SLOW";
    resolveT = 1.6;
    a.play("sounds/oops.wav");
  }
}

SDL_Rect Game::boardRect() const { return {20, 10, 300, 126}; }
SDL_Rect Game::serveRect() const { return {610, 160, 170, 240}; }
SDL_Rect Game::headRect() const { return {470, 130, 120, 120}; }

void Game::drawBoard(SDL_Renderer* r) {
  SDL_Rect br = boardRect();
  SDL_Texture* b = a.tex("uiart/orderboard.bmp");
  if (b) SDL_RenderCopy(r, b, nullptr, &br);

  // Rebuild the three text rows of the order from the Drink db: flavor+type,
  // then size/shots/milk, then bean/steam notes.
  const char* typeNames[] = {"latte", "mocha", "cap"};
  std::string line1 = target ? cap(target->type) : "";
  if (target && target->flavor != "none") line1 = cap(target->flavor) + " " + line1;
  line1 += "  #" + std::to_string(idx + 1);

  std::string line2 = cap(target ? target->size : "short") + "  " +
                      shotWordStr(target ? target->shots : 1) + "  " +
                      cap(target ? target->milk : "whole");

  std::string line3 = (target && target->bean != "regular" ? cap(target->bean) + "  " : "") +
                      std::string(target && target->type == "latte" ? "steamed" : "");

  int bx = br.x + 14, by = br.y + 14;
  font::draw(r, line1, bx, by, 255, 255, 255, 1, br.w - 20);
  font::draw(r, line2, bx, by + 18, 255, 255, 255, 1, br.w - 20);
  font::draw(r, line3, bx, by + 36, 255, 255, 255, 1, br.w - 20);

  if (resolveT > 0.0) {
    font::draw(r, resolveMsg, br.x, br.y + br.h + 4, 200, 90, 20, 1, br.w);
  } else {
    double frac = timeLeft / timeTotal;
    font::draw(r, std::string("TIME ") + std::to_string((int)timeLeft + 1),
               br.x, br.y + br.h + 4, 240, 240, 240, 1, br.w);
  }
}

void Game::drawHead(SDL_Renderer* r) {
  if (scene != Scene::Play) return;
  SDL_Rect hr = headRect();
  // During a failed-round pause swap in the angry "B" head variant.
  std::string key = (resolveT > 0.0 && !resolveResult && !headB.empty()) ? headB : head;
  SDL_Texture* t = keyedTex(a, key);
  if (t) {
    SDL_Rect src = {0, 0, 65, 65};
    SDL_RenderCopy(r, t, &src, &hr);
  }
  if (resolveT > 0.0) {
    std::string txt = resolveResult ? "HAPPY!" : "ANGRY!";
    font::draw(r, txt, hr.x, hr.y + hr.h + 6, 220, 60, 40, 1, hr.w);
  }
}

void Game::drawControls(SDL_Renderer* r) {
  auto drawKeyRect = [&](const std::string& rel, SDL_Rect rect, bool selected, const SDL_Rect* src = nullptr) {
    SDL_Texture* t = keyedTex(a, rel);
    if (t) {
      SDL_RenderCopy(r, t, src, &rect);
    } else {
      SDL_SetRenderDrawColor(r, 130, 130, 140, 255);
      SDL_RenderFillRect(r, &rect);
    }
    if (selected) {
      // Yellow edge marks the control matching the current build state.
      SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
      SDL_RenderDrawRect(r, &rect);
    }
  };

  // Bean grinders.
  drawKeyRect("uiart/grinderbtn_reg.jpg", {92, 150, 64, 70}, bean == "regular");
  drawKeyRect("uiart/grinderbtn_decaf.jpg", {166, 150, 64, 70},
              bean == "decaf" || bean == "split");
  font::draw(r, "BEAN: " + cap(bean), 92, 228, 240, 240, 255, 1, 220);

  // Milk cartons.
  drawKeyRect("uiart/milkcarton.bmp", {92, 250, 68, 121}, milk == "whole");
  drawKeyRect("uiart/nfmilkcarton.bmp", {172, 250, 68, 121}, milk == "nonfat");
  font::draw(r, "MILK " + cap(milk), 92, 378, 240, 240, 255, 1, 200);

  // Cup size stacks on the right counter.
  int smW = a.texW("uiart/smcups.jpg"), smH = a.texH("uiart/smcups.jpg");
  SDL_Rect smSrc = {0, 0, smW, smH > 0 ? smH / 5 : smH};
  drawKeyRect("uiart/smcups.jpg", {624, 500, 46, 80}, size == "short", smH > 0 ? &smSrc : nullptr);

  int medW = a.texW("uiart/medcups.jpg"), medH = a.texH("uiart/medcups.jpg");
  SDL_Rect medSrc = {0, 0, medW, medH > 0 ? medH / 5 : medH};
  drawKeyRect("uiart/medcups.jpg", {676, 482, 50, 98}, size == "tall", medH > 0 ? &medSrc : nullptr);

  int lgW = a.texW("uiart/lgcups.jpg"), lgH = a.texH("uiart/lgcups.jpg");
  SDL_Rect lgSrc = {0, 0, lgW, lgH > 0 ? lgH / 5 : lgH};
  drawKeyRect("uiart/lgcups.jpg", {735, 466, 54, 114}, size == "grande", lgH > 0 ? &lgSrc : nullptr);

  // Espresso machine spigots and shot cups on the drip tray.
  struct SpigotStation {
    int shotCount;
    int cupX1, cupX2;
    int lightX;
  };
  static const SpigotStation stations[] = {
      {1, 303, 349, 279},
      {2, 417, 462, 396},
      {3, 529, 576, 517},
  };

  for (const auto& s : stations) {
    bool active = (shots == s.shotCount);
    // Spigot indicator light
    std::string lightTex = "uiart/olight" + std::to_string(s.shotCount) + ".bmp";
    int ltW = a.texW(lightTex), ltH = a.texH(lightTex);
    SDL_Rect ltSrc = {0, 0, ltW, ltH > 0 ? ltH / 3 : ltH};
    if (active) {
      drawKeyRect(lightTex, {s.lightX, 148, 22, 22}, false, ltH > 0 ? &ltSrc : nullptr);
    }

    // Two blue shot cups under each spigot
    std::string cupTex = active ? (bean == "decaf" || bean == "split" ? "uiart/shotwcof_decaf.bmp" : "uiart/shotwcof.bmp")
                                : "uiart/shotcup.bmp";
    drawKeyRect(cupTex, {s.cupX1, 251, 38, 48}, active);
    drawKeyRect(cupTex, {s.cupX2, 251, 38, 48}, active);
  }

  // Steamer control on espresso machine.
  int glW = a.texW("uiart/glight.bmp"), glH = a.texH("uiart/glight.bmp");
  SDL_Rect glSrc = {0, 0, glW, glH > 0 ? glH / 3 : glH};
  if (steamed) {
    drawKeyRect("uiart/glight.bmp", {596, 131, 24, 24}, true, glH > 0 ? &glSrc : nullptr);
  }

  // Flavor syrup bottles on top shelf across the carved Jamoke panel.
  for (const auto& b : bottlesList()) {
    SDL_Rect rc = {b.x, b.y, 40, 110};
    drawKeyRect("uiart/bot" + b.name + "A_single.bmp", rc, flavor == b.name);
  }
}

void Game::drawHud(SDL_Renderer* r) {
  font::draw(r, "SCORE " + std::to_string(score), 640, 20, 255, 235, 120, 2, 160);
  font::draw(r, "CUST " + std::to_string(idx + 1) + "/10",
             640, 60, 255, 255, 255, 1, 160);
  if (showFps)
    font::draw(r, "FPS " + std::to_string((int)(fps + 0.5)), 640, 100,
               200, 255, 200, 1, 120);

  // Thermometer countdown tube on the left.
  SDL_Rect tr = {28, 160, 34, 272};
  SDL_SetRenderDrawColor(r, 40, 40, 50, 180);
  SDL_RenderFillRect(r, &tr);

  // Thermometer: a green column anchored to the bottom whose height tracks
  // the fraction of time left, so it visibly drains as the clock runs.
  double frac = timeLeft / timeTotal;
  int fillH = (int)((tr.h - 8) * frac);
  if (fillH > 0) {
    SDL_SetRenderDrawColor(r, 120, 200, 80, 255);
    SDL_Rect fill = {tr.x + 4, tr.y + tr.h - fillH - 4, tr.w - 8, fillH};
    SDL_RenderFillRect(r, &fill);
  }
  font::draw(r, "JAMOKE", 28, 440, 200, 200, 210, 1, 40);
}

void Game::render(SDL_Renderer* r) {
  SDL_SetRenderDrawColor(r, 20, 24, 30, 255);
  SDL_RenderClear(r);

  SDL_Texture* bg = a.tex("uiart/background.jpg");
  if (bg) SDL_RenderCopy(r, bg, nullptr, nullptr);

  switch (scene) {
    case Scene::Title: {
      SDL_Texture* logo = a.tex("uiart/gamebox.jpg");
      SDL_Rect lr = {160, 120, 0, 0};
      int w = a.texW("uiart/gamebox.jpg"), h = a.texH("uiart/gamebox.jpg");
      lr.w = w * 2; lr.h = h * 2;  // board cover art is small; draw it 2x
      if (logo) SDL_RenderCopy(r, logo, nullptr, &lr);
      font::draw(r, "THE COFFEE STAND", 250, 320, 255, 255, 255, 2, 300);
      font::draw(r, "Click the ORDER BOARD or press ENTER to start",
                 150, 420, 220, 220, 220, 2, 500);
      if (bestScore > 0)
        font::draw(r, "BEST " + std::to_string(bestScore), 300, 380,
                   255, 235, 120, 2, 200);
      font::draw(r, "SERVE 10 CUSTOMERS - GET TIPS!", 180, 460, 200, 200, 210, 1, 420);
      font::draw(r, "F2 art  F5 restart  F9 fps  CTRL+ALT+C scores  CTRL+D vsync  ESC menu",
                 40, 500, 150, 150, 160, 1, 720);
      break;
    }
    case Scene::Play:
      drawBoard(r);
      drawHead(r);
      drawControls(r);
      drawHud(r);
      break;
    case Scene::Over: {
      SDL_Rect g = {250, 120, 300, 240};
      SDL_Texture* go = a.tex("uiart/gameover.jpg");
      if (go) SDL_RenderCopy(r, go, nullptr, &g);
      std::string line = "FINAL SCORE " + std::to_string(score);
      font::draw(r, line, 280, 390, 255, 235, 120, 3, 300);
      font::draw(r, "BEST " + std::to_string(bestScore), 340, 430, 200, 200, 210, 1, 140);
      font::draw(r, "Click anywhere or press ENTER to play again",
                 140, 460, 220, 220, 220, 2, 520);
      break;
    }
  }

  SDL_RenderPresent(r);
}