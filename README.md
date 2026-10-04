# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client, with a charred log for a loading bar and fire in its cracks.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- The bar is a burning log. The fire is uncovered as the bar fills, with live lava behind a soft front, in the picture's own colour: orange, aqua, arcane or fel.
- The fire has a sound, built at run time from your own client's fire loops (none are shipped). It follows the fire from left to right.
- Eastern Kingdoms and Kalimdor show a wallpaper drawn at random from Turtle WoW's commissioned art. Dungeons, raids and battlegrounds keep their own screens, widened.
- Everything is checked against your WoW.exe first and changed in memory only. If anything differs, that part stays off and Logs/CinderLoad.log says why. WoW.exe on disk is never changed.

CinderLoad began inside [Azeroth Atmosphere](https://github.com/mytherceria/azeroth-atmosphere) and has its own home here from 0.1.6 on.

## Install

Download the latest release from [Releases](https://github.com/mytherceria/cinderload/releases/latest).

1. Put CinderLoad.dll in the game folder and add a line `CinderLoad.dll` to dlls.txt (VanillaFixes loads it).
2. Make a folder Data/CinderLoad and put the pack for your screen shape in it: LoadingScreens-16x10.MPQ, -16x9, -21x9 or -32x9.
3. Start the game. CinderLoad reads your resolution from WTF/Config.wtf and copies the matching pack into place as Data/patch-~.MPQ.

To remove: delete CinderLoad.dll, its dlls.txt line, the folder Data/CinderLoad, Data/patch-~.MPQ and Data/patch-~.cinderload.

Check the files against SHA256SUMS on the release.

## Build

- `dll/build.sh [out.dll]`: the DLL, with mingw-w64 (i686) and no C runtime.
- `tools/make_pack.py`: one screen shape's pack, from a client's own loading screens (needs StormLib and ImageMagick).
- Tests: `gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o test_switch tests/test_switch.c shared/mpq.c shared/inflate.c -lm && ./test_switch <scratch dir> [<client folder>]`. With a client folder, every address CinderLoad uses is also checked against that client's WoW.exe. `tests/test_shape.c` covers the screen shapes.

`shared/` is the MPQ reader from Azeroth Atmosphere's IndoorRain.

## Credits

The wallpapers are Turtle WoW's commissioned art, credited on each screen: Lionel Schramm, Misho Tenev, Stonegut and Ghor. Two more, a handheld game screen and an 8-bit map, are drawn from scratch for CinderLoad.

## License

GPL-3.0, see LICENSE.
