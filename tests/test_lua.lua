-- The world map's switches (dll/switches.lua) and the DLL's probe, run as the DLL sends them (test_switch.c writes them
-- out) against a stubbed 1.12 UI: Lua 5.1 or LuaJIT with 5.0's limits put on (no string methods, only 5.0's library;
-- the source checked for 5.1-only syntax).  lua5.1 tests/test_lua.lua <dir with probe.lua, setup.lua> [<layout out>
-- [<string widths>]]. One line a check: "ok <what>" or "FAIL <what>"; test_switch.c counts them.
local dir, layoutOut, widthsIn = arg[1], arg[2], arg[3]
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
                         "require", "load", "rawlen", "rawequal" }) do
        if string.find(s, "%f[%w_]" .. w .. "%f[^%w_]") then return "5.1's " .. w end
    end
    return nil
end
Check(Lex50(setupSrc) == nil, "the switches' chunk: Lua 5.0's syntax only (" .. tostring(Lex50(setupSrc)) .. ")")
Check(Lex50(probeSrc) == nil, "the probe: Lua 5.0's syntax only (" .. tostring(Lex50(probeSrc)) .. ")")
Check(string.find(setupSrc, '^local CL_MAP, CL_BAR = "parchment", "retro"\n') ~= nil and
      string.find(setupSN, '^local CL_MAP, CL_BAR = "satellite", "neo"\n') ~= nil, "the chunk begins with the choice the DLL holds")

-- string widths, as the client's font gives them (the mock's measure, else about 5.5 a letter at 10)
local widths = {}
if widthsIn then
    local w = ReadFile(widthsIn)
    if w then
        for t, n in string.gmatch(w, "([^\t\n]+)\t([%d%.]+)\n") do widths[t] = tonumber(n) end
    end
end

-- no string methods (5.0 has no metatable on strings)
local smeta = getmetatable("")
smeta.__index = nil

-- ---- the stubbed UI ----
local world                         -- one game session's state, made fresh by Fresh()
local Frame = {}
Frame.__index = Frame
local blizzard = {}                 -- frames of the game's own: none of their setters may be called
local function Note(f, what)
    if blizzard[f] then world.touched[table.getn(world.touched) + 1] = (f.name or "?") .. ":" .. what end
end
local function NewRegion(kind, name, parent)
    local f = setmetatable({ kind = kind, name = name, parent = parent, points = {}, w = 0, h = 0, scale = 1, level = 1,
                             shown = true, scripts = {}, checked = nil }, Frame)
    if parent and parent.level then f.level = parent.level + 1 end
    if name then world.env[name] = f end
    return f
end
function Frame:SetWidth(w) Note(self, "SetWidth"); self.w = w end
function Frame:SetHeight(h) Note(self, "SetHeight"); self.h = h end
function Frame:GetWidth() return self.w end
function Frame:GetHeight() return self.h end
function Frame:ClearAllPoints() Note(self, "ClearAllPoints"); self.points = {} end
function Frame:SetPoint(p, rel, relp, x, y)
    Note(self, "SetPoint")
    if type(rel) == "string" then rel = world.env[rel] end
    table.insert(self.points, { p = p, rel = rel, relp = relp or p, x = x or 0, y = y or 0 })
end
function Frame:SetScale(s) Note(self, "SetScale"); self.scale = s end
function Frame:GetScale() return self.scale end
function Frame:SetFrameLevel(l) Note(self, "SetFrameLevel"); self.level = l end
function Frame:GetFrameLevel() return self.level end
function Frame:Show() Note(self, "Show"); self.shown = true end
function Frame:Hide() Note(self, "Hide"); self.shown = false end
function Frame:IsShown() return self.shown end
function Frame:IsVisible() return self.shown and (not self.parent or self.parent:IsVisible()) end
function Frame:GetName() return self.name end
function Frame:SetScript(h, fn) Note(self, "SetScript"); self.scripts[h] = fn end
function Frame:GetScript(h) return self.scripts[h] end
function Frame:SetChecked(c)
    if c == false or c == 0 then c = nil end   -- (1.12 takes 1 or nil; never a false or 0 from us)
    self.checked = c
end
function Frame:GetChecked() return self.checked end
function Frame:SetHitRectInsets(l, r, t, b) self.hit = { l, r, t, b } end
function Frame:SetText(t) self.text = t end
function Frame:GetText() return self.text end
function Frame:GetStringWidth()
    local t = self.text or ""
    return widths[t] or string.len(t) * 5.5
end
function Frame:CreateFontString(name, layer, template)
    if template ~= "GameFontHighlightSmall" and template ~= "GameFontNormalSmall" and template ~= "GameFontNormal" then
        error("no such font: " .. tostring(template))
    end
    local fs = NewRegion("FontString", name, self)
    fs.layer = layer; fs.font = template
    table.insert(world.made, fs)
    return fs
end
function Frame:AddMessage(t) table.insert(world.chat, t) end
local function Templated(f, template)
    if template == "UICheckButtonTemplate" then
        f.w, f.h = 32, 32
        local t = f:CreateFontString(f.name .. "Text", "ARTWORK", "GameFontNormalSmall")
        t:SetPoint("LEFT", f, "RIGHT", -2, 0)
    elseif template == "UIRadioButtonTemplate" then
        f.w, f.h = 16, 16
        local t = f:CreateFontString(f.name .. "Text", "BACKGROUND", "GameFontNormalSmall")
        t:SetPoint("LEFT", f, "RIGHT", 5, 0)
    elseif template ~= nil then
        error("no such template: " .. tostring(template))
    end
    f.template = template
end
local function CreateFrame(kind, name, parent, template)
    if kind ~= "Frame" and kind ~= "CheckButton" and kind ~= "Button" then error("CreateFrame: " .. tostring(kind)) end
    if template and not name then error("a template's $parentText needs a name") end
    local f = NewRegion(kind, name, parent)
    world.creates = world.creates + 1
    table.insert(world.made, f)
    Templated(f, template)
    return f
end
local function Tooltip()
    local tt = { lines = {} }
    function tt:SetOwner(o, a) if world.tipFails then error("tooltip broke") end; self.owner = o; self.anchor = a; self.lines = {} end
    function tt:SetText(t) self.lines = { t } end
    function tt:AddLine(t) table.insert(self.lines, t) end
    function tt:Show() self.shown = true end
    function tt:Hide() self.shown = false end
    return tt
end
local function Fresh(opts)
    opts = opts or {}
    world = { env = {}, touched = {}, made = {}, chat = {}, creates = 0, sounds = {}, cvars = {} }
    local env = world.env
    for _, k in ipairs({ "assert", "error", "ipairs", "pairs", "next", "pcall", "rawget", "rawset", "setmetatable",
                         "getmetatable", "tonumber", "tostring", "type", "unpack", "xpcall" }) do env[k] = _G[k] end
    env.string = { find = string.find, format = string.format, gsub = string.gsub, lower = string.lower, upper = string.upper,
                   len = string.len, sub = string.sub, rep = string.rep, byte = string.byte, char = string.char, gfind = string.gmatch }
    env.table = { insert = table.insert, remove = table.remove, getn = table.getn, concat = table.concat, sort = table.sort }
    env.math = { floor = math.floor, ceil = math.ceil, abs = math.abs, max = math.max, min = math.min, mod = math.fmod }
    env._G = env
    env.getglobal = function(n) return env[n] end
    env.setglobal = function(n, v) env[n] = v end
    env.strlower = string.lower
    env.CreateFrame = CreateFrame
    env.PlaySound = function(s) table.insert(world.sounds, s) end
    env.SetCVar = function(n, v)
        if world.cvars[n] == nil then error("Couldn't find CVar named '" .. tostring(n) .. "'") end
        world.cvars[n] = tostring(v)
    end
    env.GetCVar = function(n)
        if world.cvars[n] == nil then error("Couldn't find CVar named '" .. tostring(n) .. "'") end
        return world.cvars[n]
    end
    env.SlashCmdList = {}
    blizzard = {}
    env.UIParent = NewRegion("Frame", "UIParent", nil)
    env.WorldMapFrame = NewRegion("Frame", "WorldMapFrame", env.UIParent)
    env.WorldMapPositioningGuide = NewRegion("Frame", "WorldMapPositioningGuide", env.WorldMapFrame)
    env.WorldMapPositioningGuide.w, env.WorldMapPositioningGuide.h = 1024, 768
    env.WorldMapDetailFrame = NewRegion("Frame", "WorldMapDetailFrame", env.WorldMapFrame)
    env.DEFAULT_CHAT_FRAME = NewRegion("Frame", "ChatFrame1", env.UIParent)
    for _, f in ipairs({ env.UIParent, env.WorldMapFrame, env.WorldMapPositioningGuide, env.WorldMapDetailFrame,
                         env.DEFAULT_CHAT_FRAME }) do blizzard[f] = true end
    env.WorldMapFrame.level = 3
    env.WORLDMAP_WINDOWED = 0
    env.WORLDMAP_WINDOWED_SCALE = 0.7
    env.GameTooltip = Tooltip()
    env.WorldMapFrame_Update = function() end           -- (Blizzard's own functions: never replaced)
    env.ToggleWorldMap = function() end
    env.WorldMapFrame_Minimize = function() end
    if opts.noMap then env.WorldMapFrame = nil; env.WorldMapPositioningGuide = nil end
    if opts.noGuide then env.WorldMapPositioningGuide = nil end
    if opts.cvar ~= false then world.cvars.CinderLoadStyle = opts.cvar or "parchment retro" end
    return world
end
local function Run(src, name)
    local fn, err = loadstring(src, name or "CinderLoad")
    if not fn then return false, err end
    setfenv(fn, world.env)
    return pcall(fn)
end
local function Click(b)            -- as the client clicks a check button: its checked state flips, then OnClick
    if b.checked then b.checked = nil else b.checked = 1 end
    world.env.this = b; world.env.arg1 = "LeftButton"
    local h = b.scripts.OnClick
    if h then h() end
    world.env.this = nil; world.env.arg1 = nil
end
local function Fire(f, h)
    world.env.this = f
    if f.scripts[h] then f.scripts[h]() end
    world.env.this = nil
end
local function Slash(msg) world.chat = {}; world.env.SlashCmdList.CINDERLOAD(msg) end
local function Said(s)
    for _, t in ipairs(world.chat) do if string.find(t, s, 1, true) then return true end end
    return false
end
local function B(key, value) return world.env["CinderLoadMap" .. key .. value] end
local function Checks(p, s, r, n)
    return B("map", "parchment").checked == p and B("map", "satellite").checked == s and B("bar", "retro").checked == r and
           B("bar", "neo").checked == n
end

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

-- ---- the switches made ----
Fresh({ cvar = "?" })
local fnRef, toggleRef = world.env.WorldMapFrame_Update, world.env.ToggleWorldMap
ok, err = Run(setupSrc)
local env = world.env
Check(ok, "the switches' chunk runs whole (" .. tostring(err) .. ")")
Check(world.cvars.CinderLoadStyle == "parchment retro", "made: the CVar told the choice held (the DLL hears that they ran)")
Check(type(env.CinderLoad) == "table" and env.CinderLoad.up == 1 and not env.CinderLoad.broken, "made: CinderLoad.up, not broken")
local panel = env.CinderLoadMapPanel
Check(panel and panel.parent == env.WorldMapFrame and panel.kind == "Frame", "a panel of its own, on the world map's frame (shown with it)")
local all = B("map", "parchment") and B("map", "satellite") and B("bar", "retro") and B("bar", "neo")
Check(all and B("map", "parchment").template == "UICheckButtonTemplate" and B("bar", "neo").kind == "CheckButton",
      "four of Blizzard's own check buttons (UICheckButtonTemplate), so a UI that reskins them reskins these")
Check(all and env["CinderLoadMapmapparchmentText"].text == "Parchment" and env["CinderLoadMapmapsatelliteText"].text == "Satellite" and
      env["CinderLoadMapbarretroText"].text == "Retro" and env["CinderLoadMapbarneoText"].text == "Neo" and
      env.CinderLoadMapmapLabel.text == "Loading screen map:" and env.CinderLoadMapbarLabel.text == "Loading bar:",
      "his names: Loading screen map: Parchment, Satellite; Loading bar: Retro, Neo")
Check(all and Checks(1, nil, 1, nil), "the choice held shows: Parchment and Retro checked, the others not")
Check(env.SLASH_CINDERLOAD1 == "/cinderload" and type(env.SlashCmdList.CINDERLOAD) == "function", "/cinderload made")
Check(table.getn(world.chat) == 0, "made quietly (nothing said in the chat)")
Check(table.getn(world.touched) == 0, "no frame of the game's own changed: " .. table.concat(world.touched, ", "))
Check(env.WorldMapFrame_Update == fnRef and env.ToggleWorldMap == toggleRef and env.WorldMapFrame.scripts.OnShow == nil,
      "no Blizzard function replaced or hooked, no script of the map's set")
-- where it sits and how it is laid out
local pt = panel.points[1]
Check(table.getn(panel.points) == 1 and pt.p == "BOTTOMRIGHT" and pt.rel == env.WorldMapPositioningGuide and pt.relp == "BOTTOMRIGHT" and
      pt.x == -16 and pt.y == 4 and panel.scale == 1 and panel.level == env.WorldMapFrame.level + 8,
      "in the strip under the map, at the bottom right of the map's own frame, over it")
local order, prevRight, laid = { env.CinderLoadMapmapLabel, B("map", "parchment"), B("map", "satellite"), env.CinderLoadMapbarLabel,
                                 B("bar", "retro"), B("bar", "neo") }, -1, true
for _, r in ipairs(order) do
    local x = r.points[1] and r.points[1].x
    if not x or x <= prevRight then laid = false end
    if r.kind == "FontString" then prevRight = x + r:GetStringWidth() else
        local t = env[r.name .. "Text"]
        prevRight = x + r.w - 2 + t:GetStringWidth()
        if r.w ~= 24 or r.h ~= 24 or not r.hit or r.hit[2] ~= -(t:GetStringWidth() - 2) then laid = false end
    end
end
Check(laid and math.abs(panel.w - prevRight) < 1e-6 and panel.h == 24,
      "laid out left to right, nothing over anything: the labels, then each 24 x 24 box and its word (which clicks it too)")
if layoutOut then              -- for the mock sheet: each piece, its left x and width in the panel, its words
    local f = io.open(layoutOut, "w")
    f:write(string.format("panel %g %g %s %g %g\n", panel.w, panel.h, pt.p, pt.x, pt.y))
    for _, r in ipairs(order) do
        if r.kind == "FontString" then
            f:write(string.format("label %g %g %s\t%s\n", r.points[1].x, r:GetStringWidth(), r.font, r.text))
        else
            local t = env[r.name .. "Text"]
            f:write(string.format("box %g %g %s %s\t%s\n", r.points[1].x, r.w, r.checked and "1" or "0", t.font, t.text))
        end
    end
    f:close()
end
-- the probe again: there now, nothing marked
Run(probeSrc, "probe")
Check(world.cvars.CinderLoadStyle == "parchment retro", "the probe: the switches there, the CVar left as it is")

-- ---- clicked ----
Click(B("map", "satellite"))
Check(world.cvars.CinderLoadStyle == "satellite retro" and Checks(nil, 1, 1, nil) and world.sounds[1] == "igMainMenuOptionCheckBoxOn",
      "Satellite clicked: the CVar says so, Satellite checked and Parchment not (one of the two, always), the options' click sound")
Click(B("map", "satellite"))
Check(world.cvars.CinderLoadStyle == "satellite retro" and Checks(nil, 1, 1, nil), "the chosen one clicked again: it stays chosen")
Click(B("bar", "neo"))
Check(world.cvars.CinderLoadStyle == "satellite neo" and Checks(nil, 1, nil, 1), "Neo clicked: the bar on its own, the map as it was")
Fire(B("bar", "retro"), "OnEnter")
local tip = env.GameTooltip
Check(tip.shown and tip.owner == B("bar", "retro") and tip.lines[1] == "Loading bar: Retro" and string.find(tip.lines[2], "class", 1, true) and
      tip.lines[3] == "From your next loading screen.", "hovered: the game's tooltip says what it is and when it counts")
Fire(B("bar", "retro"), "OnLeave")
Check(not tip.shown, "left: the tooltip goes")

-- ---- /cinderload ----
Slash("")
Check(Said("Loading screen map: Satellite.") and Said("Loading bar: Neo.") and Said("/cinderload map"), "/cinderload: the choice, and how to change it")
Slash("map")
Check(world.cvars.CinderLoadStyle == "parchment neo" and Checks(1, nil, nil, 1) and Said("From your next loading screen."),
      "/cinderload map: flipped (the boxes follow), said when it counts")
Slash("bar")
Check(world.cvars.CinderLoadStyle == "parchment retro" and Checks(1, nil, 1, nil), "/cinderload bar: flipped")
Slash("map")
Check(world.cvars.CinderLoadStyle == "satellite retro", "/cinderload map: flipped back")
Slash("bar")
Check(world.cvars.CinderLoadStyle == "satellite neo", "/cinderload bar: flipped back")
Slash("map")
Slash("bar")
Slash("map satellite")
Check(world.cvars.CinderLoadStyle == "satellite retro", "/cinderload map satellite: set")
Slash("  BAR   Neo ")
Check(world.cvars.CinderLoadStyle == "satellite neo", "/cinderload BAR Neo (any case, any spaces): set")
Slash("parchment")
Check(world.cvars.CinderLoadStyle == "parchment neo", "/cinderload parchment (the value alone): set")
Slash("bar blue")
Check(world.cvars.CinderLoadStyle == "parchment neo" and Said("say it as"), "/cinderload bar blue: not a choice, nothing changed, how to say it")
Slash("zoom")
Check(world.cvars.CinderLoadStyle == "parchment neo" and Said("say it as"), "/cinderload zoom: not a setting, nothing changed")
Slash("neo retro")
Check(world.cvars.CinderLoadStyle == "parchment neo" and Said("say it as"), "/cinderload neo retro: muddled, nothing changed")

-- ---- the map in its window (this client's minimised map) ----
env.WORLDMAP_WINDOWED = 1
Fire(panel, "OnShow")
Check(panel.scale == 0.7 and panel.points[1].rel == env.WorldMapPositioningGuide, "the map in its window: smaller, as the map is (0.7)")
env.WORLDMAP_WINDOWED_SCALE = 5
Fire(panel, "OnShow")
Check(panel.scale == 0.7, "a strange window scale: 0.7")
env.WORLDMAP_WINDOWED = 0
Fire(panel, "OnShow")
Check(panel.scale == 1 and table.getn(world.touched) == 0, "the whole-screen map again: full size; still nothing of the game's changed")

-- ---- made again in the same UI (never twice the frames) ----
local creates = world.creates
Run(setupSrc)
Check(world.creates == creates and table.getn(env.CinderLoad.controls) == 4 and env.CinderLoadMapPanel == panel and
      world.cvars.CinderLoadStyle == "parchment retro" and Checks(1, nil, 1, nil),
      "made again in the same UI: the same frames, four controls, the choice the DLL sent")

-- ---- the other form: one word (his designer's alternatives) ----
env.CinderLoad.form = "radio"
for _, c in ipairs(env.CinderLoad.controls) do env[c.button.name] = nil end
env.CinderLoad.Build()
Check(B("map", "parchment").template == "UIRadioButtonTemplate" and B("map", "parchment").w == 16 and
      table.getn(env.CinderLoad.controls) == 4 and Checks(1, nil, 1, nil), "CL.form = \"radio\": Blizzard's radio buttons instead, the same choice")
env.CinderLoad.form = "dropdown-not-made"
env.CinderLoad.Build()
Check(table.getn(env.CinderLoad.controls) == 4, "a form not made: the check boxes")

-- ---- a fault: off, the world map untouched ----
Fresh({ cvar = "?" })
Run(setupSrc)
world.tipFails = true
Fire(B("map", "satellite"), "OnEnter")
env = world.env
Check(env.CinderLoad.broken and not env.CinderLoadMapPanel.shown and world.cvars.CinderLoadStyle == "parchment retro fault" and
      Said("turned themselves off") and Said("tooltip broke"), "a fault: off and hidden, the CVar says \"fault\", the chat says where")
Click(B("map", "satellite"))
Check(world.cvars.CinderLoadStyle == "parchment retro fault", "after a fault: a click does nothing")
Slash("map")
Check(world.cvars.CinderLoadStyle == "parchment retro fault" and Said("off until the game starts again"), "after a fault: /cinderload says it is off")
Check(table.getn(world.touched) == 0, "after a fault: nothing of the game's changed")

-- ---- without the world map's frame, or its guide; without the CVar ----
Fresh({ cvar = "?", noMap = true })
ok, err = Run(setupSrc)
Check(ok and world.cvars.CinderLoadStyle == "parchment retro" and not world.env.CinderLoadMapPanel and
      type(world.env.SlashCmdList.CINDERLOAD) == "function", "no world map frame (an add-on took it away): no panel, /cinderload still")
Fresh({ cvar = "?", noGuide = true })
Run(setupSrc)
Check(world.env.CinderLoadMapPanel.points[1].rel == world.env.WorldMapFrame, "no positioning guide: at the map frame's own corner")
Fresh({ cvar = false })
ok, err = Run(setupSrc)
Check(ok and world.env.CinderLoad.broken and Said("turned themselves off"), "no CVar: the chunk never raises; off, and the chat says why")

-- ---- the other choice, as a /reload after it would make them ----
Fresh({ cvar = "?" })
Run(setupSN)
Check(world.cvars.CinderLoadStyle == "satellite neo" and Checks(nil, 1, nil, 1), "made with satellite and neo held: those checked")

-- for the mock sheet: what /cinderload says, then /cinderload map (a fresh session, the defaults)
if layoutOut then
    Fresh({ cvar = "?" })
    Run(setupSrc)
    local f = io.open(layoutOut, "a")
    for _, m in ipairs({ "", "map" }) do
        Slash(m)
        for _, t in ipairs(world.chat) do f:write("chat /cinderload" .. (m ~= "" and " " .. m or "") .. "\t" .. t .. "\n") end
    end
    f:close()
end

print(string.format("done %d %d", checks, fails))
