# Gameplay & rendering (`src/Game.h`, `src/Game.cpp`)

Owns the whole game state machine, input hit-testing, and rendering. It pulls
everything else together: `Assets` for media, `DrinkDB` for orders/roster, and
`font` for text.

## Scenes

`Scene = { Title, Play, Over }`. The game is constructed already in `Play`
(the first order is live immediately); Title/Over are entered by switching
`scene`. `reset()` re-inits run state (idx/score/served/feedback) and starts a
fresh round.

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

## Build controls (`handleClick`, fixed 800x600 rects)

Right column: serve `{600,140,177,252}`. Left column (bean grinders, milk,
shots, steam), flavor bottles row on the right, and the `NO SYRUP` reset all
live in hardcoded rectangles (see the code near `handleClick`; `boardRect` /
`serveRect` / `headRect` are `{20,10,300,126}`, `{600,140,177,252}`,
`{470,130,120,120}`). Selection toggles: beans cycle decaf → split → regular;
syrup tap-to-clear. Sounds (`grinder`, `pourmilk`, `steamer`, `cashreg`,
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
  (Ctrl-Alt-C) zeroes it.
- `showFps` + `fps`: the F9 counter. `toggleFps()` flips the flag; `main.cpp`
  pushes a smoothed 0.9/0.1 running average into `fps` each frame.
- `toTitle()`: Esc mid-game shim — resets the run and lands back on the Title
  scene (the original opened an options overlay the remake doesn't have).

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