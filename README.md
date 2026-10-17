# CinderLoad

Ultrawide and widescreen loading screens for the 1.12 World of Warcraft client. Each loading screen shows a map of where you are going and a loading bar, and you choose which of each.

- Every loading screen fills the whole screen instead of sitting in a 4:3 box: 16:10, 16:9, 21:9 and 32:9.
- The map is one of two, your choice:
  - **Parchment**: the world painted as the game's own world map paints it, from the client's own map tiles, by candlelight. It opens on the whole world over the Maelstrom and glides in one straight zoom to where you arrive as the bar fills, the continent's painting and then the zone's coming up as it closes in, the zone whole to its own borders and softening into the continent outside them. The zones nearby come up with it, under it, painted the same way and fading out with distance, the continent's painting showing where they have faded. To begin with, each zone that borders yours fades across its own land: at full strength where a straight line out from where you arrive enters it, gone where that line leaves it. So from Goldshire you see the northern half of Duskwood with Darkshire, gone by its southern edge, and the western side of Redridge with Lakeshire and its bridge, gone by its eastern edge; from Eastvale Logging Camp you see Redridge too. Three other fades can be chosen (below). Arriving in a capital, the view glides to the zone around it, whole, with that zone's own neighbours under it: Elwynn Forest for Stormwind, Durotar for Orgrimmar, Dun Morogh for Ironforge, Tirisfal Glades for the Undercity, Mulgore for Thunder Bluff, Teldrassil for Darnassus, the Thalassian Highlands for Alah'Thalas. The zone and its neighbours appear together, the view going on over the continent's painting until both are ready, and the city's own detailed map comes up over the zone only in the last close-up, as the view lands. CinderLoad prepares the land of every zone once, ahead, from the client's own terrain (on the character list or while you stand idle in the world, never during a loading screen; well under a minute of work in all, about 1 MB in Data/CinderLoad), so a first visit is as quick as any other. Until that is done, while the neighbours' land is still being read on a first visit, the zone comes up at the latest as that last close-up begins, its neighbours following as soon as they are ready. The continent's name fades away as the view closes in, and the world map's own player arrow marks where you arrive. All of each zone is painted, as though your character had explored it all (Map areas, below, Full map to begin with); set to Discovered, only the parts your character has explored are painted, the rest left blank as on the world map.
  - **Satellite**: the living map of Azeroth, the whole world drawn from above in 3D, zooming in on where you arrive (for a dungeon, raid or battleground, its entrance). As it closes in, the land around where you arrive sharpens to its own detail, tile by tile, so the landing is as sharp as the screen. Cities, zones with their levels, towns, flight paths and instance entrances are marked, and the place's name appears across the top as you arrive. Ships and zeppelins sail their real routes, currents run across the sea and into the Maelstrom, and clouds drift with each zone's usual weather, always in broad daylight. With Map areas set to Discovered, the ground your character has not discovered is shown darkened and slightly blurred, where the parchment leaves it blank: each zone round where you arrive, by the game's own world map pieces, coming up with a fifth of the bar. Each zone's hidden ground ends at its own border, by the land CinderLoad prepares for the parchment (above), not over a neighbour's discovered ground; a zone whose land is not prepared yet hides a little past its border.
- The loading bar is one of two, your choice:
  - **Retro**: the game's own loading bar, filling in your class's colour.
  - **Neo**: a blade of light in your class's colour in a metal frame across the whole width of the screen, the RavenCraft logo on it.
  - Either bar's fill is one of three: Heat (Retro's own: a slow heat drifting toward the tip, dark where it is cool), Smooth (the same, even) or Blade light (Neo's own blade of light, near white along its centre), in your class's colour or CinderLoad's ember. Each bar keeps its own: Retro begins with Heat and Neo with Blade light.
- The sounds are built at run time from your own client's sound files (none are shipped).
- Everything is checked against your WoW.exe first and changed in memory only. If anything differs, that part stays off and Logs/CinderLoad.log says why. WoW.exe on disk is never changed.

## 0.4.1

- The satellite shows what your character has not discovered darkened and slightly blurred, when Map areas is Discovered.
- The satellite glides in one smooth zoom onto where you arrive: no more stopping short of the end and then rushing in.
- A teleport on the satellite opens straight on its start, halfway in, without a moment of the whole world first.

CinderLoad began inside [Azeroth Atmosphere](https://github.com/mytherceria/azeroth-atmosphere) and has its own home here from 0.1.6 on.

## Install

Download the latest release from [Releases](https://github.com/mytherceria/cinderload/releases/latest).

1. Put CinderLoad.dll in the game folder and add a line `CinderLoad.dll` to dlls.txt (VanillaFixes loads it).
2. Make a folder Data/CinderLoad and put the pack for your screen shape in it: LoadingScreens-16x10.MPQ, -16x9, -21x9 or -32x9.
3. Put CinderSat.MPQ in Data/CinderLoad too: the satellite's detail, one file for every screen shape. Without it the satellite shows its world map alone, without the sharper land as it lands.
4. Start the game. CinderLoad reads your resolution from WTF/Config.wtf and copies the matching pack into place as Data/patch-~.MPQ.

Upgrading from 0.3.0: replace CinderLoad.dll and the pack for your screen shape in Data/CinderLoad with this release's, and add CinderSat.MPQ to Data/CinderLoad (step 3). Leave Data/patch-~.MPQ alone: as the game starts, CinderLoad finds the new pack and copies it into place itself. The dlls.txt line and your choices in Data/CinderLoad/style.txt stay as they are.

Upgrading from 0.4.0: replace CinderLoad.dll alone. The packs and CinderSat.MPQ are 0.4.0's, unchanged.

Strongly recommended, though not required: the HD world map patch, Data/patch-M.mpq ("Reforged HD Patch-M" in the RavenCraft launcher). It makes the parchment sharp with 512-pixel map tiles; without it CinderLoad works just the same, but the client's own 256-pixel tiles are used and look soft. Its Faithful variant installs as Data/patch-M_faithful.mpq, which CinderLoad does not read.

To remove: delete CinderLoad.dll, its dlls.txt line, the folder Data/CinderLoad (with what CinderLoad keeps there, such as your choice in style.txt, each character's explored areas in explored-*.bin and explored-*.txt, and each zone's land, read once from the client's terrain, in land-*.bin), Data/patch-~.MPQ and Data/patch-~.cinderload.

Check the files against SHA256SUMS on the release.

## Choosing the map, the bar and the map's areas

Parchment, the Full map and Retro to begin with. Each is chosen on its own, and a change counts from your next loading screen.

- **In CinderLoad's window**: type `/cinderload` in the chat and a window opens, made of the game's own window art; drag it by its title, and close it with Close, its X or Escape. It holds:
  - **Loading map**: Parchment or Satellite, each with a picture; **Map areas** for both maps: Full map paints all of each zone, as though you had discovered everything, Discovered only the parts of each zone your character has discovered, as the world map does (on the satellite, the rest darkened and slightly blurred); and **Nearby zones, faded with distance**, how the zones round where you arrive fade: Per zone, whole zone (each zone across the whole of it, to begin with), Per zone, nearest third (each zone's nearest third whole, then fading), Circle, 3,600 yd or Circle, 1,800 yd (a circle round where you arrive).
  - **Loading bar**: Retro or Neo, each with a picture. With Neo picked, its four qualities: Liquid, Pulse, Unstable and Raw, any of them (Pulse and Raw to begin with).
  - **Bar size**: Bar thickness (50% to 200%, both bars), Bar width (20% to 100% of the screen; to begin with, Retro's own width) and Whole width of the screen. The width is Retro's alone: Neo is always the whole width of the screen, as its ends have no metal caps, so with Neo picked the width and the whole width are greyed and the window says why.
  - **Texture / effect**: Heat, Smooth or Blade light for the bar picked (on Neo, inside its frame, its orb, lightning and logo as ever), each bar keeping its own (Retro Heat and Neo Blade light to begin with), and In my class's colour (unticked: CinderLoad's ember).
  - **Preview**: the bottom of the screen in miniature with the bar at the chosen thickness and width, standing where the loading screen stands it, its fill drawn with the chosen effect in the chosen colour (your class's, or CinderLoad's ember), a still for now. Defaults puts everything back as it began.
  - The window's pictures are made in game, never shipped: the parchment from your client's own painting of the Eastern Kingdoms, by candlelight; the living map from CinderLoad's own; Neo's frame and logo CinderLoad's own; Retro's frame your client's casting-bar frame (the game's loading-bar frame itself is kept clear by CinderLoad's pack, so the game's own bar can show its fire); the fills drawn.
  - All of it is drawn on the loading screens: both bars, each at its own thickness times the slider's, the Retro frame's caps kept and its middle stretched to a wider width or cut short for a narrower one (never squeezed, so it stays as sharp as the caps), its centre where the game's own bar has it at every thickness; Neo edge to edge, its rails counted from the width of the screen, the RavenCraft logo sized with the blade; the fill and its colour as chosen; Neo's qualities.
- **In the chat**: `/cinderload map`, `/cinderload bar` or `/cinderload areas` switches one. `/cinderload map parchment`, `/cinderload map satellite`, `/cinderload bar retro`, `/cinderload bar neo`, `/cinderload areas discovered` or `/cinderload areas full` sets one (the choice's word alone, such as `/cinderload neo`, works too). Anything else is answered with how to say it, and nothing changes. These say what is chosen after the change.
- **In a file**: the choice is kept in Data/CinderLoad/style.txt, three lines: `map parchment` or `map satellite`, `bar retro` or `bar neo`, and `areas discovered` or `areas full`. A fourth line, `fade`, is how the parchment's zones nearby fade, as the window's Nearby zones choice says, one of four: `fade zone`, each zone that borders yours across its own land, at full strength where a straight line out from where you arrive enters it and gone where that line leaves it; `fade third`, the same, but at full strength over the nearest third of that line's way across it, then fading to nothing where it leaves; `fade 3600`, a circle round where you arrive, every zone with land inside it at full strength out to 1800 yards and gone at 3600; `fade 1800`, the same circle at half the size, full strength to 900 yards and gone at 1800. Without the line it is `fade zone`. It is kept when the file has it and never added by a choice of the map, the bar or the areas, so without it the default always applies; picked in the window, it is written, and kept from then on. The window adds the bar's look: `thick 100` (percent, 50 to 200), `width own` or `width 50` (percent of the screen, 20 to 100; Retro's alone), `whole off` or `whole on`, `effect heat`, `effect smooth` or `effect blade` (Retro's fill), `neofill blade`, `neofill heat` or `neofill smooth` (Neo's), `colour class` or `colour ember`, and `quality pulse raw` (any of liquid, pulse, unstable and raw, or none). A line missing keeps today's look. A line `prepare off` stops CinderLoad preparing each zone's land ahead (above), so each is read on its first visit instead; without it, or with `prepare on`, it prepares them, and a choice made in game keeps the line when the file has one and never adds it. Any other line is ignored, so an older CinderLoad reads a newer file as before. You can edit it with the game closed; it may be saved as plain text, UTF-8 or Unicode (UTF-16), as Notepad saves it. A choice made in game is written to it within a quarter of a second, and again as the game quits.

The window and `/cinderload` are made by CinderLoad.dll inside the game; there is no add-on to install. They exist only while CinderLoad's view is on (see below): with the view off, the loading screens are the game's own world map zoom with a burning log, and there is no choice to make. Nothing of CinderLoad's is on the world map, and nothing of the game's own windows is changed. If the window ever meets an error it turns itself off until the game starts again, and the chat says so. If a game ever closes while it is being made, Data/CinderLoad/ui-off.txt keeps it off until you delete it; style.txt still sets everything.

## The view

Both maps and both bars are drawn in a window of CinderLoad's own (the view), over the game, while a loading screen shows. It is on by default.

- To turn it off, create an empty file Data/CinderLoad/view-off.txt. The loading screens then keep the world map's zoom, drawn by the game, with a burning log for the bar.
- On Windows, when the game is set to take the whole screen (exclusive full screen: SET gxWindow "0" in WTF/Config.wtf, or -fullscreen), it stays off, because its window would be hidden behind the game's. In a window, the RavenCraft client's default, it shows. Create Data/CinderLoad/view.txt to try it in full screen anyway. Under Wine and Proton (Linux, SteamOS, a Mac) it runs either way.

## Build

- `dll/build.sh [out.dll]`: the DLL, with mingw-w64 (i686) and no C runtime.
- `tools/make_pack.py`: one screen shape's pack, from a client's own loading screens (needs StormLib and ImageMagick). The window ships no pictures: it makes them in game from the client's own textures and the pack's own (its living map, Neo's frame and logo), so no pack carries Blizzard art for it.
- Tests: `gcc -std=c99 -Wall -Wno-unused-function -Itests/shim -Ishared -o test_switch tests/test_switch.c shared/mpq.c shared/inflate.c -lm && ./test_switch <scratch dir> [<client folder>]`. With a client folder, every address CinderLoad uses is also checked against that client's WoW.exe. With lua5.1 installed, the window (dll/switches.lua) runs in a stubbed 1.12 UI (tests/test_lua.lua, tests/wow_ui.lua), every control driven, and the whole path from a pick in the window to style.txt, the next loading screen and back runs through tests/lua_bridge.lua; with a client folder, the world map is the client's own UIDropDownMenu.lua and WorldMapFrame.lua, read from its archives, and is checked untouched. `tools/render_panel.py` draws the window from the stubbed UI's frame tree with the client's own art. `tests/test_shape.c` covers the screen shapes.

`shared/` is the MPQ reader from Azeroth Atmosphere's IndoorRain.

## Credits

The world map is stitched from the client's own map tiles. The bar's rails are Undercity's iron, from the client too. Releases before 0.2.0 carried wallpapers from Turtle WoW's commissioned art by Lionel Schramm, Misho Tenev, Stonegut and Ghor.

## License

GPL-3.0, see LICENSE.
