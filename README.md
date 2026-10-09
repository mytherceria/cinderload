# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client: a living map of Azeroth glides down to where you are going, while a blade of light fills in your class's colour between two purple ravens.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- Every loading screen is the world map, stitched from the client's own map tiles. As the bar fills, the view zooms in on where you are arriving; for a dungeon, raid or battleground, on its entrance. Cities, zones with their levels, towns, flight paths and every instance entrance are marked, more of them as you get closer.
- The place's name appears across the top as you arrive: the continent, the zone, then the part of the zone you land in. Under the zone's name is its level range, coloured against your level as the game colours quests, and whose territory it is.
- The map is alive. Ships and zeppelins sail their real routes, currents run across the sea and into the Maelstrom, clouds drift with each zone's usual rain, snow or sand, and the light follows the realm's clock, from dawn to night.
- The bar is a blade of light in your class's colour: Druid orange, Hunter green, Mage light blue, Paladin pink, Priest white, Rogue yellow, Shaman blue, Warlock purple, Warrior tan. Two ravens in RavenCraft purple stand at its ends, in honour of RavenCraft.
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

## The living map's window

The living map, the place's name, the ships, the weather and the blade are drawn in a window of CinderLoad's own, over the game, while a loading screen shows. It is on by default.

- To turn it off, create an empty file Data/CinderLoad/view-off.txt. The loading screens then keep the world map's zoom, drawn by the game, with a burning log for the bar.
- On Windows, when the game is set to take the whole screen (exclusive full screen: SET gxWindow "0" in WTF/Config.wtf, or -fullscreen), it stays off, because its window would be hidden behind the game's. In a window, the RavenCraft client's default, it shows. Create Data/CinderLoad/view.txt to try it in full screen anyway. Under Wine and Proton (Linux, SteamOS, a Mac) it runs either way.

## Build

- `dll/build.sh [out.dll]`: the DLL, with mingw-w64 (i686) and no C runtime.
- `tools/make_pack.py`: one screen shape's pack, from a client's own loading screens (needs StormLib and ImageMagick).
- Tests: `gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o test_switch tests/test_switch.c shared/mpq.c shared/inflate.c -lm && ./test_switch <scratch dir> [<client folder>]`. With a client folder, every address CinderLoad uses is also checked against that client's WoW.exe. `tests/test_shape.c` covers the screen shapes.

`shared/` is the MPQ reader from Azeroth Atmosphere's IndoorRain.

## Credits

The world map is stitched from the client's own map tiles. The ravens are made from the client's own raven model, reposed and recoloured, and the bar's rails are Undercity's iron, from the client too. Releases before 0.2.0 carried wallpapers from Turtle WoW's commissioned art by Lionel Schramm, Misho Tenev, Stonegut and Ghor.

## License

GPL-3.0, see LICENSE.
