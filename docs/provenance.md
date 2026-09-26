# Original release provenance

What the original Jamoke game actually was, drawn from the archived RealArcade /
GameHouse distribution of `esc0rtd3w/realarcade-wrapper-killer` (commit
`c2cb9c72`, sparse-checked `gameconsole` dump). Not needed to run the remake
(it loads `DATA/` directly), but it settles the "which build is the local exe,
and why RAWK cannot unlock it" questions.

## Official identity (verified)

- **RealArcade v2.1 catalog**: `realarcadev21/games/tmp.txt` line 1479 —
  `jamoke` sits between `jammedagain` (1478) and `janeshotel` (1480). Jamoke
  was a stock RealArcade v2.1 title.
- **GameHouse product ID**: `migrationInfo.packages.xml` contains
  `<gameID>jamoke</gameID><productID>7412</productID>`.
- **Gamepage / product GUID**: `FFD24A74-22DD-4bc5-8C17-11AF2A4A0931`. It is
  the server `online/gamepage/...` directory name in the mirror log and the
  ASCII GUID embedded in both `.mez` headers.
- **Master game DB** (`src/rna/rnarestore/db/GAMEFIND.DBF`, record 4904):
  `MULTIID=Jamoke`, `GAMENAME="1.0 Full"`, `EXTRA2=full`, `GAMETYPE` holds a
  second GUID `FFFFFFFF-1DD2-11B2-85A6-00D0B...` (truncated by the 50-char
  field).
- **Catalog memo** (`GAMEFIND.FPT`, same DB): tagline *"Keep the coffee
  flowing and your customers happy. Like the drinks you'll pour, Jamoke is
  both tasty and addictive!"* and the wrapper/DRM config fields, including
  `FileName=JamokeLib.exe` (the exact local binary filename),
  `MinMonitorResolution=640x480`, `MinDirectXVersion=07`, `MinProcessorType=P-90`,
  `OperatingSystem=Windows95, Windows98, Windows2000`, `APILevelSupport=None`.

  Caveat: the FPT is a pooled memo store; after the FileName block the raw
  stream visibly runs into another game's memo (a `WindowTitle=Treasure Island`
  segment). Treat the Jamoke memo as ending before that binary padding.

## Server distribution footprint

From `push.to.10.34.240.71.log` (2013-11-26/27 rsync-style mirror), Jamoke's
live layout:

- `games/demo/mezzanines/jamoke_free.mez`, `games/demo/mezzanines/jamokedemo.mez`
- `games/demo/rgp/JamokeDemo.rgp`
- `games/demorgses/{europergps,rgp,rgp/JP,rgp/JP/rgp_only_backup_042608,Old Rgps}/jamoke_free.rgp`
- `games/full/mezzanines/jamoke_full.mez`, `games/full/mezzanines/jamokefull.mez`
- `realarcadev21/en/games/jamoke/jamoke.xml`
- `realarcadev21/games/jamoke/jamoke.rga`, `realarcadev21/games/jamoke/DRMConfigjamoke.rdc`,
  `realarcadev21/games/jamoke/jamoke_images.rar`, `realarcadev21/games/jamoke/tps/lycos2_.rga`
- `online/gamepage/FFD24A74-22DD-4bc5-8C17-11AF2A4A0931`

## Preserved vs lost

Preserved in-repo (the `gameconsole/games/...` dump), `.mez` = XZip2.0 format:

| File | Size | MD5 |
|------|------|-----|
| `games/demo/mezzanines/jamoke_free.mez` | 14,502 B | `1c2ca2d3a8d07b1b26c2cacca784775f` |
| `games/demo/mezzanines/jamokedemo.mez` | 14,502 B | `1c2ca2d3a8d07b1b26c2cacca784775f` |
| `games/full/mezzanines/jamoke_full.mez` | 13,834 B | `32e955c555c5c5f828059da2f538c2cc` |
| `games/full/mezzanines/jamokefull.mez` | 13,834 B | `32e955c555c5c5f828059da2f538c2cc` |

Demo and full pairs are byte-identical. The `.mez` files are storefront
game-info, not game content — and they decode fully: the XZip2.0 body was
decoded with the community unpacker (see below) and every CRC verifies.

### Mezzanine contents (decoded)

XZip2.0 format: `XZip2.0\0` signature directly (no `RASGI2.0` wrapper on
`.mez`), ID = the product GUID `FFD24A74-22DD-4bc5-8C17-11AF2A4A0931`,
compression level 8, solid LZ stream (splay-tree coded file table, adaptive
range-coded blocks). Decoded with `ravenDS/raven-tools`
(`pc/real-arcade/xzip2.py`, disassembly by christphen). Verdict of the
earlier reverse-engineering attempts: the data is *not* zlib/deflate
(full-file brute-force scan negative, entropy ≈ 7.98 bit/byte) — it is
NetZip-proprietary coding, exactly as the `ExecuteXZip` engine string implies.

Demo `jamoke_free.mez` → 5 files, 27,939 B:

| Recovered | Size | MD5 | package date |
|-----------|------|-----|--------------|
| `cover.png` | 10,426 B | `697a096b1fab6c836fbff4b35ef6dc3f` | 2001-02-12 |
| `demo_overview_body.rtf` | 6,711 B | `46fa19faecf28d0a5b74bc21789da599` | 2001-05-30 |
| `issues.rtf` | 2,743 B | `32d65f3cfc0dde55b3fbd06b14f6f9e0` | 2001-03-06 |
| `manual.rtf` | 4,783 B | `62afae4f21f88344b6016cf3018cdbe4` | 2001-02-20 |
| `mezzanine.xml` | 3,276 B | `b58e51c52a07753df1cac507940ef648` | 2001-09-13 |

Full `jamoke_full.mez` → 5 files, 24,806 B: identical `cover.png`,
`issues.rtf`, `manual.rtf`; `full_overview_body.rtf` (3,665 B,
`73b1170b2172eb6c7c3219992b045239`, 2001-02-20) and `mezzanine.xml`
(3,189 B, `1441bdd04312fe24375c1b599bc935ef`, 2001-09-13) replace their
demo variants. Recovered copies live in `media/provenance/mez-{demo,full}/`
(gitignored — RealNetworks media stays local per EULA).

Storefront metadata recovered (`mezzanine.xml`):

- Version **1.0.0.2** (demo row reads `1.0.0.2 demo`), YearReleased **2000**,
  ESRB **E**, USA, English, `DeveloperID=28`, GameCaption "Jamoke".
- Developer's website: `http://www.wizbang.com/`. Related titles:
  `81C92481-29C8-497f-B74B-C252DCAE527C`, `EABCA8FB-3B59-48a2-AB8B-AF1873CB2C12`.
- Demo-only rows: `FullVersionDownloadSize=3100` (KB) and the RealArcade
  "UNLOCK THE FULL VERSION… INSTANTLY" e-commerce pitch (30-day guarantee,
  "My Account" backup).
- Client-only links (not in the archive): streaming preview SMIL at
  `ramhurl.real.com/marketing/games/arcadetemplate/strategy/jamoke/start.smi`,
  reviews/boards `inpl://` URLs, `game://Help/Jamoke.chm` and
  `game://readme.txt`.
- Overview copy (full): *"Ever wondered what it's like to be a barista in a
  hip coffee bar?… Three modes of play are available: Marathon, Speed Freak
  and Beat the Clock. Like the drinks you'll pour, Jamoke is both tasty and
  addictive!"* — the three mode names match `JamokeLib.exe`'s loading screen.
  Screenshot/credit strings: "Mike Engle", "Edgar Martinez" (demo).
- Manual (real gameplay notes): hot keys `Esc` (menu/options toggle), `F2`
  (full art reload), `F5` (restart game), `F9` (frame-rate display), `Enter`/
  `Space` (dismiss dialogs), `Ctrl-Alt-C` (clear high scores), `Ctrl-D`
  (continuous vs double-buffer update; double-buffer default).

Lost (listed on the repo's own missing/obsolete audits, no content captured):

- `jamoke_full.mez` / `jamokefull.mez` / `jamoke_free.mez` / `JamokeDemo.rgp`
  in `missing_files_list.txt` (lines 2295-2296, 3230, 4138-4139) — the state
  at audit time, while the push log shows the same paths existed on the mirror.
- `jamoke_free.rgs` in `extras/rgs/rgs-archived-installers-list-missing.txt`.
- `extras/rgs/_retired games unavailable/jamoke.txt` — 0-byte placeholder.
- No captured `jamoke.rga`, `jamoke.xml`, `DRMConfigjamoke.rdc`,
  `jamoke_images.rar`, or `.rgp` content anywhere in the tree.

## Why RAWK cannot unlock the local exe

The local `JamokeLib.exe` (774,144 B, WizBang LibV3 engine, no `Wrapper::`,
`prepareForLaunch`, `RacNotRunning`, or `gameExtract` markers) is the game
binary RAWK *produces*, not the wrapped artifact it patches. RealGames demo
flavor (`?src=demo` URLs, dynamic `RngInterstitial.dll` upsell), no time-trial
timer to remove. And the wrapper binaries such an unlock would consume
(`.rgs`/`.rga`) are the lost set above. The surviving `.mez` files are
preserved *and* now fully decoded — and they contain only storefront display
content (cover/manual/overview/`mezzanine.xml`), **no wrapper payload**. The
`.rgs`/`.rgp` installers were served from the mirror's other directories
(`gameconsole/rgs/…`, `games/demo/rgp/…`) and were not captured in this dump,
so the wrapper remains genuinely unrecoverable here.

Net result: Jamoke was genuinely distributed as a GameHouse/RealArcade v2.1
full title (catalog + productID 7412 + product GUID + "1.0 Full" DB record +
matching `FileName=JamokeLib.exe`), the demo/full `.mez` mezzanines are
preserved, decoded and CRC-verified (covering the whole storefront record for
v1.0.0.2), but the wrapper payloads themselves are gone — so the archive
provenance is solid and the RAWK route is a dead end. The remake sidesteps
both by loading `DATA/` directly.

## Raw source

- `esc0rtd3w/realarcade-wrapper-killer` @ `c2cb9c72` — sparse clone with
  `gameconsole/` + `extras/rgs/` checked out; `src/` reachable via
  `git show HEAD:src/...`.
- Relevant paths: `gameconsole/realarcadev21/games/tmp.txt`,
  `gameconsole/games/{demo,full}/mezzanines/jamoke*.mez`,
  `gameconsole/push.to.10.34.240.71.log` (+ `1.bak`, `missing_files_list.txt`),
  `extras/misc/migrationInfo.packages.xml`,
  `src/rna/rnarestore/db/GAMEFIND.{DBF,FPT}`,
  `extras/rgs/rgs-archived-installers-list-missing.txt`.

Scratch copies used during this investigation (extracted DBF/FPT + migration
XML + `.mez`) live in `/tmp/opencode/mezwork/` and are throwaway; the
*recovered* mezzanine files were copied out to `media/provenance/`.
Decoder references: `ravenDS/raven-tools` `pc/real-arcade/xzip2.py`
(disassembly by christphen), `SabreTools/SabreTools.Serialization`
(`SabreTools.Data.Models/RealArcade/Constants.cs` — RASGI2.0/XZip2.0 model).