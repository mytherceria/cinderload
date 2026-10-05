# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client: the world map zooms in on where you are going while the bar burns in your class's colour.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- Every loading screen is the world map, stitched from the client's own map tiles. As the bar fills, the view zooms in on where you are arriving; for a dungeon, raid or battleground, on its entrance. Cities, zones with their levels, towns, flight paths and every instance entrance are marked, more of them as you get closer, and currents run across the sea.
- The bar is a burning log, the fire uncovered as the bar fills, in your class's colour: Druid orange, Hunter green, Mage light blue, Paladin pink, Priest white, Rogue yellow, Shaman blue, Warlock purple, Warrior tan.
- Druids get their own frame on 21:9 screens: a wrought iron and stained glass banner across the top with the bear, cat, stag and raven, and a garland of leaves and flowers that grows in along the bottom, with wisps and its own sound building as it loads. The other classes follow, one at a time.
- The sounds are built at run time from your own client's sound files (none are shipped).
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

The world map is stitched from the client's own map tiles. The banner, the garland and the bar are drawn from code; the animals on the banner are traced from the client's own models. Releases before 0.2.0 carried wallpapers from Turtle WoW's commissioned art by Lionel Schramm, Misho Tenev, Stonegut and Ghor.

## License

GPL-3.0, see LICENSE.
