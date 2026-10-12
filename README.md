# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client. Each loading screen shows a map of where you are going and a loading bar, and you choose which of each.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- The map is one of two, your choice:
  - **Parchment**: the world painted as the game's own world map paints it, from the client's own map tiles, by candlelight. It opens on the whole world over the Maelstrom and glides in one straight zoom to where you arrive as the bar fills, the continent's painting and then the zone's coming up as it closes in, the zone whole to its own borders and softening into the continent outside them. The zones nearby come up with it, under it, painted the same way and fading out with distance, the continent's painting showing where they have faded. To begin with, each zone that borders yours fades across its own land: at full strength where a straight line out from where you arrive enters it, gone where that line leaves it. So from Goldshire you see the northern half of Duskwood with Darkshire, gone by its southern edge, and the western side of Redridge with Lakeshire and its bridge, gone by its eastern edge; from Eastvale Logging Camp you see Redridge too. Three other fades can be chosen (style.txt, below). Arriving in a capital, the view glides to the zone around it, whole, with that zone's own neighbours under it: Elwynn Forest for Stormwind, Durotar for Orgrimmar, Dun Morogh for Ironforge, Tirisfal Glades for the Undercity, Mulgore for Thunder Bluff, Teldrassil for Darnassus, the Thalassian Highlands for Alah'Thalas. The zone and its neighbours appear together, the view going on over the continent's painting until both are ready, and the city's own detailed map comes up over the zone only in the last close-up, as the view lands. CinderLoad prepares the land of every zone once, ahead, from the client's own terrain (on the character list or while you stand idle in the world, never during a loading screen; well under a minute of work in all, about 200 KB in Data/CinderLoad), so a first visit is as quick as any other. Until that is done, while the neighbours' land is still being read on a first visit, the zone comes up at the latest as that last close-up begins, its neighbours following as soon as they are ready. The continent's name fades away as the view closes in, and the world map's own player arrow marks where you arrive. Only the parts of each zone your character has explored are painted, the rest left blank as on the world map, unless Map areas (below) is set to Full map.
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

Strongly recommended, though not required: the HD world map patch, Data/patch-M.mpq ("Reforged HD Patch-M" in the RavenCraft launcher). It makes the parchment sharp with 512-pixel map tiles; without it CinderLoad works just the same, but the client's own 256-pixel tiles are used and look soft. Its Faithful variant installs as Data/patch-M_faithful.mpq, which CinderLoad does not read.

To remove: delete CinderLoad.dll, its dlls.txt line, the folder Data/CinderLoad (with what CinderLoad keeps there, such as your choice in style.txt, each character's explored areas in explored-*.bin and explored-*.txt, and each zone's land, read once from the client's terrain, in land-*.bin), Data/patch-~.MPQ and Data/patch-~.cinderload.

Check the files against SHA256SUMS on the release.

## Choosing the map, the bar and the map's areas

Parchment, Retro and Discovered to begin with. Each is chosen on its own, and a change counts from your next loading screen.

- **On the world map (M)**: three drop-downs at the left end of its top bar, left of Continent, labelled "Loading map" (Parchment or Satellite), "Loading bar" (Retro or Neo) and "Map areas" (Discovered or Full map). Map areas is for the parchment: Discovered shows only the parts of each zone your character has discovered, as the world map does; Full map shows all of it, as though you had discovered everything. They work like the map's own Continent and Zone drop-downs. On the game's small, windowed map they are hidden along with Continent and Zone; maximise the map to use them.
- **In the chat**: `/cinderload` says what is chosen. `/cinderload map`, `/cinderload bar` or `/cinderload areas` switches one. `/cinderload map parchment`, `/cinderload map satellite`, `/cinderload bar retro`, `/cinderload bar neo`, `/cinderload areas discovered` or `/cinderload areas full` sets one (the choice's word alone, such as `/cinderload neo`, works too). Anything else is answered with how to say it, and nothing changes.
- **In a file**: the choice is kept in Data/CinderLoad/style.txt, three lines: `map parchment` or `map satellite`, `bar retro` or `bar neo`, and `areas discovered` or `areas full`. A fourth line, `fade`, sets how the parchment's zones nearby fade, one of four: `fade zone`, each zone that borders yours across its own land, at full strength where a straight line out from where you arrive enters it and gone where that line leaves it; `fade third`, the same, but at full strength over the nearest third of that line's way across it, then fading to nothing where it leaves; `fade 3600`, a circle round where you arrive, every zone with land inside it at full strength out to 1800 yards and gone at 3600; `fade 1800`, the same circle at half the size, full strength to 900 yards and gone at 1800. Without the line it is `fade zone`; a choice made in game keeps the line when the file has one and never adds it, so without it the default always applies. A line `prepare off` stops CinderLoad preparing each zone's land ahead (above), so each is read on its first visit instead; without it, or with `prepare on`, it prepares them, and a choice made in game keeps the line the same way. Any other line is ignored. You can edit it with the game closed; it may be saved as plain text, UTF-8 or Unicode (UTF-16), as Notepad saves it. A choice made in game is written to it within a quarter of a second, and again as the game quits.

The drop-downs and `/cinderload` are made by CinderLoad.dll inside the game; there is no add-on to install. They exist only while CinderLoad's view is on (see below): with the view off, the loading screens are the game's own world map zoom with a burning log, and there is no choice to make. They never change the game's own world map or its drop-downs. If they ever meet an error they turn themselves off until the game starts again, and the chat says so. If a game ever closes while they are being made, Data/CinderLoad/ui-off.txt keeps them off until you delete it; style.txt still sets the map, the bar and the map's areas.

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
