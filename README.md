# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client. Each loading screen shows a map of where you are going and a loading bar, and you choose which of each.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- The map is one of two, your choice:
  - **Parchment**: the world painted as the game's own world map paints it, from the client's own map tiles, by candlelight. It opens on the whole world over the Maelstrom and glides in one straight zoom to where you arrive as the bar fills, the continent's painting and then the zone's coming up as it closes in, the zone whole to its own borders and softening into the continent outside them. The zones nearby come up with it, under it: each zone whose land lies within 1600 yards of where you arrive (the twelve nearest at most), painted the same way, at full strength out to 800 yards and fading to nothing at 1600, so from Eastvale Logging Camp you see Redridge too. The continent's name fades away as the view closes in, and the world map's own player arrow marks where you arrive. Only the parts of each zone your character has explored are painted, the rest left blank as on the world map, unless Map areas (below) is set to Full map.
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

- **In CinderLoad's window**: type `/cinderload` in the chat and a window opens, made of the game's own window art; drag it by its title, and close it with Close, its X or Escape. It holds:
  - **Loading map**: Parchment or Satellite, each with a picture; **Map areas** for the parchment: Discovered shows only the parts of each zone your character has discovered, as the world map does, Full map all of it, as though you had discovered everything; and **Nearby zones**, a slider for how far round where you arrive the parchment shows the zones nearby (off to 4000 yards).
  - **Loading bar**: Retro or Neo, each with a picture. With Neo picked, its four qualities: Liquid, Pulse, Unstable and Raw, any of them (Pulse and Raw to begin with).
  - **Bar size**: Bar thickness (50% to 200%), Bar width (20% to 100% of the screen; to begin with, each bar's own width) and Whole width of the screen.
  - **Texture / effect**: Heat, Smooth or Blade light, and In my class's colour.
  - **Preview**: the bottom of the screen in miniature with the bar at the chosen thickness and width, its fill drawn with the chosen effect in the chosen colour (your class's, or CinderLoad's ember), a still for now. Defaults puts everything back as it began.
  - The bar's thickness, width and whole width are drawn on the loading screens: both bars, each at its own thickness times the slider's, the Retro frame's caps kept and its middle stretched to a wider width or cut short for a narrower one (never squeezed, so it stays as sharp as the caps), the Neo frame's caps at the bar's ends and its rails counted from the width, the RavenCraft logo sized and placed with the blade. The effect and the class's colour are kept and read by CinderLoad.dll already, and drawn in a later release; the window says so over that box ("kept; drawn in a later version"). Neo's qualities and the zones nearby take effect now.
- **In the chat**: `/cinderload map`, `/cinderload bar` or `/cinderload areas` switches one. `/cinderload map parchment`, `/cinderload map satellite`, `/cinderload bar retro`, `/cinderload bar neo`, `/cinderload areas discovered` or `/cinderload areas full` sets one (the choice's word alone, such as `/cinderload neo`, works too). Anything else is answered with how to say it, and nothing changes. These say what is chosen after the change.
- **In a file**: the choice is kept in Data/CinderLoad/style.txt, three lines: `map parchment` or `map satellite`, `bar retro` or `bar neo`, and `areas discovered` or `areas full`. A fourth line, `nearby 1600`, sets how far from where you arrive, in yards, the parchment shows the zones nearby: at full strength out to half that distance, then fading to nothing at it. `nearby 0` turns them off, and the most is 4000. Without the line it is 1600; a choice of the map, the bar or the areas made in game keeps the line when the file has one and never adds it, so without it the default always applies (the window's Nearby zones slider, moved, writes it). The window adds the bar's look: `thick 100` (percent, 50 to 200), `width own` or `width 50` (percent of the screen, 20 to 100), `whole off` or `whole on`, `effect heat`, `effect smooth` or `effect blade`, `colour class` or `colour ember`, and `quality pulse raw` (any of liquid, pulse, unstable and raw, or none). A line missing keeps today's look. Any other line is ignored, so an older CinderLoad reads a newer file as before. You can edit it with the game closed; it may be saved as plain text, UTF-8 or Unicode (UTF-16), as Notepad saves it. A choice made in game is written to it within a quarter of a second, and again as the game quits.

The window and `/cinderload` are made by CinderLoad.dll inside the game; there is no add-on to install. They exist only while CinderLoad's view is on (see below): with the view off, the loading screens are the game's own world map zoom with a burning log, and there is no choice to make. Nothing of CinderLoad's is on the world map, and nothing of the game's own windows is changed. If the window ever meets an error it turns itself off until the game starts again, and the chat says so. If a game ever closes while it is being made, Data/CinderLoad/ui-off.txt keeps it off until you delete it; style.txt still sets everything.

## The view

Both maps and both bars are drawn in a window of CinderLoad's own (the view), over the game, while a loading screen shows. It is on by default.

- To turn it off, create an empty file Data/CinderLoad/view-off.txt. The loading screens then keep the world map's zoom, drawn by the game, with a burning log for the bar.
- On Windows, when the game is set to take the whole screen (exclusive full screen: SET gxWindow "0" in WTF/Config.wtf, or -fullscreen), it stays off, because its window would be hidden behind the game's. In a window, the RavenCraft client's default, it shows. Create Data/CinderLoad/view.txt to try it in full screen anyway. Under Wine and Proton (Linux, SteamOS, a Mac) it runs either way.

## Build

- `dll/build.sh [out.dll]`: the DLL, with mingw-w64 (i686) and no C runtime.
- `tools/make_pack.py`: one screen shape's pack, from a client's own loading screens (needs StormLib and ImageMagick). `--panel <dir>` adds the window's pictures, made by `tools/make_panel_art.py` from two loading screens recorded in game.
- Tests: `gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o test_switch tests/test_switch.c shared/mpq.c shared/inflate.c -lm && ./test_switch <scratch dir> [<client folder>]`. With a client folder, every address CinderLoad uses is also checked against that client's WoW.exe. With lua5.1 installed, the window (dll/switches.lua) runs in a stubbed 1.12 UI (tests/test_lua.lua, tests/wow_ui.lua), every control driven, and the whole path from a pick in the window to style.txt, the next loading screen and back runs through tests/lua_bridge.lua; with a client folder, the world map is the client's own UIDropDownMenu.lua and WorldMapFrame.lua, read from its archives, and is checked untouched. `tools/render_panel.py` draws the window from the stubbed UI's frame tree with the client's own art. `tests/test_shape.c` covers the screen shapes.

`shared/` is the MPQ reader from Azeroth Atmosphere's IndoorRain.

## Credits

The world map is stitched from the client's own map tiles. The bar's rails are Undercity's iron, from the client too. Releases before 0.2.0 carried wallpapers from Turtle WoW's commissioned art by Lionel Schramm, Misho Tenev, Stonegut and Ghor.

## License

GPL-3.0, see LICENSE.
