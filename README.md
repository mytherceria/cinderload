# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client. Each loading screen shows a map of where you are going and a loading bar, and you choose which of each.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- The map is one of two, your choice:
  - **Parchment**: the world painted as the game's own world map paints it, from the client's own map tiles, by candlelight. It opens on the whole world over the Maelstrom and glides in one straight zoom to where you arrive as the bar fills, the continent's painting and then the zone's coming up as it closes in. Only the parts of the zone your character has explored are painted; the rest is left blank, as on the world map.
  - **Satellite**: the living map of Azeroth, stitched from the client's own map tiles, zooming in on where you arrive (for a dungeon, raid or battleground, its entrance). Cities, zones with their levels, towns, flight paths and instance entrances are marked, and the place's name appears across the top as you arrive. Ships and zeppelins sail their real routes, currents run across the sea and into the Maelstrom, clouds drift with each zone's usual weather, and the light follows the realm's clock.
- The loading bar is one of two, your choice:
  - **Retro**: the game's own loading bar, filling in your class's colour.
  - **Neo**: a blade of light in your class's colour in a metal frame, the RavenCraft logo on it.
- The sounds are built at run time from your own client's sound files (none are shipped).
- Everything is checked against your WoW.exe first and changed in memory only. If anything differs, that part stays off and Logs/CinderLoad.log says why. WoW.exe on disk is never changed.

CinderLoad began inside [Azeroth Atmosphere](https://github.com/mytherceria/azeroth-atmosphere) and has its own home here from 0.1.6 on.

## Install

Download the latest release from [Releases](https://github.com/mytherceria/cinderload/releases/latest).

1. Put CinderLoad.dll in the game folder and add a line `CinderLoad.dll` to dlls.txt (VanillaFixes loads it).
2. Make a folder Data/CinderLoad and put the pack for your screen shape in it: LoadingScreens-16x10.MPQ, -16x9, -21x9 or -32x9.
3. Start the game. CinderLoad reads your resolution from WTF/Config.wtf and copies the matching pack into place as Data/patch-~.MPQ.

Optional: the HD world map patch, Data/patch-M.mpq ("Reforged HD Patch-M" in the RavenCraft launcher), makes the parchment sharp with 512-pixel map tiles; without it the client's own 256-pixel tiles are used and look soft. Its Faithful variant installs as Data/patch-M_faithful.mpq, which CinderLoad does not read.

To remove: delete CinderLoad.dll, its dlls.txt line, the folder Data/CinderLoad, Data/patch-~.MPQ and Data/patch-~.cinderload.

Check the files against SHA256SUMS on the release.

## Choosing the map and the bar

Parchment and Retro to begin with. Each is chosen on its own, and a change counts from your next loading screen.

- **On the world map (M)**: two drop-downs at the left end of its top bar, left of Continent, labelled "Loading map" (Parchment or Satellite) and "Loading bar" (Retro or Neo). They work like the map's own Continent and Zone drop-downs. On the game's small, windowed map they are hidden along with Continent and Zone; maximise the map to use them.
- **In the chat**: `/cinderload` says what is chosen. `/cinderload map` or `/cinderload bar` switches one. `/cinderload map parchment`, `/cinderload map satellite`, `/cinderload bar retro` or `/cinderload bar neo` sets one (the choice's word alone, such as `/cinderload neo`, works too). Anything else is answered with how to say it, and nothing changes.
- **In a file**: the choice is kept in Data/CinderLoad/style.txt, two lines: `map parchment` or `map satellite`, and `bar retro` or `bar neo`. You can edit it with the game closed; it may be saved as plain text, UTF-8 or Unicode (UTF-16), as Notepad saves it. A choice made in game is written to it within a quarter of a second, and again as the game quits.

The drop-downs and `/cinderload` are made by CinderLoad.dll inside the game; there is no add-on to install. They exist only while CinderLoad's view is on (see below): with the view off, the loading screens are the game's own world map zoom with a burning log, and there is no choice to make. They never change the game's own world map or its drop-downs. If they ever meet an error they turn themselves off until the game starts again, and the chat says so. If a game ever closes while they are being made, Data/CinderLoad/ui-off.txt keeps them off until you delete it; style.txt still sets the map and the bar.

## The view

Both maps and both bars are drawn in a window of CinderLoad's own (the view), over the game, while a loading screen shows. It is on by default.

- To turn it off, create an empty file Data/CinderLoad/view-off.txt. The loading screens then keep the world map's zoom, drawn by the game, with a burning log for the bar.
- On Windows, when the game is set to take the whole screen (exclusive full screen: SET gxWindow "0" in WTF/Config.wtf, or -fullscreen), it stays off, because its window would be hidden behind the game's. In a window, the RavenCraft client's default, it shows. Create Data/CinderLoad/view.txt to try it in full screen anyway. Under Wine and Proton (Linux, SteamOS, a Mac) it runs either way.

## Build

- `dll/build.sh [out.dll]`: the DLL, with mingw-w64 (i686) and no C runtime.
- `tools/make_pack.py`: one screen shape's pack, from a client's own loading screens (needs StormLib and ImageMagick).
- Tests: `gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o test_switch tests/test_switch.c shared/mpq.c shared/inflate.c -lm && ./test_switch <scratch dir> [<client folder>]`. With a client folder, every address CinderLoad uses is also checked against that client's WoW.exe. With lua5.1 installed, the world map's drop-downs (dll/switches.lua) run in a stubbed 1.12 UI (tests/test_lua.lua, tests/wow_ui.lua), and the whole path from a pick on the map to style.txt and the next loading screen runs through tests/lua_bridge.lua; with a client folder, against the client's own UIDropDownMenu.lua and WorldMapFrame.lua, read from its archives. `tests/test_shape.c` covers the screen shapes.

`shared/` is the MPQ reader from Azeroth Atmosphere's IndoorRain.

## Credits

The world map is stitched from the client's own map tiles. The bar's rails are Undercity's iron, from the client too. Releases before 0.2.0 carried wallpapers from Turtle WoW's commissioned art by Lionel Schramm, Misho Tenev, Stonegut and Ghor.

## License

GPL-3.0, see LICENSE.
