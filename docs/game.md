# Gameplay & rendering (`src/Game.h`, `src/Game.cpp`)

Owns the whole game state machine, input hit-testing, and rendering. It pulls
everything else together: `Assets` for media, `DrinkDB` for orders/roster, and
`font` for text.

## Scenes

`Scene = { Title, Play, Over }`. The game **boots on `Title`** (like the
original) so no order timer runs before the player chooses to start; the
constructor calls `reset()` to stage the first round and then holds `Title`.
`startRun()` commits to it and enters `Play` — bound to Enter/Space on
Title/Over *and* to a left click on Title (the title screen advertises
"click the ORDER BOARD"). `reset()` re-inits run state
(idx/score/served/feedback) and starts a fresh round.

## Roster & rounds

- `pickRoster()` builds a **10-slot roster** from the customer pool (preferring
  customers whose drink resolves in the DB), cycling the pool to fill 10.
  `idx` points at the current slot; `newRound()` guards `idx >= 10` → `Over`.
- Per-customer time limit: 55 s easy, 42 s hard, else 50 s.
- The round target is `db.find(c->drinkId)` with a `drinks[0]` fallback.

## Faces by customer index

`newRound()` picks the customer's head art by their own position among
same-gender customers (`genderSeq`, data/sprite order), from a male pool (11
names) or female pool (6 names). Same customer ⇒ same face; consecutive
customers advance through the pools. It does **not** use a global round/serve
counter, so faces never desync or repeat for different people. The angry
feedback variant is `"<name>B"` — looked up in the index first; missing
variants (e.g. `M_DudeHeadB`) are cleared and the base head is shown.

## Build controls (`Game::controls`, one table for drawing and hit-testing)

Every clickable rectangle lives in a single static table, `Game::controls()`,
and **both** `drawControls()` and `handleClick()` read it. They used to be two
independent sets of numbers that merely sat near each other — the drawn cup was
50x98 while the clickable box was 54x130, the syrup shelf 40x110 drawn against
44x115 clicked, and the serve control was clickable in an empty region on the
right. Nothing tied them together, and the unit tests cannot see art, which is
how "clicking the tall cup does nothing" becomes possible at all.

`GameLayout.*` in `tests/TestGame.cpp` locks the invariants: every rect is inside
the 800x600 logical space, **no two controls overlap** (an overlap silently
steals clicks, because `handleClick` tests in order and the earlier control
wins), and `syrupRects()` matches `bottlesList()`.

- **serve** `{0,254,198,98}` — the ORDER control, on the left sidebar. This is
  the one rect *measured* rather than invented: the white arrow is baked into
  `UIArt/Background.jpg` and its near-white pixels span x 1..195, y 256..350. It
  needs no extra art; the background already draws it.
- **beanRegular/beanDecaf** `{92,150,64,70}` / `{166,150,64,70}`
- **milkWhole/milkNonfat** `{100,400,68,121}` / `{180,400,68,121}` — moved clear
  of the ORDER button when the serve rect moved onto the sidebar. Their true
  original position is still unverified (issue #1).
- **cupShort/cupTall/cupGrande** `{624,500,46,80}` / `{676,482,50,98}` /
  `{735,466,54,114}`
- **shot1/shot2/shot3** `{285,230,110,80}` / `{400,230,110,80}` /
  `{510,230,110,80}`
- **steamer** `{592,133,32,32}` — shrunk onto its own indicator light.
- **syrups** one per `bottlesList()` entry, `{x, y, 40, 110}`.

`boardRect()` `{20,10,300,126}` and `headRect()` `{470,130,120,120}` are
decorative and not clickable. Selection toggles: beans cycle decaf → split →
regular; syrup tap-to-clear. Sounds (`grinder`, `pourmilk`, `steamer`, `cashreg`,
`oops`) fire per interaction.

## Serve validation

Attributes are compared one by one: size, shots, bean, milk, flavor, and
steam — where `steamed` is only required when `target->type == "latte"`.
Correct: +100, +25 tip if `timeLeft > 10`, plays `cashreg.wav`. Wrong: `WRONG
- NO TIP`, plays `oops.wav`. Either way a 1.6 s feedback pause (`resolveT`)
freezes input, shows HAPPY! / ANGRY! over the (possibly swapped) head, then
advances `served`, `idx` and starts `newRound()`. A timeout is a miss that
reuses the same pause path (`TOO SLOW`). Input is ignored entirely while
`resolveT > 0`.

## Update / voice

One order voice-over per customer — `voices/<m|f>_order{1..4}.wav` chosen by
round index — plays on the first update of the round. The timer drains each
frame; hitting 0 routes into the feedback path above.

## Session record & debug readouts

- `bestScore` holds the session high score; the game-over transition in
  `newRound()` records `max(bestScore, score)` so a fresh run never erases it.
  Shown on the Title screen and next to the final score. `clearScores()`
  (Ctrl-Alt-C) zeroes it. The record survives abandoning a run.
- `showFps` + `fps`: the F9 counter. `toggleFps()` flips the flag; `main.cpp`
  pushes a smoothed 0.9/0.1 running average into `fps` each frame.
- `paused` + `quitPending`: the Esc menu overlay. `togglePause()` opens and
  closes it while keeping the run intact; `update()` returns early and
  `handleClick()` swallows input while it is up, so the customer timer cannot
  drain behind the menu. `requestQuitToTitle()` is the only destructive path
  and it needs two presses — the first arms `quitPending` and the overlay
  warns, the second calls `toTitle()` (full `reset()` + `Scene::Title`).
- `startRun()`: Title/Over into a fresh `Scene::Play` round. `toTitle()` is
  the abandon-a-run operation and is only reachable from the confirmed quit.

## Rendering

`render()` clears, draws the shared `uiart/background.jpg`, then switches on
scene:
- Title: `gamebox.jpg` logo at 2x (from `texW/texH`), start instructions, best
  score, and a hot-key hint line.
- Play: order board (rebuilt text rows from the Drink: flavor+type, size/shots/
  milk, bean/steam notes, all clamped by `maxWidth`), head, controls, HUD.
- Over: `gameover.jpg`, final score, best score, restart prompt.

The HUD shows score and `CUST idx+1/10`, and a thermometer whose green column
drains bottom-up with `timeLeft/timeTotal` over the `thermometer.bmp` art.