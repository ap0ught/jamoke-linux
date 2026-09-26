# Data model & parsing (`src/Drinks.h`, `src/Drinks.cpp`)

Parses the two LibV3 text tables that drive the game and exposes them as plain
(structs + vectors) with an id->drink lookup.

## `drinkmenu.txt`

Block format: a `drink:` line opens a record; following `key=value` lines fill
it until the next marker. `//` comments are stripped, values are trimmed and
unquoted.

Canonicalization happens in `setKey`:
- `type`, `size`, `milk`, `difficulty`, `flavor` are matched case-insensitively
  against allowed enum lists; unknown values fall back to the defaults (or
  empty for flavor/type).
- `shots` is one token packing two dimensions ("double regular", "triple|decaf"):
  the code substring-probes it. Triple beats double; decaf beats split/regular.
  Invalid beans/shots fall back to 1 shot / regular.

## `CustomerList.txt`

Same block format (`customer:` + lines). Fields: `sprite`, `gender`, `drink`,
`shift`. Gender is lowercased so `Game` can match it directly.

**Sprite tokens**: the data writes `sprite=ID_BMP_CUSTOMER_n` (an exe-level
macro), and real indices were previously lost to `atoi` (returning 0 for
everyone). `spriteNum` now extracts the number from either a macro token or a
plain integer. Customers are listed in sprite order, so `customers[i].sprite == i`
on the real data — the face index for `Game`'s by-index head lookup
(see `docs/game.md`).

## Pointer-safe byId

`byId` maps id -> `const Drink*` and is **rebuilt only after `load()` finishes
parsing**, because `push_back` can reallocate `drinks` and invalidate pointers
captured earlier. Never populate `byId` during the parse loop.

## Invariants relied on elsewhere

- `db.find(id)` returns nullptr for unknown ids; `Game` guards the null target
  and falls back to `drinks[0]`.
- `load()` returns `!drinks.empty()` (a data dir with no drinks is a failed
  load; customers alone are not enough).
- `Game::pickRoster` prefers customers whose order resolves in the DB, so a
  customer whose drink id is missing never enters the roster (unless nothing
  resolves, in which case the raw list is used and the null target is
  re-guarded at round start).