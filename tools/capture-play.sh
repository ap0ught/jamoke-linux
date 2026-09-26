#!/usr/bin/env bash
# Play a full automated shift and save a screenshot at each beat into
# media/shots/. Run from anywhere; paths are relative to the repo root.
#
#   tools/capture-play.sh [window-size]
#
# Frames land in media/shots/, which is gitignored: they are made entirely of
# the original game's art, which stays out of version control per the
# RealNetworks EULA (see README.md). The script that *generates* them is
# tracked, so the shots are reproducible rather than checked in.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

win="${1:-800x600}"
bin="build/jamoke"
if [[ ! -x "$bin" ]]; then
  echo "error: $bin not found -- run: cmake -B build && cmake --build build" >&2
  exit 1
fi

mkdir -p media/shots
rm -f media/shots/show_*.png

echo "capturing a full shift at ${win} -> media/shots/"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  "$bin" --window "$win" --script tests/play/showcase.txt

echo
ls -1 media/shots/
