# Font (`src/Font.h`, `src/Font.cpp`)

Tiny built-in **5x7 bitmap font** — the game has no SDL2_ttf dependency, so
all on-screen text (`SCORE`, order board, labels, overlays) is rasterized here.

## Glyph table

`GLYPHS[128][7]` holds ASCII 0..127; each glyph is 7 row bytes, bit 4 of a row
byte being the **leftmost** of its 5 columns (bits 4..0 map to columns 0..4).
`blitGlyph` masks the index with `c & 0x7F` so a byte >= 0x80 (e.g. from locale
strings) can never index out of bounds.

## Metrics

- Height: `7 * scale` (fixed).
- Width: `6 * scale` per character — 5 columns plus a 1px advance gap, matching
  the draw loop. `font::width` is therefore purely `len * 6 * scale`.

## Drawing

Two-pass render for readability over busy photo backgrounds:
1. An **outline pass** stamps every glyph once per 8-neighbor offset in black.
2. A **fill pass** draws the glyphs in the requested color on top.

`maxWidth` truncates a running line once `x` advance passes it (used to keep
text inside the order board and HUD columns). A space just advances — nothing
is blitted.