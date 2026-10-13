#!/bin/bash
# Builds CinderLoad.dll for the 32-bit 1.12.1 client with mingw-w64 and no C runtime, the same way as
# IndoorRain/dll/build.sh: ./build.sh [out.dll]. The fire sound reads the client's archives with IndoorRain's MPQ reader
# (mpq.c, inflate.c), carried in ../shared from Azeroth Atmosphere's IndoorRain/dll.
set -eu
cd "$(dirname "$0")"
out="${1:-CinderLoad.dll}"
i686-w64-mingw32-gcc -O2 -Wall -Wextra -Wframe-larger-than=4096 -std=c99 -msse2 -mfpmath=sse -shared -nostdlib -nostartfiles -fno-ident -fno-asynchronous-unwind-tables \
  -fno-stack-protector -fno-builtin -ffreestanding -fno-tree-loop-distribute-patterns -mno-stack-arg-probe \
  -I../shared -s -Wl,--exclude-all-symbols -Wl,--kill-at -Wl,--subsystem,windows -Wl,-e,_DllMainCRTStartup@12 -o "$out" \
  cinderload.c ../shared/mpq.c ../shared/inflate.c -lkernel32 -luser32
i686-w64-mingw32-objdump -p "$out" | grep -E "DLL Name" | tr '\n' ' '; echo
# (its float maths SSE2, never past it: DllMainCRTStartup leaves a processor without SSE2 to the game's own screens)
if i686-w64-mingw32-objdump -d "$out" | grep -qiE '\s(fisttp[a-z]*|lddqu|movddup|movs[hl]dup|h(add|sub)p[sd]|addsubp[sd]|pshufb|palignr|ph(add|sub)[a-z]*|pmaddubsw|pmulhrsw|psign[bwd]|pabs[bwd]|ptest|pmov[sz]x[a-z]*|p?blend[a-z]*|dpp[sd]|insertps|extractps|pextr[bdq]|pinsr[bdq]|pmin(sb|sd|uw|ud)|pmax(sb|sd|uw|ud)|pmuldq|pmulld|packusdw|pcmp[a-z]*q|pcmp[ei]str[im]|crc32[a-z]*|popcnt|round[sp][sd]|movntdqa|mpsadbw|phminposuw|v[a-z]{3,})\s'; then
  echo "$out: an instruction past SSE2" >&2; exit 1
fi
ls -la "$out"
