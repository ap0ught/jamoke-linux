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
  // Boot on the title menu like the original, so the first order's timer
  // never runs before the player has actually started. reset() leaves the
  // first round staged; startRun() is what commits to it.
  scene = Scene::Title;
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
  paused = false;
  quitPending = false;
  pickRoster();
  newRound();
}

// Enter/Space from the title menu or the game-over card: a fresh run.
void Game::startRun() {
  reset();
  scene = Scene::Play;
}

// Esc mid-game opens the original's menu/options overlay, which leaves the
// run untouched (unlike a straight trip to the title menu, which would throw
// away the score on a stray keypress).
void Game::togglePause() {
  if (scene != Scene::Play) return;
  paused = !paused;
  quitPending = false;
}

// Abandoning a run is destructive, so the menu overlay arms a confirmation
// and only acts on a second press.
void Game::requestQuitToTitle() {
  if (!paused) return;
  if (!quitPending) {
    quitPending = true;
    return;
  }
  toTitle();
}

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

const Game::Controls& Game::controls() {
  // Single source of truth for the machine layout. drawControls() draws these
  // rects and handleClick() hit-tests the same ones, so a control cannot be
  // drawn in one place and clicked in another.
  //
  // The serve rect is the one measured from the original art rather than
  // guessed: the white ORDER arrow is baked into UIArt/Background.jpg, and its
  // near-white pixels span x 1..195, y 256..350. The remake draws that same
  // background, so the control belongs on the left sidebar -- not floating in
  // an empty region on the right where nothing was ever drawn.
  static const Controls c = [] {
    Controls t{
      /*serve=*/{0, 254, 198, 98},
      /*beanRegular=*/{92, 150, 64, 70},
      /*beanDecaf=*/{166, 150, 64, 70},
      /*milkWhole=*/{100, 400, 68, 121},
      /*milkNonfat=*/{180, 400, 68, 121},
      /*cupShort=*/{624, 500, 46, 80},
      /*cupTall=*/{676, 482, 50, 98},
      /*cupGrande=*/{735, 466, 54, 114},
      /*shot1=*/{285, 230, 110, 80},
      /*shot2=*/{400, 230, 110, 80},
      /*shot3=*/{510, 230, 110, 80},
      /*steamer=*/{592, 133, 32, 32},
      /*syrups=*/{},
    };
    // Syrup hit boxes are the bottle positions from bottlesList(), so they can
    // never drift from where the bottles are actually drawn.
    for (const auto& b : bottlesList()) t.syrups.emplace_back(b.name, SDL_Rect{b.x, b.y, 40, 110});
    return t;
  }();
  return c;
}

namespace {
// The syrup rects are the bottle positions widened to the hit box, filled in
// lazily because they derive from bottlesList().
std::vector<std::pair<std::string, SDL_Rect>> syrupTable() {
  std::vector<std::pair<std::string, SDL_Rect>> v;
  for (const auto& b : bottlesList()) v.emplace_back(b.name, SDL_Rect{b.x, b.y, 40, 110});
  return v;
}
}  // namespace

const std::vector<std::pair<std::string, SDL_Rect>>& Game::syrupRects() {
  static const std::vector<std::pair<std::string, SDL_Rect>> v = syrupTable();
  return v;
}

void Game::handleClick(int x, int y) {
  if (paused) return;  // menu overlay swallows clicks
  if (scene == Scene::Title) { startRun(); return; }
  if (scene == Scene::Over) { reset(); return; }
  if (scene != Scene::Play || resolveT > 0.0) return;  // no input mid-feedback

  // x/y arrive already in the 800x600 logical space (main.cpp scales the
  // mouse), so every hit test below is a hardcoded UI rectangle.
  auto box = [&](SDL_Rect r) { return inRect(r, x, y); };

  const Controls& c = controls();

  if (box(c.serve)) { serve(); return; }
  if (box(c.beanRegular)) { selectBean("regular"); a.play("sounds/grinder.wav"); return; }
  if (box(c.beanDecaf)) { selectBean("decaf"); a.play("sounds/grinder.wav"); return; }
  if (box(c.milkWhole)) { selectMilk("whole"); a.play("sounds/pourmilk.wav"); return; }
  if (box(c.milkNonfat)) { selectMilk("nonfat"); a.play("sounds/pourmilk.wav"); return; }
  if (box(c.cupShort)) { selectSize("short"); a.play("sounds/cups.wav"); return; }
  if (box(c.cupTall)) { selectSize("tall"); a.play("sounds/cups.wav"); return; }
  if (box(c.cupGrande)) { selectSize("grande"); a.play("sounds/cups.wav"); return; }
  if (box(c.shot1)) { shots = 1; a.play("sounds/grinder.wav"); return; }
  if (box(c.shot2)) { shots = 2; a.play("sounds/grinder.wav"); return; }
  if (box(c.shot3)) { shots = 3; a.play("sounds/grinder.wav"); return; }
  if (box(c.steamer)) { steamed = !steamed; a.play("sounds/steamer.wav"); return; }
  for (const auto& kv : c.syrups) {
    if (box(kv.second)) { selectFlavor(kv.first); return; }
  }
}

void Game::update(double dt) {
  if (scene != Scene::Play) return;
  if (paused) return;  // menu overlay: freeze the timer and the round

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

  const Controls& c = controls();

  // The ORDER button needs no extra blit: the white arrow is already baked
  // into UIArt/Background.jpg, which this scene draws. OrderButton.bmp is the
  // pressed-state sprite and is deliberately not drawn -- it is a 3-frame
  // strip (177x252) that renders badly here, because its top-left pixel is
  // near-white, so the corner-sniffing transparency key in Assets keys out the
  // arrow itself and leaves the magenta field. See issue #9.

  // Bean grinders.
  drawKeyRect("uiart/grinderbtn_reg.jpg", c.beanRegular, bean == "regular");
  drawKeyRect("uiart/grinderbtn_decaf.jpg", c.beanDecaf,
              bean == "decaf" || bean == "split");
  font::draw(r, "BEAN: " + cap(bean), 92, 228, 240, 240, 255, 1, 220);

  // Milk cartons.
  drawKeyRect("uiart/milkcarton.bmp", c.milkWhole, milk == "whole");
  drawKeyRect("uiart/nfmilkcarton.bmp", c.milkNonfat, milk == "nonfat");
  font::draw(r, "MILK " + cap(milk), 100, 528, 240, 240, 255, 1, 200);

  // Cup size stacks on the right counter.
  int smW = a.texW("uiart/smcups.jpg"), smH = a.texH("uiart/smcups.jpg");
  SDL_Rect smSrc = {0, 0, smW, smH > 0 ? smH / 5 : smH};
  drawKeyRect("uiart/smcups.jpg", c.cupShort, size == "short", smH > 0 ? &smSrc : nullptr);

  int medW = a.texW("uiart/medcups.jpg"), medH = a.texH("uiart/medcups.jpg");
  SDL_Rect medSrc = {0, 0, medW, medH > 0 ? medH / 5 : medH};
  drawKeyRect("uiart/medcups.jpg", c.cupTall, size == "tall", medH > 0 ? &medSrc : nullptr);

  int lgW = a.texW("uiart/lgcups.jpg"), lgH = a.texH("uiart/lgcups.jpg");
  SDL_Rect lgSrc = {0, 0, lgW, lgH > 0 ? lgH / 5 : lgH};
  drawKeyRect("uiart/lgcups.jpg", c.cupGrande, size == "grande", lgH > 0 ? &lgSrc : nullptr);

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
    drawKeyRect("uiart/glight.bmp", c.steamer, true, glH > 0 ? &glSrc : nullptr);
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
      font::draw(r, "Click the ORDER BOARD or press ENTER / SPACE to start",
                 150, 420, 220, 220, 220, 2, 500);
      if (bestScore > 0)
        font::draw(r, "BEST " + std::to_string(bestScore), 300, 380,
                   255, 235, 120, 2, 200);
      font::draw(r, "SERVE 10 CUSTOMERS - GET TIPS!", 180, 460, 200, 200, 210, 1, 420);
      font::draw(r, "F2 art  F5 restart  F9 fps  CTRL+ALT+C scores  CTRL+D vsync  ESC menu",
                 40, 500, 150, 150, 160, 1, 720);      break;
    }
    case Scene::Play:
      drawBoard(r);
      drawHead(r);
      drawControls(r);
      drawHud(r);
      break;
    case Scene::Over: {
      // GAMEOVER.jpg is a *two-frame vertical strip* (350x278 = 2 x 350x139,
      // both frames identical). Drawing the whole texture stacks two squashed
      // "Game Over" panels on top of each other, so take the top frame and
      // preserve its aspect ratio.
      SDL_Texture* go = a.tex("uiart/gameover.jpg");
      int gw = a.texW("uiart/gameover.jpg"), gh = a.texH("uiart/gameover.jpg");
      if (go && gw > 0 && gh > 0) {
        SDL_Rect src = {0, 0, gw, gh / 2};
        SDL_Rect dst = {250, 170, 300, (300 * (gh / 2)) / gw};
        SDL_RenderCopy(r, go, &src, &dst);
      }
      std::string line = "FINAL SCORE " + std::to_string(score);
      font::draw(r, line, 280, 400, 255, 235, 120, 3, 300);
      font::draw(r, "BEST " + std::to_string(bestScore), 340, 440, 200, 200, 210, 1, 140);
      font::draw(r, "Click anywhere or press ENTER / SPACE to play again",
                 140, 475, 220, 220, 220, 2, 520);
      break;
    }
  }

  // Menu overlay (Esc): dims the live board without touching run state, so a
  // stray Esc can never cost the player a run.
  if (paused) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 170);
    SDL_Rect dim = {0, 0, 800, 600};
    SDL_RenderFillRect(r, &dim);
    font::draw(r, "MENU", 340, 190, 255, 235, 120, 3, 120);
    font::draw(r, "ESC or ENTER / SPACE  resume", 250, 270, 230, 230, 230, 2, 300);
    font::draw(r, "F5  restart this run", 250, 310, 230, 230, 230, 2, 300);
    font::draw(r, "Q  quit to title", 250, 350, 230, 230, 230, 2, 300);
    if (quitPending)
      font::draw(r, "PRESS Q AGAIN - THIS ENDS THE RUN (SCORE LOST)",
                 120, 410, 255, 120, 120, 2, 560);
    font::draw(r, "CUST " + std::to_string(idx + 1) + "/10   SCORE " +
                      std::to_string(score),
               250, 460, 200, 200, 210, 1, 300);
  }

  SDL_RenderPresent(r);
}