# Asset / media layer (`src/Assets.h`, `src/Assets.cpp`)

Front for SDL_Image and SDL audio over the original Jamoke `DATA` tree. All
lookups anywhere in the codebase are by **lowercase relative path**, e.g.
`"uiart/background.jpg"`, `"customers/M_DudeHead.bmp"`.

## Index (`loadRoot`)

`indexDir` walks the tree once with `std::filesystem::recursive_directory_iterator`
and stores `lowercase(relative path) -> absolute path` in `files`. This makes
every later lookup case-insensitive no matter how the media directory is cased
on disk (the DATA tree mixes `.BMP` and `.bmp`, and the source often disagrees).
`loadRoot` returns false (and the app refuses to start) if the directory is
missing or empty.

## Textures (lazy, cached)

`tex()` decodes + uploads on first use (`IMG_Load` ->
`SDL_CreateTextureFromSurface`) and caches the `SDL_Texture*` in `textures`.
Missing art yields `nullptr`, and every call site renders a fallback instead of
crashing. `texW`/`texH` query the cached texture's size (used for centering and
for scaling small art like the title logo).

## Keyed transparency (`keyedTex`, in `src/Game.cpp`)

Face/cup BMPs bake an alpha key into the top-left pixel. `keyedTex` reads that
pixel back and calls `SDL_SetColorKey` — but only if the color "looks like a
key" (near-black, magenta, or green), a plausibility guard that stops genuinely
dark art from being keyed out. It is a free function in Game.cpp rather than an
Assets method because it needs the renderer and pixel data, but it is the
texture-loading path for all sprites with baked-in transparency.

## Audio (`play`)

One shared output device, opened lazily on the first sound. Decoded wavs are
cached (spec + raw PCM). Every play routes the file through an
`SDL_AudioStream` into `SDL_QueueAudio` and bounded 64 KiB chunks — the only
path guaranteed to produce PCM in the device's *actual* format, since SDL may
adjust frequency/channels when opening the device. Missing files are a silent
no-op.