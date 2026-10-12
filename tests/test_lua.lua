-- CinderLoad's window (dll/switches.lua: /cinderload, his design of 11 Oct 2026) and the DLL's probe, run as the DLL
-- sends them (test_switch.c writes them out) against a stubbed 1.12 UI (tests/wow_ui.lua): Lua 5.1 with 5.0's limits
-- put on (no string methods, only 5.0's library; the source checked for 5.1-only syntax). Every control driven as a
-- player drives it; the CVar it leaves checked word for word. With the client's own UIDropDownMenu.lua and
-- WorldMapFrame.lua (framexml=<dir>), the world map is the client's own, and is checked untouched.
--   lua5.1 tests/test_lua.lua <dir with probe.lua, setup.lua, setup-satellite-neo.lua, setup-full.lua, setup-look.lua>
--          [layout=<out>] [widths=<in>] [framexml=<dir>]
-- One line a check: "ok <what>" or "FAIL <what>"; test_switch.c counts them.
local here = string.gsub(arg[0], "[^/]*$", "")
if here == "" then here = "./" end
local UI = dofile(here .. "wow_ui.lua")
local dir = arg[1]
local opt = {}
for i = 2, table.getn(arg) do
    local _, _, k, v = string.find(arg[i], "^(%a+)=(.*)$")
    if k and v ~= "" then opt[k] = v end
end
local checks, fails = 0, 0
local function Check(cond, what)
    checks = checks + 1
    if cond then print("ok " .. what) else fails = fails + 1; print("FAIL " .. what) end
end
local function ReadFile(p)
    local f = io.open(p, "rb")
    if not f then return nil end
    local s = f:read("*a")
    f:close()
    return s
end
local probeSrc, setupSrc = ReadFile(dir .. "/probe.lua"), ReadFile(dir .. "/setup.lua")
local setupSN, setupFull = ReadFile(dir .. "/setup-satellite-neo.lua"), ReadFile(dir .. "/setup-full.lua")
local setupLook = ReadFile(dir .. "/setup-look.lua")
Check(probeSrc and setupSrc and setupSN and setupFull and setupLook, "the DLL's probe and its switches' chunk written out")
if not (probeSrc and setupSrc and setupSN and setupFull and setupLook) then print("done"); os.exit(1) end

-- 5.1-only syntax (and 5.2+ tokens) the client's Lua 5.0 would not take: found outside strings and comments
local function Lex50(src)
    local code, i, n = {}, 1, string.len(src)
    while i <= n do
        local c = string.sub(src, i, i)
        if string.sub(src, i, i + 1) == "--" then
            if string.sub(src, i + 2, i + 3) == "[[" then return "a long comment" end
            local e = string.find(src, "\n", i, true)
            i = e or n + 1
        elseif c == '"' or c == "'" then
            local j = i + 1
            while j <= n and string.sub(src, j, j) ~= c do
                if string.sub(src, j, j) == "\\" then j = j + 1 end
                if string.sub(src, j, j) == "\n" then return "a string across lines" end
                j = j + 1
            end
            table.insert(code, " \"\" ")
            i = j + 1
        elseif string.sub(src, i, i + 1) == "[[" or string.sub(src, i, i + 1) == "[=" then
            return "a long string"
        else
            table.insert(code, c)
            i = i + 1
        end
    end
    local s = table.concat(code)
    local bad = { { "#", "the length operator" }, { "%", "the modulo operator" }, { "...", "varargs" }, { "::", "a label" },
                  { "//", "floor division" }, { "&", "a bitwise and" }, { "<<", "a shift" }, { ">>", "a shift" },
                  { "|", "a bitwise or" } }
    for _, b in ipairs(bad) do
        if string.find(s, b[1], 1, true) then return b[2] end
    end
    if string.find(s, "~[^=]") then return "a bitwise not" end
    for _, w in ipairs({ "goto", "select", "gmatch", "match", "maxn", "fmod", "loadstring", "setfenv", "getfenv", "module",
                         "require", "load", "rawlen", "rawequal", "HookScript", "SetSize", "SetShown", "SetFormattedText",
                         "UIDropDownMenu_CreateInfo" }) do
        if string.find(s, "%f[%w_]" .. w .. "%f[^%w_]") then return "not in 1.12: " .. w end
    end
    return nil
end
Check(Lex50(setupSrc) == nil, "the switches' chunk: Lua 5.0's syntax and 1.12's API only (" .. tostring(Lex50(setupSrc)) .. ")")
Check(Lex50(probeSrc) == nil, "the probe: Lua 5.0's syntax only (" .. tostring(Lex50(probeSrc)) .. ")")
Check(string.find(setupSrc, '^local CL_MAP, CL_BAR, CL_AREAS = "parchment", "retro", "discovered"\n') ~= nil and
      string.find(setupSN, '^local CL_MAP, CL_BAR, CL_AREAS = "satellite", "neo", "discovered"\n') ~= nil and
      string.find(setupFull, '^local CL_MAP, CL_BAR, CL_AREAS = "parchment", "retro", "full"\n') ~= nil, "the chunk begins with the choice the DLL holds")
local DEF = "nearby 1600 thick 100 width own whole off effect heat colour class quality pulse raw"
local LOOK = "nearby 600 thick 150 width 60 whole on effect blade colour ember quality liquid unstable"
Check(string.find(setupSrc, '^[^\n]*\nlocal CL_LOOK = "' .. DEF .. '"\n%-%- CinderLoad') ~= nil and
      string.find(setupLook, '\nlocal CL_LOOK = "' .. LOOK .. '"\n', 1, true) ~= nil,
      "and its second line the zones nearby and the bar's look the DLL holds, as the CVar carries them")

-- string widths, as the client's font gives them (the look's measure, else about 5.5 a letter at 10)
if opt.widths then
    local w = ReadFile(opt.widths)
    if w then
        for t, n in string.gmatch(w, "([^\t\n]+)\t([%d%.]+)\n") do UI.widths[t] = tonumber(n) end
    end
end

-- no string methods (5.0 has no metatable on strings)
getmetatable("").__index = nil

local real = opt.framexml
local world, env
local function Fresh(o)
    o = o or {}
    o.frameXml = real
    world = UI.Fresh(o)
    env = world.env
    return world
end
local function Run(src, name) return UI.Run50(src, name) end
local function P(name) return env["CinderLoadPanel" .. (name or "")] end
local function Said(s)
    for _, t in ipairs(world.chat) do if string.find(t.text, s, 1, true) then return t end end
end
local function Slash(msg) world.chat = {}; env.SlashCmdList.CINDERLOAD(msg) end
local function OpenMap() if not env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end; UI.Frame() end
local function CloseMap() if env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end end
local function NoErrors() return table.getn(world.errors) == 0 end
local function Near(a, b, e) return a and b and math.abs(a - b) < (e or 0.01) end
local function CV() return world.cvars.CinderLoadStyle end
local function Value(three, rest) return three .. " " .. (rest or DEF) end
-- where a region is in the window, in the mockup's units (760 x 608, from its top left, y down)
local function PB(r)
    local l, b, rr, t = UI.Rect(r)
    local pl, pb, pr, pt = UI.Rect(P())
    if not l or not pl then return nil end
    local es = P():GetEffectiveScale()
    return { l = (l - pl) / es, t = (pt - t) / es, r = (rr - pl) / es, b = (pt - b) / es }
end
local function At(r, x, y, w, h)
    local q = PB(r)
    return q and Near(q.l, x) and Near(q.t, y) and (not w or Near(q.r - q.l, w)) and (not h or Near(q.b - q.t, h))
end
local BLIZZ = { "ToggleDropDownMenu", "UIDropDownMenu_Initialize", "UIDropDownMenu_AddButton", "UIDropDownMenu_SetSelectedValue",
                "UIDropDownMenu_SetWidth", "UIDropDownMenu_SetText", "UIDropDownMenu_Refresh", "UIDropDownMenuButton_OnClick",
                "CloseDropDownMenus", "ToggleWorldMap", "WorldMapFrame_Minimize", "WorldMapFrame_Maximize", "WorldMapFrame_Update",
                "WorldMapContinentsDropDown_OnLoad", "WorldMapZoneDropDown_OnLoad", "WorldMapContinentButton_OnClick", "HideUIPanel",
                "ShowUIPanel", "SetCVar", "GetCVar", "CreateFrame", "PlaySound" }
local function Functions() local t = {}; for _, n in ipairs(BLIZZ) do t[n] = env[n] end; return t end
local function SameFunctions(t) for _, n in ipairs(BLIZZ) do if env[n] ~= t[n] then return false end end; return true end
local function Scripts()
    local t = {}
    for _, f in ipairs(world.made) do
        if f.blizzard then for h, fn in pairs(f.scripts) do t[tostring(f) .. "." .. h] = fn end end
    end
    return t
end
local function SameScripts(t)
    local now = Scripts()
    for k, v in pairs(t) do if now[k] ~= v then return false end end
    for k, v in pairs(now) do if t[k] ~= v then return false end end
    return true
end
local function Touched() return table.concat(world.touched, ", ") end
-- nothing of ours on the world map: no frame of ours made under it, none of its old drop-downs, the map's own as ever
local function MapUntouched()
    for _, f in ipairs(world.made) do
        if not f.blizzard then
            local p = f
            while p do if p == env.WorldMapFrame then return false end; p = p.parent end
        end
    end
    for k in pairs(env) do if type(k) == "string" and string.find(k, "^CinderLoadMap") then return false end end
    return true
end
print("the world map's code: " .. (real and "the client's own UIDropDownMenu.lua and WorldMapFrame.lua" or "this test's stand-ins"))

-- ---- the probe ----
Fresh({ cvar = false })
local ok, err = Run(probeSrc, "probe")
Check(ok, "the probe never raises, even before the CVar is there (" .. tostring(err) .. ")")
Fresh()
Run(probeSrc, "probe")
Check(CV() == "?", "the probe: the switches not there (a login, a /reload), the CVar marked \"?\"")
Fresh({ noMap = true })
Run(probeSrc, "probe")
Check(CV() == "parchment retro discovered", "the probe: no world map frame (not in the world's UI), nothing marked")

-- ---- made, at a login ----
Fresh({ cvar = "?" })
local fns, scripts = Functions(), Scripts()
ok, err = Run(setupSrc)
Check(ok and NoErrors(), "the chunk runs whole (" .. tostring(err) .. table.concat(world.errors, "; ") .. ")")
Check(CV() == Value("parchment retro discovered"), "made: the CVar told the whole choice held, the reach and the look with it (the DLL hears that it ran): " .. tostring(CV()))
Check(type(env.CinderLoad) == "table" and env.CinderLoad.up == 1 and not env.CinderLoad.broken, "made: CinderLoad.up, not broken")
Check(not P() and env.SLASH_CINDERLOAD1 == "/cinderload" and type(env.SlashCmdList.CINDERLOAD) == "function" and table.getn(world.chat) == 0,
      "made quietly: /cinderload, and no window until it is asked for")
Check(MapUntouched() and Touched() == "" and SameFunctions(fns) and SameScripts(scripts),
      "nothing on the world map (the map is CinderMap's): no frame of ours on it, none of the old drop-downs; nothing of the game's changed (" .. Touched() .. ")")

-- ---- /cinderload alone: the window ----
world.sounds = {}
Slash("")
local p = P()
Check(p and p:IsVisible() and p.parent == env.UIParent and p.kind == "Frame" and table.getn(world.chat) == 0 and world.sounds[1] == "igMainMenuOpen" and NoErrors(),
      "/cinderload alone: the window opens over the game (its own frame on UIParent), with the game's open sound, nothing said")
Check(p.strata == "DIALOG" and p.toplevel and p.movable and p.clamped and p.mouse and p.w == 760 and p.h == 608,
      "a dialog: 760 x 608 as the mockup, on top, movable, kept on the screen, taking the mouse (no click through to the world)")
local cx, cy = p:GetCenter()
local ux, uy = env.UIParent:GetCenter()
Check(Near(cx, ux) and Near(cy, uy + 40), "it opens in the middle of the screen, a little high")
local bd = p.backdrop
Check(bd and bd.edgeFile == "Interface\\DialogFrame\\UI-DialogBox-Border" and bd.edgeSize == 32 and bd.insets.left == 11 and bd.insets.top == 12 and
      p.backdropColor[1] == 0 and Near(p.backdropColor[4], 0.88), "the client's dialog border (UI-DialogBox-Border, 32), its black 88% opaque (the plus)")
local fills = { n = 0, other = "" }
for _, f in ipairs(world.made) do
    if not f.blizzard and f.backdrop and f.backdrop.bgFile and f.backdropColor then          -- (a colour set on a fill of ours)
        fills.n = fills.n + 1
        if f.backdrop.bgFile ~= "Interface\\ChatFrame\\ChatFrameBackground" then fills.other = fills.other .. " " .. f.backdrop.bgFile end
    end
end
Check(bd.bgFile == "Interface\\ChatFrame\\ChatFrameBackground" and fills.n == 6 and fills.other == "",
      "the window's and its five boxes' fill the client's plain opaque white (ChatFrameBackground), so 88% black and the boxes' grey are drawn as set, not multiplied by the tooltip's see-through grey:" .. fills.other)
local head = P("Title")
local htex = {}
for _, k in ipairs(head.kids) do if k.kind == "Texture" and k.texture then table.insert(htex, k) end end
Check(table.getn(htex) == 3 and htex[1].texture == "Interface\\DialogFrame\\UI-DialogBox-Header" and At(head, 280, -16, 200, 48) and
      Near(htex[1].texcoord[1], 59 / 256) and Near(htex[3].texcoord[2], 196 / 256) and head.drag == "LeftButton" and head.mouse,
      "the title: the client's dialog header in three (its caps kept, its middle stretched), 200 wide over the top edge, dragged by the left button")
local title = env.CinderLoad.ui.title
local layered = title.parent == head and title.layer == "OVERLAY"
for _, k in ipairs(head.kids) do if k.kind == "Texture" and k.layer == "OVERLAY" then layered = false end end
Check(title.text == "CinderLoad" and title.fontFile == "Fonts\\MORPHEUS.TTF" and title.size == 18 and title.color[1] == 1 and Near(title.color[2], 0.82) and layered,
      "its word, \"CinderLoad\", in the client's MORPHEUS at 18, gold, the header's own and over its art and its darker plate (drawn after them)")
Check(At(P("Close"), 724, 4, 32, 32) and P("Close").kind == "Button" and env[P("Close").name] and
      (function() for _, k in ipairs(P("Close").kids) do if k.role == "normal" then return k.texture == "Interface\\Buttons\\UI-Panel-MinimizeButton-Up" end end end)(),
      "the close X: the client's UIPanelCloseButton at the top right")
local present = 0
for _, f in ipairs(env.UISpecialFrames) do if f == "CinderLoadPanel" then present = present + 1 end end
Check(present == 1, "Escape closes it as it closes the game's own windows (one entry in UISpecialFrames)")
-- every word: its size, the mockup's, and none smaller than 12
local small, texts, golds = 0, {}, {}
for _, f in ipairs(world.made) do
    if f.kind == "FontString" and not f.blizzard and f.text and f.text ~= "" then
        if (f.size or 0) < 12 then small = small + 1 end
        texts[f.text] = f
    end
end
Check(small == 0 and texts["Loading map"] and texts["Loading map"].size == 14 and texts["Bar size"] and texts["Texture / effect"] and texts["Preview"] and
      texts["Loading bar"] and texts["Loading map"].color[2] == 0.82 and texts["Parchment"] and texts["Parchment"].size == 13 and
      texts["a still here; it moves in game"] and texts["Drag the title to move this window.  Each change shows from your next loading screen."],
      "its words as the mockup's: the sections' titles gold at 14, the choices white at 13, nothing under 12 (larger type: the plus)")
local coming = env.CinderLoad.ui.coming
local saidOver = 0
for _, f in ipairs(world.made) do if f.kind == "FontString" and f.text == "kept; drawn in a later version" then saidOver = saidOver + 1 end end
Check(coming and table.getn(coming) == 1 and saidOver == 1 and coming[1].text == "kept; drawn in a later version" and coming[1]:IsVisible() and
      coming[1].size == 12 and coming[1].color[1] == 1 and PB(coming[1]).l > PB(texts["Texture / effect"]).r + 8 and
      PB(coming[1]).b < 300 and PB(coming[1]).b > 260 and Near(PB(coming[1]).r, 730) and
      texts["Drag the title to move this window.  Each change shows from your next loading screen."],
      "over Texture / effect only, at its right, clear of its title: kept, drawn in a later version (no loading screen draws the effect or colour yet); Bar size says nothing, since the loading screens draw it now, so the header's line stays true")
-- where everything is (the mockup's units)
local u = env.CinderLoad.ui
local cards = {}
for _, c in ipairs(u.cards) do cards[c.key .. " " .. c.value] = c.button end
Check(At(cards["map parchment"], 40, 96, 150, 80) and At(cards["map satellite"], 202, 96, 150, 80) and At(cards["bar retro"], 40, 400, 150, 31) and
      At(cards["bar neo"], 202, 400, 150, 31) and At(cards["effect heat"], 410, 312, 96, 24) and At(cards["effect smooth"], 516, 312, 96, 24) and
      At(cards["effect blade"], 622, 312, 96, 24), "the pictures where the mockup has them: the two maps' 150 x 80, the two bars' 150 x 31, the three effects' 96 x 24")
Check(At(P("mapparchment"), 40, 184, 18, 18) and At(P("mapsatellite"), 202, 184, 18, 18) and At(P("areasdiscovered"), 40, 250, 18, 18) and
      At(P("areasfull"), 202, 250, 18, 18) and At(P("barretro"), 40, 440, 18, 18) and At(P("barneo"), 202, 440, 18, 18),
      "the radio buttons where the mockup has them (the client's UIRadioButtonTemplate)")
Check(At(P("nearbySlider"), 42, 308, 308, 17) and At(P("thickSlider"), 410, 114, 308, 17) and At(P("widthSlider"), 410, 178, 308, 17) and
      At(P("Whole"), 408, 222, 28, 28) and At(P("Colour"), 408, 362, 28, 28) and At(P("Preview"), 406, 448, 316, 88) and
      At(P("Defaults"), 30, 568, 120, 26) and At(P("CloseButton"), 610, 568, 120, 26),
      "the sliders, check boxes, preview and the two buttons where the mockup has them")
Check(P("nearbySlider").backdrop.edgeFile == "Interface\\Buttons\\UI-SliderBar-Border" and env[P("nearbySlider").name .. "Thumb"].texture ==
      "Interface\\Buttons\\UI-SliderBar-Button-Horizontal" and env[P("Defaults").name .. "Text"].text == "Defaults" and
      env[P("CloseButton").name .. "Text"].text == "Close" and env[P("Whole").name .. "Text"].text == "Whole width of the screen",
      "the client's own: OptionsSliderTemplate's bar and thumb, UIPanelButtonTemplate's red buttons, UICheckButtonTemplate's box")
-- what lies inside a box is drawn over it: the box's own child (a frame's children are drawn over all of its own art,
-- so a word or control of the window's own inside a box would be under the box's backdrop)
local boxes, under = {}, ""
for _, f in ipairs(world.made) do
    if f.backdrop and f.backdrop.edgeFile == "Interface\\Tooltips\\UI-Tooltip-Border" and f.backdrop.edgeSize == 16 then table.insert(boxes, f) end
end
for _, f in ipairs(world.made) do
    local q = not f.blizzard and f ~= p and (f.kind ~= "Texture") and PB(f)
    if q and f:IsVisible() then
        for _, b in ipairs(boxes) do
            local bq = PB(b)
            if q.l >= bq.l and q.r <= bq.r and q.t >= bq.t and q.b <= bq.b and f ~= b then
                local a = f.parent
                while a and a ~= b do a = a.parent end
                if not a then under = under .. " " .. tostring(f.name or f.text or f.kind) end
            end
        end
    end
end
Check(table.getn(boxes) == 5 and under == "", "the five sections' boxes, and everything inside each is drawn over it, never under its backdrop:" .. under)
-- the choice held, shown
local function Shows(map, areas, bar)
    return P("map" .. map):GetChecked() and not P("map" .. (map == "parchment" and "satellite" or "parchment")):GetChecked() and
           P("areas" .. areas):GetChecked() and not P("areas" .. (areas == "full" and "discovered" or "full")):GetChecked() and
           P("bar" .. bar):GetChecked() and not P("bar" .. (bar == "retro" and "neo" or "retro")):GetChecked()
end
local function Gold(key, value)
    for _, c in ipairs(u.cards) do
        if c.key == key and (c.value == value) ~= (c.on == true) then return false end
    end
    return true
end
local function SliderSays(key, v, text)
    local s = u.sliders[key]
    return Near(s.slider:GetValue(), v) and s.value.text == text
end
Check(Shows("parchment", "discovered", "retro") and Gold("map", "parchment") and Gold("bar", "retro") and Gold("effect", "heat") and
      SliderSays("nearby", 1600, "1600 yards") and SliderSays("thick", 100, "100%") and SliderSays("width", 30, "each bar's own") and
      not P("Whole"):GetChecked() and P("Colour"):GetChecked() and u.words[1]:IsVisible() and not u.neoTitle:IsVisible() and
      not P("QualityPulse"):IsVisible(), "the choice held shows: Parchment, Discovered, 1600 yards, Retro (its words), 100%, each bar's own width (Retro's on this screen), Heat in my class's colour")

-- ---- every control, as a player uses it ----
world.sounds = {}
UI.Click(P("mapsatellite"))
Check(CV() == Value("satellite retro discovered") and Shows("satellite", "discovered", "retro") and Gold("map", "satellite") and
      world.sounds[1] == "igMainMenuOptionCheckBoxOn" and NoErrors(), "Satellite's radio: the CVar says so, its radio alone checked, its picture framed in gold")
UI.Click(cards["map parchment"])
Check(CV() == Value("parchment retro discovered") and Shows("parchment", "discovered", "retro") and Gold("map", "parchment"), "the parchment's picture clicked: chosen")
UI.Click(P("areasfull"))
Check(CV() == Value("parchment retro full") and Shows("parchment", "full", "retro"), "Full map's radio: the map's areas full")
UI.Click(P("areasfull"))
Check(CV() == Value("parchment retro full") and Shows("parchment", "full", "retro"), "Full map's radio again: still full (a radio, not a switch)")
UI.Click(P("areasdiscovered"))
Check(CV() == Value("parchment retro discovered"), "Discovered's radio: back")
UI.Slide(P("nearbySlider"), 600)
Check(CV() == Value("parchment retro discovered", "nearby 600 thick 100 width own whole off effect heat colour class quality pulse raw") and
      SliderSays("nearby", 600, "600 yards"), "the zones nearby's slider to 600: \"600 yards\", the CVar's nearby 600")
UI.Slide(P("nearbySlider"), 0)
Check(string.find(CV(), " nearby 0 ", 1, true) and SliderSays("nearby", 0, "off"), "to its left end: off (nearby 0)")
UI.Slide(P("nearbySlider"), 9000)
Check(string.find(CV(), " nearby 4000 ", 1, true) and SliderSays("nearby", 4000, "4000 yards"), "past its right end: 4000 yards, the most")
UI.Slide(P("nearbySlider"), 1634)
Check(string.find(CV(), " nearby 1600 ", 1, true) and SliderSays("nearby", 1600, "1600 yards"), "between its steps: the nearest hundred yards (1634: 1600)")
UI.Click(P("barneo"))
Check(CV() == Value("parchment neo discovered") and Shows("parchment", "discovered", "neo") and Gold("bar", "neo") and not u.words[1]:IsVisible() and
      u.neoTitle:IsVisible() and P("QualityPulse"):IsVisible() and P("QualityRaw"):GetChecked() and P("QualityPulse"):GetChecked() and
      not P("QualityLiquid"):GetChecked() and not P("QualityUnstable"):GetChecked(), "Neo's radio: chosen; Retro's words give way to Neo's four qualities, Pulse and Raw ticked (today's)")
Check(At(P("QualityLiquid"), 40, 490, 26, 26) and At(P("QualityPulse"), 202, 490, 26, 26) and At(P("QualityUnstable"), 40, 516, 26, 26) and
      At(P("QualityRaw"), 202, 516, 26, 26), "the four in the bar's box, two by two, where Retro's words were")
UI.Click(P("QualityLiquid"))
Check(string.find(CV(), " quality liquid pulse raw$") and P("QualityLiquid"):GetChecked(), "Liquid ticked: quality liquid pulse raw")
UI.Click(P("QualityPulse"))
Check(string.find(CV(), " quality liquid raw$") and not P("QualityPulse"):GetChecked(), "Pulse unticked: quality liquid raw")
UI.Click(P("QualityLiquid")); UI.Click(P("QualityRaw"))
Check(string.find(CV(), " quality none$"), "none ticked: quality none")
UI.Click(P("QualityUnstable")); UI.Click(P("QualityPulse")); UI.Click(P("QualityRaw")); UI.Click(P("QualityLiquid"))
Check(string.find(CV(), " quality liquid pulse unstable raw$"), "all four: liquid pulse unstable raw, in the DLL's order whatever the clicks'")
UI.Click(P("QualityLiquid")); UI.Click(P("QualityUnstable"))
UI.Click(cards["bar retro"])
Check(CV() == Value("parchment retro discovered") and u.words[1]:IsVisible() and not P("QualityPulse"):IsVisible() and Gold("bar", "retro"),
      "Retro's picture: chosen, its words back, Neo's qualities kept as they were (pulse raw)")
UI.Slide(P("thickSlider"), 125)
Check(string.find(CV(), " thick 125 ", 1, true) and SliderSays("thick", 125, "125%"), "the thickness slider to 125: \"125%\", thick 125")
UI.Slide(P("thickSlider"), 10)
Check(string.find(CV(), " thick 50 ", 1, true) and SliderSays("thick", 50, "50%"), "to its left end: 50%, the least")
UI.Slide(P("thickSlider"), 999)
Check(string.find(CV(), " thick 200 ", 1, true) and SliderSays("thick", 200, "200%"), "to its right end: 200%, the most")
UI.Slide(P("thickSlider"), 127)
Check(string.find(CV(), " thick 125 ", 1, true), "between its steps: the nearest 5% (127: 125)")
UI.Slide(P("widthSlider"), 50)
Check(string.find(CV(), " width 50 ", 1, true) and SliderSays("width", 50, "50% of the screen"), "the width slider to 50: \"50% of the screen\", width 50")
UI.Slide(P("widthSlider"), 0)
Check(string.find(CV(), " width 20 ", 1, true) and SliderSays("width", 20, "20% of the screen"), "to its left end: 20%, the least")
UI.Slide(P("widthSlider"), 100)
Check(string.find(CV(), " width 100 ", 1, true), "to its right end: 100%")
UI.Slide(P("widthSlider"), 50)
UI.Click(P("Whole"))
Check(string.find(CV(), " width 50 whole on ", 1, true) and P("Whole"):GetChecked() and u.sliders.width.value.text == "the whole screen",
      "Whole width of the screen ticked: whole on, the width kept under it, the slider says the whole screen")
Check(Near(P("widthSlider"):GetValue(), 100) and P("widthSlider").alpha == 0.5 and not P("widthSlider"):IsEnabled(),
      "and the width slider's thumb at its end, the slider greyed and still while the whole width is ticked")
UI.Click(P("Whole"))
Check(string.find(CV(), " width 50 whole off ", 1, true) and not P("Whole"):GetChecked() and u.sliders.width.value.text == "50% of the screen",
      "unticked: whole off, back to its 50%")
Check(Near(P("widthSlider"):GetValue(), 50) and P("widthSlider").alpha == 1 and P("widthSlider"):IsEnabled(), "and the width slider live again, its thumb at 50%")
UI.Click(cards["effect smooth"])
Check(string.find(CV(), " effect smooth ", 1, true) and Gold("effect", "smooth"), "Smooth's picture: effect smooth, framed in gold")
UI.Click(cards["effect blade"])
Check(string.find(CV(), " effect blade ", 1, true) and Gold("effect", "blade"), "Blade light's: effect blade")
UI.Click(cards["effect heat"])
Check(string.find(CV(), " effect heat ", 1, true) and Gold("effect", "heat"), "Heat's: effect heat")
UI.Click(P("Colour"))
Check(string.find(CV(), " colour ember ", 1, true) and not P("Colour"):GetChecked(), "In my class's colour unticked: colour ember (CinderLoad's own)")
UI.Click(P("Colour"))
Check(string.find(CV(), " colour class ", 1, true) and P("Colour"):GetChecked(), "ticked again: colour class")
-- the preview: the bottom of the screen at 316 wide, the bar as the choice lays it
local pv = u.preview
local H = 316 * env.GetScreenHeight() / env.GetScreenWidth()
local function Bar() return PB(pv.bar[1]), PB(pv.bar[2]), PB(pv.bar[3]) end
local a1, a2, a3 = Bar()
Check(pv.ground.texture == "Interface\\CinderLoad\\PreviewParchment" and pv.bar[2].texture == "Interface\\CinderLoad\\PreviewRetro" and
      Near(a3.r - a1.l, 158) and Near(a1.b - a1.t, 42 / 720 * H * 1.25) and Near(a1.l - 406, 79) and Near(a2.l, a1.r) and Near(a3.l, a2.r),
      "the preview: the parchment's foot, Retro's bar 125% thick and half the strip wide, its caps kept and its middle stretched")
UI.Click(P("Whole"))
a1, a2, a3 = Bar()
Check(Near(a3.r - a1.l, 316) and Near(a1.l, 406), "whole width: the bar across the strip")
UI.Click(P("Whole"))
UI.Click(P("mapsatellite")); UI.Click(P("barneo"))
a1, a2, a3 = Bar()
Check(pv.ground.texture == "Interface\\CinderLoad\\PreviewSatellite" and pv.bar[2].texture == "Interface\\CinderLoad\\PreviewNeo" and
      Near(a1.b - a1.t, 0.066 * H * 1.25), "Satellite and Neo: the living map's foot and the blade's frame band (0.066 of the screen's height)")
-- the preview's fill: the chosen effect in the chosen colour (this player a mage), in the bar's channel to 60%, the rest dark
local MAGE, EMBER = { 0.412, 0.8, 0.941 }, { 1, 0.541, 0.165 }
local function Is3(t, c, k) k = k or 1; return t and Near(t[1], c[1] * k) and Near(t[2], c[2] * k) and Near(t[3], c[3] * k) end
local function Fill() return PB(pv.fill), PB(pv.rest), PB(pv.core) end
local f, r, c = Fill()
Check(pv.fill.texture == "Interface\\ChatFrame\\ChatFrameBackground" and pv.fill.gradient and pv.fill.gradient[1] == "HORIZONTAL" and
      Is3({ pv.fill.gradient[2], pv.fill.gradient[3], pv.fill.gradient[4] }, MAGE, 0.35) and Is3({ pv.fill.gradient[5], pv.fill.gradient[6], pv.fill.gradient[7] }, MAGE) and
      not pv.core:IsVisible() and Near(f.l, a1.l + (a1.r - a1.l) * 8 / 40) and Near(r.r, a3.r - (a3.r - a3.l) * 8 / 40) and
      Near(f.r - f.l, 0.6 * (r.r - f.l)) and Near(r.l, f.r) and Near(f.t, a1.t + (a1.b - a1.t) * 11 / 48) and Near(f.b - f.t, (a1.b - a1.t) * 34 / 48) and
      pv.rest.color and pv.rest.color[1] < 0.05,
      "the preview's fill, Heat in the class's colour: dark at its tail to the mage's colour at its tip, over Neo's blade inside its frame's rails (rows 11 to 45 of 48, 8 texels into each cap) to 60%, the rest dark")
UI.Click(cards["effect smooth"])
Check(Is3(pv.fill.vertex, MAGE) and not pv.fill.gradient and not pv.core:IsVisible(), "Smooth: the fill the class's colour, even")
UI.Click(cards["effect blade"])
f, r, c = Fill()
Check(Is3(pv.fill.vertex, MAGE) and pv.core:IsVisible() and pv.core.vertex[1] > 0.85 and pv.core.vertex[3] > 0.95 and Near(c.l, f.l) and Near(c.r, f.r) and
      c.t > f.t and c.b < f.b, "Blade light: the class's colour round a near-white core along the fill")
UI.Click(P("Colour"))
Check(Is3(pv.fill.vertex, EMBER) and Is3(pv.colour, EMBER), "In my class's colour unticked: the fill CinderLoad's ember")
UI.Click(P("Colour"))
Check(Is3(pv.fill.vertex, MAGE), "ticked again: the class's colour")
UI.Click(cards["bar retro"])
a1, a2, a3 = Bar()
f, r = Fill()
Check(Near(f.l, a1.l + (a1.r - a1.l) * 28 / 34) and Near(r.r, a3.r - (a3.r - a3.l) * 27 / 34) and Near(f.t, a1.t + (a1.b - a1.t) * 9 / 42) and
      Near(f.b - f.t, (a1.b - a1.t) * 25 / 42), "Retro: the fill in its channel (its art's texels 28 to 449 across, 9 to 34 down)")
UI.Click(cards["effect heat"])
UI.Click(P("Colour"))
-- Defaults (the class's colour unticked before it)
world.sounds = {}
UI.Click(P("Defaults"))
Check(CV() == Value("parchment retro discovered") and Shows("parchment", "discovered", "retro") and SliderSays("thick", 100, "100%") and
      SliderSays("width", 30, "each bar's own") and SliderSays("nearby", 1600, "1600 yards") and P("Colour"):GetChecked() and not P("Whole"):GetChecked() and
      Gold("effect", "heat") and NoErrors(), "Defaults: everything as it began (the parchment, Discovered, 1600 yards, Retro, 100%, each bar's own, Heat, the class's colour, Pulse and Raw)")
UI.Click(P("barneo"))
a1, a2, a3 = Bar()
Check(Near(a3.r - a1.l, 316) and Near(a1.l, 406) and SliderSays("width", 100, "each bar's own"),
      "Neo at its own width: the bar across the whole strip, the width slider's thumb at its end (Neo's own is the screen's)")
UI.Click(P("barretro"))
-- moved by its title
local l0, t0 = p:GetLeft(), p:GetTop()
Check(UI.Drag(P("Title"), 100, -50) and Near(p:GetLeft(), l0 + 100 / 0.82) and Near(p:GetTop(), t0 - 50 / 0.82) and NoErrors(),
      "dragged by its title: it moves with the mouse")
Check(not UI.Drag(p, 100, 0) and not UI.Drag(P("Preview"), 100, 0) and Near(p:GetLeft(), l0 + 100 / 0.82), "dragged anywhere else: it stays")
world.sounds = {}
UI.Click(P("CloseButton"))
Check(not p:IsShown() and world.sounds[1] == "igMainMenuClose", "Close: shut, with the game's close sound")
Slash("")
Check(p:IsShown() and Near(p:GetLeft(), l0 + 100 / 0.82), "/cinderload again: open where it was left")
UI.Click(P("Close"))
Check(not p:IsShown(), "the close X: shut")
Slash("")
p:Hide()                                                    -- (Escape: the game's CloseSpecialWindows hides each in UISpecialFrames)
Check(not p:IsShown() and NoErrors(), "Escape: shut")

-- ---- /cinderload's words, as before ----
Slash("")
Slash("map")
Check(CV() == Value("satellite retro discovered") and Shows("satellite", "discovered", "retro") and
      Said("CinderLoad: loading map Satellite, loading bar Retro, map areas Discovered, from your next loading screen."), "/cinderload map: switched; the open window follows")
Slash("bar")
Check(CV() == Value("satellite neo discovered") and Shows("satellite", "discovered", "neo"), "/cinderload bar: switched")
Slash("map")
Slash("bar")
Check(CV() == Value("parchment retro discovered"), "/cinderload map, bar: back")
Slash("map satellite")
Check(CV() == Value("satellite retro discovered"), "/cinderload map satellite: set")
Slash("map satellite")
Check(CV() == Value("satellite retro discovered"), "/cinderload map satellite again: still satellite (set, not switched)")
Slash("  BAR   Neo ")
Check(CV() == Value("satellite neo discovered"), "/cinderload BAR Neo (any case, any spaces): set")
Slash("parchment")
Check(CV() == Value("parchment neo discovered"), "/cinderload parchment (the value alone): set")
Slash("map parchment")
Slash("bar retro")
Check(CV() == Value("parchment retro discovered"), "/cinderload map parchment, /cinderload bar retro: set")
Slash("satellite")
Slash("neo")
Slash("retro")
Check(CV() == Value("satellite retro discovered"), "/cinderload satellite, neo, retro (each value alone): set")
Slash("areas")
Check(CV() == Value("satellite retro full") and Shows("satellite", "full", "retro") and
      Said("CinderLoad: loading map Satellite, loading bar Retro, map areas Full map, from your next loading screen."), "/cinderload areas: switched; the window follows")
Slash("areas full")
Check(CV() == Value("satellite retro full"), "/cinderload areas full: set")
Slash("discovered")
Check(CV() == Value("satellite retro discovered"), "/cinderload discovered (the value alone): set")
Slash("full")
Slash("Areas  Discovered")
Check(CV() == Value("satellite retro discovered"), "/cinderload Areas Discovered (any case, any spaces): set")
Slash("areas discovered")
Slash("map parchment")
local strict = true
for _, bad in ipairs({ "map=satellite", "map: satellite", "map, satellite", "map 2", "map satellite now", "bar blue", "zoom", "neo retro",
                       "map map", "help", "map=", "bar neo neo", "retro please", "areas all", "areas full full", "full map", "map full",
                       "areas=full", "discovered full", "thick 125", "open", "panel" }) do
    Slash(bad)
    local held = CV() == Value("parchment retro discovered") and env.CinderLoad.map == "parchment" and env.CinderLoad.bar == "retro"
    if not (held and env.CinderLoad.areas == "discovered" and Said("nothing changed:") and Said("/cinderload areas full") and
            Said("type /cinderload to open its window")) then strict = false; print("/cinderload " .. bad .. " was taken") end
end
Check(strict, "/cinderload strict: \"map=satellite\", \"map: satellite\", a word too many, a word not ours: nothing changed, how to say it (the window's way first)")
Check(MapUntouched() and Touched() == "" and SameFunctions(fns) and NoErrors(),
      "all along, nothing on the world map and nothing of the game's changed (" .. Touched() .. ")")
OpenMap()
Check(env.WorldMapFrame:IsVisible() and not env.CinderLoadMapPanel and MapUntouched() and NoErrors(), "the world map opened: the map's own, nothing of ours on it")
CloseMap()

-- ---- the look the DLL holds, at a login ----
Fresh({ cvar = "?" })
Run(setupLook)
Check(CV() == Value("parchment retro discovered", LOOK) and NoErrors(), "made with a look held (style.txt's): the CVar says it back word for word")
Slash("")
u = env.CinderLoad.ui
Check(SliderSays("nearby", 600, "600 yards") and SliderSays("thick", 150, "150%") and SliderSays("width", 100, "the whole screen") and
      env.CinderLoad.width == 60 and P("Whole"):GetChecked() and not P("Colour"):GetChecked() and Gold("effect", "blade") and NoErrors(),
      "the window shows it: 600 yards, 150%, the whole screen (its thumb at the end, 60% kept under it), Blade light, not the class's colour")
UI.Click(P("barneo"))
Check(P("QualityLiquid"):GetChecked() and P("QualityUnstable"):GetChecked() and not P("QualityPulse"):GetChecked() and not P("QualityRaw"):GetChecked(),
      "and Neo's qualities held: Liquid and Unstable")
-- a look set by hand in style.txt, off the sliders' steps: said back exactly at a login (no click), so style.txt is never
-- rewritten by a login; only the slider a player moves puts its own value on a step
for _, odd in ipairs({ "nearby 1250 thick 123 width 33 whole off effect heat colour class quality pulse raw",
                       "nearby 40 thick 51 width 99 whole off effect smooth colour class quality raw" }) do
    Fresh({ cvar = "?" })
    Run((string.gsub(setupSrc, 'local CL_LOOK = "[^"]*"', 'local CL_LOOK = "' .. odd .. '"', 1)))
    local said = CV()
    Slash("")
    u = env.CinderLoad.ui
    local _, _, yd = string.find(odd, "^nearby (%d+)")
    local held = CV() == said and SliderSays("nearby", tonumber(yd), yd .. " yards")
    UI.Slide(P("thickSlider"), 127)
    Check(said == Value("parchment retro discovered", odd) and held and
          CV() == Value("parchment retro discovered", (string.gsub(odd, "thick %d+", "thick 125"))) and NoErrors(),
          "a look set by hand off the sliders' steps (" .. odd .. "): said back word for word at a login and as the window opens; the thickness slider moved puts only itself on a step")
end
Fresh({ cvar = "?" })
Run(setupLook)
Slash("")
u = env.CinderLoad.ui
env.CinderLoad.ReadLook("thick 9999 width 3 nearby -5 whole maybe effect fire colour red quality raw pulse banana thick")
Check(env.CinderLoad.thick == 200 and env.CinderLoad.width == 20 and env.CinderLoad.nearby == 0 and env.CinderLoad.whole and env.CinderLoad.effect == "blade" and
      env.CinderLoad.ember and env.CinderLoad.quality.raw and env.CinderLoad.quality.pulse and not env.CinderLoad.quality.liquid,
      "a look read with numbers out of range held to them, words not ours passed over (kept as they were)")

-- ---- the pack older than the window: dark plates where its pictures go ----
Fresh({ cvar = "?", noPack = true })
Run(setupSrc)
Slash("")
u = env.CinderLoad.ui
Check(P():IsVisible() and u.preview.ground.color and not u.preview.ground.texture and u.cards[1].picture.color and NoErrors(),
      "the pictures not in the client (an older pack): dark plates in their place, everything else as ever")

-- ---- a fault: off, the game's UI as ever ----
Fresh({ cvar = "?" })
Run(setupSrc)
Slash("")
local set = env.SetCVar
local refused = nil
env.SetCVar = function(n, v)                                             -- (a fault in ours: its first word refused)
    if not refused and string.find(tostring(v), "satellite", 1, true) then refused = 1; error("refused") end
    return set(n, v)
end
world.errors = {}
UI.Click(P("mapsatellite"))
env.SetCVar = set
Check(env.CinderLoad.broken and not P():IsShown() and CV() == Value("satellite retro discovered") .. " fault" and Said("turned itself off") and
      Said("refused") and NoErrors(), "a fault in ours: off and hidden, the CVar says \"fault\", the chat says where; no error left to the game")
Slash("")
Check(not P():IsShown() and Said("off until the game starts again"), "after a fault: /cinderload says it is off, and opens nothing")
UI.Run(P("mapparchment"), "OnClick")
Check(string.find(CV(), " fault$") and NoErrors(), "after a fault: a control clicked anyway (by something else) does nothing")
Fresh({ cvar = "?" })
Run(setupSrc)
Slash("")
env.CinderLoad.ui.preview.bar[1].SetTexCoord = function() error("the art broke") end
UI.Slide(P("thickSlider"), 150)
Check(env.CinderLoad.broken and not P():IsShown() and string.find(CV(), " fault$") and Said("the art broke") and NoErrors(),
      "a fault while the window redraws: caught, off, said; no error left to the game")

-- ---- made again in the same UI; a /reload ----
Fresh({ cvar = "?" })
Run(setupSrc)
Slash("")
local creates, p1 = world.creates, P()
Run(setupSrc)
Slash("")
Check(world.creates == creates and P() == p1 and P():IsShown() and CV() == Value("parchment retro discovered") and NoErrors(),
      "made again in the same UI: the same window, nothing made twice, the choice the DLL sent")
Fresh({ cvar = "?" })
Run(setupSN)
Slash("")
Check(CV() == Value("satellite neo discovered") and Shows("satellite", "discovered", "neo") and NoErrors(), "a /reload after Satellite and Neo were chosen: the window shows them")
Fresh({ cvar = "?" })
Run(setupFull)
Slash("")
Check(CV() == Value("parchment retro full") and Shows("parchment", "full", "retro") and NoErrors(), "a /reload after the full map was chosen: shows it")

-- ---- without the world map at all; without the CVar ----
Fresh({ cvar = "?", noMap = true })
ok, err = Run(setupSrc)
Slash("")
Check(ok and CV() == Value("parchment retro discovered") and P() and P():IsShown() and NoErrors(), "no world map frame (an add-on took it away): the window works the same")
Fresh({ cvar = false })
ok, err = Run(setupSrc)
Check(ok and env.CinderLoad.broken and Said("turned itself off"), "no CVar: the chunk never raises; off, and the chat says why")

-- for the picture: the window's frame tree as the harness holds it (tools/render_panel.py draws it with the client's
-- art): as the mockup shows it (600 yards, 125%, 50%), and (neolayout=) with the satellite and Neo picked
local function Dump(path)
    local f = io.open(path, "w")
    local function Q(v) if v == nil then return "" end; return string.gsub(tostring(v), "[\t\n]", " ") end
    local function L(t) if not t then return "" end; local o = {}; for i = 1, table.getn(t) do o[i] = Q(t[i]) end; return table.concat(o, " ") end
    for _, r in ipairs(world.made) do r.id0 = nil end
    for i, r in ipairs(world.made) do
        local l, b, rr, t = UI.Rect(r)
        local bdp = r.backdrop
        f:write(table.concat({ Q(i), Q(r.kind), Q(r.name), Q(r.parent and r.parent.id0), Q(r.role), Q(r.layer), Q(r.strata), Q(r.level),
                               r:IsVisible() and "1" or "0", Q(l), Q(b), Q(rr), Q(t), Q(r.texture), L(r.texcoord), L(r.color), Q(r.blend),
                               Q(r.text), Q(r.fontFile or (r.kind == "FontString" and "Fonts\\FRIZQT__.TTF" or nil)), Q(r.size), Q(r.justifyH),
                               r.checked and "1" or "0", bdp and Q(bdp.bgFile) or "", bdp and Q(bdp.edgeFile) or "", bdp and Q(bdp.edgeSize) or "",
                               bdp and bdp.insets and L({ bdp.insets.left, bdp.insets.right, bdp.insets.top, bdp.insets.bottom }) or "",
                               L(r.backdropColor), L(r.backdropBorderColor), Q(r.min), Q(r.max), Q(r.value), Q(r:GetEffectiveScale()),
                               Q(table.getn(r.points or {}) == 1 and r.points[1].p or ""),
                               r.gradient and L(r.gradient) or L(r.vertex) }, "\t") .. "\n")
        r.id0 = i
    end
    f:close()
end
if opt.layout then
    Fresh({ cvar = "?" })
    Run(setupSrc)
    Slash("")
    UI.Slide(P("nearbySlider"), 600)
    UI.Slide(P("thickSlider"), 125)
    UI.Slide(P("widthSlider"), 50)
    Dump(opt.layout)
    if opt.neolayout then
        UI.Click(P("mapsatellite"))
        UI.Click(P("barneo"))
        UI.Click(P("QualityLiquid"))
        Dump(opt.neolayout)
    end
end

print(string.format("done %d %d", checks, fails))
