-- The world map's two drop-downs (dll/switches.lua) and the DLL's probe, run as the DLL sends them (test_switch.c writes
-- them out) against a stubbed 1.12 UI (tests/wow_ui.lua): Lua 5.1 with 5.0's limits put on (no string methods, only
-- 5.0's library; the source checked for 5.1-only syntax). With the client's own UIDropDownMenu.lua and WorldMapFrame.lua
-- (framexml=<dir>, read out of a client's archives by test_switch.c), the drop-downs open, pick and share their list
-- through the client's own code, and the map is minimised and maximised by it.
--   lua5.1 tests/test_lua.lua <dir with probe.lua, setup.lua, setup-satellite-neo.lua> [layout=<out>] [widths=<in>]
--          [framexml=<dir>]
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
local setupSN = ReadFile(dir .. "/setup-satellite-neo.lua")
Check(probeSrc and setupSrc and setupSN, "the DLL's probe and its switches' chunk written out")
if not (probeSrc and setupSrc and setupSN) then print("done"); os.exit(1) end

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
Check(string.find(setupSrc, '^local CL_MAP, CL_BAR = "parchment", "retro"\n') ~= nil and
      string.find(setupSN, '^local CL_MAP, CL_BAR = "satellite", "neo"\n') ~= nil, "the chunk begins with the choice the DLL holds")

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
local function D(key) return env["CinderLoadMap" .. key .. "DropDown"] end
local function Arrow(key) return env["CinderLoadMap" .. key .. "DropDownButton"] end
local function Said(s)
    for _, t in ipairs(world.chat) do if string.find(t.text, s, 1, true) then return t end end
end
local function Slash(msg) world.chat = {}; env.SlashCmdList.CINDERLOAD(msg) end
local function OpenMap() if not env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end; UI.Frame() end
local function CloseMap() if env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end end
local function List()                         -- the shared list as shown: its buttons' words, values and checks
    local l, out = env.DropDownList1, {}
    if not l:IsShown() then return nil end
    for j = 1, l.numButtons do
        local b = env["DropDownList1Button" .. j]
        table.insert(out, { text = b:GetText(), value = b.value, checked = env[b:GetName() .. "Check"]:IsShown() and 1 or nil,
                            button = b, shown = b:IsShown() })
    end
    return out
end
local function ListSays(a, b)                 -- two buttons, these words, the check on the one with a "*"
    local l = List()
    if not l or table.getn(l) ~= 2 then return false end
    for i, want in ipairs({ a, b }) do
        local _, _, name, star = string.find(want, "^(.-)(%*?)$")
        if l[i].text ~= name or not l[i].shown or (star == "*") ~= (l[i].checked == 1) then return false end
    end
    return true
end
local function PickInList(text)
    local l = List()
    if not l then return false end
    for _, b in ipairs(l) do if b.text == text then return UI.Click(b.button) end end
    return false
end
local function NoErrors() return table.getn(world.errors) == 0 end
local function Near(a, b) return a and b and math.abs(a - b) < 0.01 end
local function Boxed(r) local l, t, rr, b = UI.Box(r); return { l = l, t = t, r = rr, b = b } end
local BLIZZ = { "ToggleDropDownMenu", "UIDropDownMenu_Initialize", "UIDropDownMenu_AddButton", "UIDropDownMenu_SetSelectedValue",
                "UIDropDownMenu_SetWidth", "UIDropDownMenu_SetText", "UIDropDownMenu_Refresh", "UIDropDownMenuButton_OnClick",
                "CloseDropDownMenus", "ToggleWorldMap", "WorldMapFrame_Minimize", "WorldMapFrame_Maximize", "WorldMapFrame_Update",
                "WorldMapContinentsDropDown_OnLoad", "WorldMapZoneDropDown_OnLoad", "WorldMapContinentButton_OnClick" }
local function Functions() local t = {}; for _, n in ipairs(BLIZZ) do t[n] = env[n] end; return t end
local function SameFunctions(t) for _, n in ipairs(BLIZZ) do if env[n] ~= t[n] then return false end end; return true end
local function Scripts()
    local t = {}
    for _, f in ipairs({ env.WorldMapFrame, env.WorldMapContinentDropDown, env.WorldMapContinentDropDownButton, env.WorldMapZoneDropDown,
                         env.WorldMapZoneDropDownButton, env.DropDownList1, env.DropDownList1Button1 }) do
        for h, fn in pairs(f.scripts) do t[f.name .. "." .. h] = fn end
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
local function OnlyScale()                    -- every touch of ours the shared list's scale, once a list of ours opened
    for _, t in ipairs(world.touched) do if t ~= "DropDownList1:SetScale" then return false end end
    return true
end
print("the drop-downs' code: " .. (real and "the client's own UIDropDownMenu.lua and WorldMapFrame.lua" or "this test's stand-ins"))

-- ---- the probe ----
Fresh({ cvar = false })
local ok, err = Run(probeSrc, "probe")
Check(ok, "the probe never raises, even before the CVar is there (" .. tostring(err) .. ")")
Fresh()
Run(probeSrc, "probe")
Check(world.cvars.CinderLoadStyle == "?", "the probe: the switches not there (a login, a /reload), the CVar marked \"?\"")
Fresh({ noMap = true })
Run(probeSrc, "probe")
Check(world.cvars.CinderLoadStyle == "parchment retro", "the probe: no world map frame (not in the world's UI), nothing marked")

-- ---- made, at a login (the map closed) ----
Fresh({ cvar = "?" })
local fns, scripts = Functions(), Scripts()
ok, err = Run(setupSrc)
Check(ok and NoErrors(), "the switches' chunk runs whole (" .. tostring(err) .. table.concat(world.errors, "; ") .. ")")
Check(world.cvars.CinderLoadStyle == "parchment retro", "made: the CVar told the choice held (the DLL hears that they ran)")
Check(type(env.CinderLoad) == "table" and env.CinderLoad.up == 1 and not env.CinderLoad.broken, "made: CinderLoad.up, not broken")
local panel = env.CinderLoadMapPanel
Check(panel and panel.parent == env.WorldMapFrame and panel.kind == "Frame", "a frame of its own on the world map's (shown with it)")
local m, b = D("map"), D("bar")
Check(m and b and m.parent == panel and b.parent == panel and env[m.name .. "Left"] and env[m.name .. "Middle"] and env[m.name .. "Right"] and
      env[m.name .. "Text"] and Arrow("map") and Arrow("bar"), "two drop-downs made from the client's own UIDropDownMenuTemplate")
local ml, bl = env[m.name .. "Label"], env[b.name .. "Label"]
Check(ml and bl and ml.text == "Loading map" and bl.text == "Loading bar" and ml.font == "GameFontNormalSmall" and bl.font == "GameFontNormalSmall" and
      ml.layer == "OVERLAY" and ml.points[1].p == "TOPLEFT" and ml.points[1].rel == m and ml.points[1].x == 20 and ml.points[1].y == 10,
      "his labels, \"Loading map\" and \"Loading bar\", in gold over each (GameFontNormalSmall at the template's top left, 20, 10: as the map's \"Continent\" and \"Zone\")")
Check(env[m.name .. "Middle"].w == 92 and m.w == 142 and env[m.name .. "Text"].w == 67 and b.w == 142 and m.noResize == 1,
      "each 92 wide inside, as UIDropDownMenu_SetWidth makes it (its frame 142, its words 67)")
Check(env[m.name .. "Text"].text == "Parchment" and env[b.name .. "Text"].text == "Retro" and m.selectedValue == "parchment" and
      b.selectedValue == "retro", "the choice held shows: Parchment, Retro")
Check(env.SLASH_CINDERLOAD1 == "/cinderload" and type(env.SlashCmdList.CINDERLOAD) == "function", "/cinderload made")
Check(table.getn(world.chat) == 0, "made quietly (nothing said in the chat)")
Check(Touched() == "" and SameFunctions(fns) and SameScripts(scripts),
      "no frame of the game's own changed by it, no Blizzard function replaced or hooked, no script of the game's set (" .. Touched() .. ")")

-- ---- the map opened: where they are ----
OpenMap()
Check(m:IsVisible() and b:IsVisible() and env.WorldMapContinentDropDown:IsVisible() and NoErrors(), "the map opened: both shown, as Continent and Zone")
local mb, bb, cb, zb = Boxed(m), Boxed(b), Boxed(env.WorldMapContinentDropDown), Boxed(env.WorldMapZoneDropDown)
local cl = Boxed(env.WorldMapContinentDropDown.label)
local mlb, blb = Boxed(ml), Boxed(bl)
Check(Near(mb.l, 32) and Near(mb.t, 35) and Near(bb.l, 141) and Near(bb.r, 283) and Near(mb.t, cb.t) and Near(bb.t, cb.t) and Near(mb.b, cb.b),
      "on the top bar, on Continent's row, from x 32 (clear of the bar's flourish) to 283 of the map's 1024: the mock's place")
Check(bb.r < cb.l - 50 and blb.r < cl.l and mlb.r < blb.l and Near(Boxed(env[m.name .. "Left"]).t, Boxed(env.WorldMapContinentDropDownLeft).t),
      "covering nothing: the second ends 59 short of Continent, the labels apart, the art at Continent's height")
if opt.layout then              -- for the look: each region of ours and the map's top row, in the map's box
    local f = io.open(opt.layout, "w")
    local function Out(state, r)
        local l, t, rr, bt = UI.Box(r)
        if not l or not r:IsVisible() then return end
        local tc = r.texcoord and table.concat(r.texcoord, " ") or ""
        f:write(string.format("%s\t%s\t%s\t%.3f %.3f %.3f %.3f\t%s\t%s\t%s\t%s\t%s\n", state, r.name or "?", r.kind, l, t, rr, bt,
                              r.texture or r.font or "", tc, r.text or "", r.justifyH or "", r.checked and "1" or ""))
    end
    local function All(state)
        for _, d in ipairs({ m, b, env.WorldMapContinentDropDown, env.WorldMapZoneDropDown }) do
            for _, k in ipairs({ "Left", "Middle", "Right", "Text", "ButtonNormalTexture" }) do Out(state, env[d.name .. k]) end
            Out(state, d.label or env[d.name .. "Label"])
        end
        if env.DropDownList1:IsShown() then
            Out(state, env.DropDownList1)
            for j = 1, env.DropDownList1.numButtons do
                local bt = env["DropDownList1Button" .. j]
                Out(state, bt)
                Out(state, env[bt.name .. "Check"])
                Out(state, env[bt.name .. "NormalText"])
                Out(state, env[bt.name .. "Highlight"])
            end
        end
    end
    All("closed")
    UI.Click(Arrow("map"))
    UI.Enter(env.DropDownList1Button2)
    All("open")
    UI.Leave(env.DropDownList1Button2)
    UI.Click(Arrow("map"))
    f:close()
    world.sounds = {}
    world.touched = {}
end

-- ---- its list: opened, picked ----
UI.Click(Arrow("map"))
local list = env.DropDownList1
local lb = Boxed(list)
Check(list:IsShown() and env.UIDROPDOWNMENU_OPEN_MENU == m.name and ListSays("Parchment*", "Satellite") and NoErrors(),
      "its arrow clicked: the shared list opens with its two choices, Parchment checked (the client's own ToggleDropDownMenu)")
Check(list:GetScale() == 1 and Near(lb.l, 40) and Near(lb.t, Boxed(env[m.name .. "Left"]).b - 22) and world.sounds[1] == "igMainMenuOptionCheckBoxOn",
      "the list at the map's own size under it, where the client puts Continent's (its UI scale of 0.82 not put on it), with the arrow's sound")
Check(Touched() == "DropDownList1:SetScale", "opening it, nothing of the game's changed but that list's scale (set again as any list opens): " .. Touched())
UI.Enter(env.DropDownList1Button2)
local tip = env.GameTooltip
Check(tip.shown and tip.lines[1] == "Satellite" and string.find(tip.lines[2] or "", "From your next loading screen.", 1, true),
      "a choice pointed at (beginner's tips on): the game's tooltip says what it is and when it counts")
UI.Leave(env.DropDownList1Button2)
world.sounds = {}
PickInList("Satellite")
Check(world.cvars.CinderLoadStyle == "satellite retro" and not list:IsShown() and env[m.name .. "Text"].text == "Satellite" and
      m.selectedValue == "satellite" and env[b.name .. "Text"].text == "Retro" and world.sounds[1] == "UChatScrollButton" and NoErrors(),
      "Satellite picked: the CVar says so, the list closes, the drop-down says Satellite, the bar as it was, the list's sound")
UI.Click(Arrow("map"))
Check(ListSays("Parchment", "Satellite*"), "opened again: Satellite checked")
UI.Click(Arrow("map"))
Check(not list:IsShown(), "its arrow again: the list closes")
UI.Click(Arrow("bar"))
Check(ListSays("Retro*", "Neo") and env.UIDROPDOWNMENU_OPEN_MENU == b.name, "the bar's: Retro, Neo")
PickInList("Neo")
Check(world.cvars.CinderLoadStyle == "satellite neo" and env[b.name .. "Text"].text == "Neo" and env[m.name .. "Text"].text == "Satellite",
      "Neo picked: the bar on its own, the map as it was")

-- ---- the shared list: Continent and Zone as ever ----
world.touched = {}
local cArrow, zArrow = env.WorldMapContinentDropDownButton, env.WorldMapZoneDropDownButton
UI.Click(cArrow)
local before = List()
Check(before and table.getn(before) == 2 and before[1].text == "Kalimdor" and before[2].text == "Eastern Kingdoms" and list:GetScale() == 1 and
      env.UIDROPDOWNMENU_OPEN_MENU == "WorldMapContinentDropDown" and Near(Boxed(list).l, cb.l + 8) and NoErrors(),
      "after ours, Continent's list: its continents, at its place and size")
PickInList("Kalimdor")
Check(env.UIDropDownMenu_GetSelectedID(env.WorldMapContinentDropDown) == 1 and world.map.c == 1 and not list:IsShown(), "Kalimdor picked from it: the map's own")
UI.Click(Arrow("map"))
UI.Click(cArrow)
Check(ListSays("Kalimdor*", "Eastern Kingdoms") and env.UIDROPDOWNMENU_OPEN_MENU == "WorldMapContinentDropDown",
      "ours opened, then Continent's straight after: its own two, its own check")
local snap = {}
for j = 1, 2 do
    local bt = env["DropDownList1Button" .. j]
    snap[j] = { bt:GetText(), env[bt.name .. "Check"]:IsShown(), bt.lockedHighlight, bt.w }
end
Slash("map parchment")
local same = list:IsShown()
for j = 1, 2 do
    local bt = env["DropDownList1Button" .. j]
    local now = { bt:GetText(), env[bt.name .. "Check"]:IsShown(), bt.lockedHighlight, bt.w }
    for k = 1, 4 do if now[k] ~= snap[j][k] then same = false end end
end
Check(same and env[m.name .. "Text"].text == "Parchment" and world.cvars.CinderLoadStyle == "parchment neo",
      "a choice made with /cinderload while Continent's list is open: ours shows it, that list untouched (its words, check, highlight, width)")
UI.Click(cArrow)
UI.Click(zArrow)
Check(ListSays("Ashenvale", "Darkshore") and env.UIDROPDOWNMENU_OPEN_MENU == "WorldMapZoneDropDown" and list:GetScale() == 1,
      "and Zone's after them: Kalimdor's zones")
UI.Click(Arrow("bar"))
Check(ListSays("Retro", "Neo*") and env.UIDROPDOWNMENU_OPEN_MENU == b.name, "ours over Zone's open list: ours")
Check(OnlyScale() and SameFunctions(fns) and SameScripts(scripts) and NoErrors(),
      "all along, nothing of the game's changed but the list's scale; no function or script of the game's replaced (" .. Touched() .. ")")
CloseMap()
Check(not list:IsShown(), "the map closed with our list open: the list closes with it")

-- ---- the windowed map (this client's minimised one), in the client's own order ----
OpenMap()
env.WorldMapFrame_Minimize()
Check(env.WORLDMAP_WINDOWED == 1 and env.WorldMapFrame:IsVisible() and env.WorldMapContinentDropDown:GetAlpha() == 0 and
      not m:IsShown() and not b:IsShown() and NoErrors(),
      "the open map minimised (shown again before WORLDMAP_WINDOWED is set): ours hidden with Continent and Zone, never too big in the window")
UI.Frame()
Check(not m:IsShown() and panel.scripts.OnUpdate == nil, "its first frame: still hidden, and nothing left running each frame")
CloseMap()
OpenMap()
Check(not m:IsShown() and not b:IsShown(), "the windowed map opened again: hidden")
Slash("bar")
Check(world.cvars.CinderLoadStyle == "parchment retro" and Said("loading bar Retro"), "/cinderload with the windowed map: still there")
env.WorldMapFrame_Maximize()
local mb2 = Boxed(m)
Check(env.WORLDMAP_WINDOWED == 0 and m:IsVisible() and b:IsVisible() and Near(mb2.l, 32) and Near(mb2.t, 35) and
      env[b.name .. "Text"].text == "Retro" and NoErrors(), "maximised again: shown in their place, the choice made meanwhile showing")
CloseMap()
env.WorldMapFrame_Minimize()                                             -- (closed: the client's way at login when saved so)
OpenMap()
Check(not m:IsShown(), "minimised while closed, then opened: hidden")
env.WorldMapFrame_Maximize()
Check(m:IsVisible(), "and maximised: shown")
env.WorldMapContinentDropDown:SetAlpha(0)                                 -- (an add-on hiding the map's top row after it shows)
UI.Frame()
Check(not m:IsShown(), "the map's top row hidden by something after it showed: ours too, on the next frame")
env.WorldMapContinentDropDown:SetAlpha(1)
CloseMap()
OpenMap()
Check(m:IsShown(), "and back when it is back")

-- ---- a map made a movable window by an add-on (ShaguTweaks' WorldMap Window, pfUI's map) ----
env.WorldMapFrame:SetScale(0.85)
env.WorldMapFrame:ClearAllPoints()
env.WorldMapFrame:SetPoint("CENTER", env.UIParent, "CENTER", 0, 30)
env.WorldMapFrame:SetWidth(1017)
env.WorldMapFrame:SetHeight(723)
CloseMap()
OpenMap()
local mb3 = Boxed(m)
UI.Click(Arrow("map"))
Check(m:IsVisible() and Near(mb3.l, 32) and Near(mb3.t, 35) and Near(m:GetEffectiveScale(), 0.85) and Near(list:GetScale(), 0.85) and
      ListSays("Parchment*", "Satellite") and NoErrors(), "the map a window at 0.85: ours in the same place on it, at its size, the list too")
UI.Click(Arrow("map"))
env.WorldMapFrame:SetScale(1)
env.WorldMapFrame:SetAllPoints()

-- ---- /cinderload ----
Slash("")
local said = world.chat[1]
Check(said and said.text == "CinderLoad: loading map Parchment, loading bar Retro. Type /cinderload map or /cinderload bar to switch." and
      said.r == 1 and said.g == 1 and said.b == 0, "/cinderload: the choice, in the system's yellow, as the mock has it")
Slash("map")
Check(world.cvars.CinderLoadStyle == "satellite retro" and env[m.name .. "Text"].text == "Satellite" and
      Said("CinderLoad: loading map Satellite, loading bar Retro, from your next loading screen."), "/cinderload map: switched; the drop-down follows")
Slash("bar")
Check(world.cvars.CinderLoadStyle == "satellite neo", "/cinderload bar: switched")
Slash("map")
Slash("bar")
Check(world.cvars.CinderLoadStyle == "parchment retro", "/cinderload map, bar: back")
Slash("map satellite")
Check(world.cvars.CinderLoadStyle == "satellite retro", "/cinderload map satellite: set")
Slash("map satellite")
Check(world.cvars.CinderLoadStyle == "satellite retro", "/cinderload map satellite again: still satellite (set, not switched)")
Slash("  BAR   Neo ")
Check(world.cvars.CinderLoadStyle == "satellite neo", "/cinderload BAR Neo (any case, any spaces): set")
Slash("parchment")
Check(world.cvars.CinderLoadStyle == "parchment neo", "/cinderload parchment (the value alone): set")
local strict = true
for _, bad in ipairs({ "map=satellite", "map: satellite", "map, satellite", "map 2", "map satellite now", "bar blue", "zoom", "neo retro",
                       "map map", "help", "map=", "bar neo neo", "retro please" }) do
    Slash(bad)
    local held = world.cvars.CinderLoadStyle == "parchment neo" and env.CinderLoad.map == "parchment" and env.CinderLoad.bar == "neo"
    if not (held and Said("nothing changed:") and Said("/cinderload bar neo")) then strict = false; print("/cinderload " .. bad .. " was taken") end
end
Check(strict, "/cinderload strict: \"map=satellite\", \"map: satellite\", \"map, satellite\", \"map 2\", a word too many, a word not ours: nothing changed, how to say it")

-- ---- a fault: off, the map and its drop-downs as ever ----
OpenMap()
local add = env.UIDropDownMenu_AddButton
env.UIDropDownMenu_AddButton = function(info, level)                     -- (a fault in ours: our menu's button refused)
    if env.UIDROPDOWNMENU_INIT_MENU == m.name then error("refused") end
    return add(info, level)
end
world.errors = {}
UI.Click(Arrow("map"))
env.UIDropDownMenu_AddButton = add
Check(env.CinderLoad.broken and not panel:IsShown() and not m:IsVisible() and world.cvars.CinderLoadStyle == "parchment neo fault" and
      Said("turned themselves off") and Said("refused") and NoErrors(),
      "a fault in ours: off and hidden, the CVar says \"fault\", the chat says where; no error left to the game")
UI.Run(Arrow("map"), "OnClick")                                       -- (clicked by something else while hidden: Button:Click())
Check(not list:IsShown() and NoErrors(), "after a fault: its arrow clicked anyway (hidden, by something else) does nothing")
Slash("map")
Check(world.cvars.CinderLoadStyle == "parchment neo fault" and Said("off until the game starts again"), "after a fault: /cinderload says it is off")
UI.Click(cArrow)
Check(ListSays("Kalimdor*", "Eastern Kingdoms") and PickInList("Eastern Kingdoms") and world.map.c == 2 and NoErrors(),
      "after a fault: Continent's drop-down as ever")
CloseMap()

-- a fault in the client's own drop-down code while ours opens: caught as ours, off, never left to the game
Fresh({ cvar = "?" })
Run(setupSrc)
OpenMap()
local toggle = env.ToggleDropDownMenu
env.ToggleDropDownMenu = function(level, value, frame, a, x, y)
    if frame == D("map") then error("the list broke") end
    return toggle(level, value, frame, a, x, y)
end
UI.Click(Arrow("map"))
env.ToggleDropDownMenu = toggle
Check(env.CinderLoad.broken and not D("map"):IsVisible() and world.cvars.CinderLoadStyle == "parchment retro fault" and Said("the list broke") and
      NoErrors(), "a fault in the client's drop-down code as ours opens: caught, off, said; no error left to the game")
CloseMap()

-- ---- made again in the same UI; a /reload; made while a list is open ----
Fresh({ cvar = "?" })
Run(setupSrc)
local creates, m1, p1 = world.creates, D("map"), env.CinderLoadMapPanel
Run(setupSrc)
OpenMap()
Check(world.creates == creates and table.getn(env.CinderLoad.drops) == 2 and env.CinderLoadMapPanel == p1 and m1 == D("map") and
      world.cvars.CinderLoadStyle == "parchment retro" and D("map"):IsVisible() and NoErrors(),
      "made again in the same UI: the same frames, two drop-downs, the choice the DLL sent")
Fresh({ cvar = "?" })
Run(setupSN)
OpenMap()
Check(world.cvars.CinderLoadStyle == "satellite neo" and env[D("map").name .. "Text"].text == "Satellite" and env[D("bar").name .. "Text"].text == "Neo",
      "a /reload after satellite and neo were chosen: made showing them")
Fresh({ cvar = "?" })
OpenMap()
UI.Click(env.WorldMapContinentDropDownButton)
Run(setupSrc)
Check(ListSays("Kalimdor", "Eastern Kingdoms") and NoErrors(), "made while Continent's list is open: that list left whole")
UI.Click(env.WorldMapContinentDropDownButton)
UI.Click(Arrow("map"))
Check(ListSays("Parchment*", "Satellite"), "and ours opens after it")

-- ---- without the world map's frame, or its guide; without the CVar ----
Fresh({ cvar = "?", noMap = true })
ok, err = Run(setupSrc)
Check(ok and world.cvars.CinderLoadStyle == "parchment retro" and not env.CinderLoadMapPanel and type(env.SlashCmdList.CINDERLOAD) == "function",
      "no world map frame (an add-on took it away): no drop-downs, /cinderload still")
Fresh({ cvar = "?", noGuide = true })
Run(setupSrc)
Check(env.CinderLoadMapPanel.points[1].rel == env.WorldMapFrame and NoErrors(), "no positioning guide: at the map frame's own top left")
Fresh({ cvar = false })
ok, err = Run(setupSrc)
Check(ok and env.CinderLoad.broken and Said("turned themselves off"), "no CVar: the chunk never raises; off, and the chat says why")

-- for the look: what /cinderload says, then /cinderload map (a fresh session, the defaults)
if opt.layout then
    Fresh({ cvar = "?" })
    Run(setupSrc)
    local f = io.open(opt.layout, "a")
    for _, msg in ipairs({ "", "map" }) do
        Slash(msg)
        for _, t in ipairs(world.chat) do
            f:write("chat\t/cinderload" .. (msg ~= "" and " " .. msg or "") .. "\t" .. t.text .. "\t" .. t.r .. " " .. t.g .. " " .. t.b .. "\n")
        end
    end
    f:close()
end

print(string.format("done %d %d", checks, fails))
