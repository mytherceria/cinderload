#!/bin/sh
# CinderLoad 0.4.0's satellite assets, the whole build in order (13 Oct 2026): the four overviews, the detail archive at
# 512 and 256 px, the four test packs, the checks and the stills. Heavy steps run one at a time in their own memory
# scope (MemoryMax 8G). Nothing is written into a game client.
#   STORMLIB=/path/to/libstorm.so tools/build_sat040.sh <out dir>   (out gets packs/ and stills/; scratch: $WORK)
set -e
OUT=${1:?usage: build_sat040.sh <out dir>}
HERE=$(cd "$(dirname "$0")/.." && pwd)
INBOX=${SAT_INBOX:-inputs}      # the build's inputs: the master, 0.3.0's assets, the w3d export
MASTER=${MASTER:-$INBOX/azeroth-topdown-2026-10-13}
PACKS030=${PACKS030:-$INBOX/cinderload-release-0.3.0-tonight/assets}
WORK=${WORK:-build/sat040}
export STORMLIB=${STORMLIB:?set STORMLIB to the path of libstorm.so}
export WOW_DATA=${WOW_DATA:?set WOW_DATA to the game client Data folder, read only}
run() { systemd-run --user --scope -q -p MemoryMax=8G -p MemorySwapMax=1G -- "$@"; }

rm -rf "$WORK" && mkdir -p "$WORK" "$OUT/packs" "$OUT/stills"
run python3 "$HERE/tools/sat_overview.py" "$MASTER" "$PACKS030" "$WORK/ov"
run python3 "$HERE/tools/make_sat.py" "$MASTER" "$PACKS030" "$WORK/sat" --best --sea-png "$WORK/ov/sea-21x9.png"
python3 "$HERE/tests/test_sat.py" "$PACKS030" "$WORK/sat" "$WORK/ov"
for s in 16x10 16x9 21x9 32x9; do
  rm -f "$OUT/packs/LoadingScreens-$s.MPQ"
  python3 "$HERE/tools/make_pack040.py" "$PACKS030/LoadingScreens-$s.MPQ" "$WORK/ov/CinderMap-$s.blp" \
    "$OUT/packs/LoadingScreens-$s.MPQ"
done
for px in 512 256; do
  mkdir -p "$OUT/packs/CinderSat-$px"
  cp "$WORK/sat/$px/CinderSat.MPQ" "$WORK/sat/$px/sat.txt" "$WORK/sat/$px/BUILD.txt" "$OUT/packs/CinderSat-$px/"
done
mkdir -p "$OUT/packs/overview" && cp "$WORK"/ov/CinderMap-*.blp "$OUT/packs/overview/"
(cd "$OUT/packs" && find . -type f \( -name '*.MPQ' -o -name '*.blp' -o -name 'sat.txt' \) | sort | xargs sha256sum \
  > SHA256SUMS)
rm -f "$OUT"/stills/*.jpg "$OUT"/stills/stills-*.txt
run python3 "$HERE/tools/sat_stills.py" "$PACKS030" "$WORK/ov" "$WORK/sat" "$OUT/stills" 21x9 "$MASTER"
echo "built: $OUT/packs, $OUT/stills"
