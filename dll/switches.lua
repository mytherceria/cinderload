-- CinderLoad's choices in game: /cinderload opens a window of its own (his design, 11 Oct 2026: "you type that and a
-- moveable vanilla-plus looking menu pops up on screen"), the mockup he approved (PANEL-MOCKUP, 11 Oct): the loading
-- screen's map (Parchment | Satellite) and the parchment's map areas (Discovered | Full map) and its zones nearby (a
-- slider, in yards), the loading bar (Retro | Neo), its thickness and width (sliders) or the whole width of the screen,
-- its texture / effect (Heat | Smooth | Blade light) in the class's colour or not, Neo's four qualities (Liquid, Pulse,
-- Unstable, Raw: any of them), a still preview, Defaults and Close; dragged by its title. Nothing on the world map: the
-- map belongs to CinderMap. /cinderload map, bar and areas (and their values) work as before.
-- Run inside the game by CinderLoad.dll, on the game's own thread while no Lua is running (no add-on folder: the release
-- is the DLL and its packs). The DLL puts two lines before this text, the choice it holds:
--   local CL_MAP, CL_BAR, CL_AREAS = "parchment", "retro", "discovered"
--   local CL_LOOK = "nearby 1200 thick 100 width own whole off effect heat colour class quality pulse raw"
-- Written for the client's Lua 5.0: no #, no %, no ... in a body, no string methods, no string.match.
-- If this breaks, the game's own UI is unharmed: only frames of its own are made, every handler runs inside pcall, a
-- fault switches it all off (CL.broken, the CVar told "fault"), no function of Blizzard's is replaced or hooked and no
-- frame of the game's is changed (its one entry in UISpecialFrames lets Escape close it, as every panel's does).
-- The DLL hears a choice through one CVar of its own, CinderLoadStyle ("parchment retro discovered nearby 1200 thick
-- 100 ..."), and keeps it in Data\CinderLoad\style.txt: a DLL cannot give this client's Lua a function of its own (each
-- C function's address is checked against WoW.exe's code before it is called, 0x42A320 from luaD_precall 0x6F61A1).
local CL = CinderLoad
if type(CL) ~= "table" then CL = {}; CinderLoad = CL end
CL.up = 1
CL.map = CL_MAP
CL.bar = CL_BAR
CL.areas = CL_AREAS
CL.cvar = "CinderLoadStyle"
CL.broken = nil
CL.settings = {
    { key = "map", name = "loading map",
      options = { { value = "parchment", text = "Parchment" }, { value = "satellite", text = "Satellite" } } },
    { key = "bar", name = "loading bar",
      options = { { value = "retro", text = "Retro" }, { value = "neo", text = "Neo" } } },
    { key = "areas", name = "map areas",
      options = { { value = "discovered", text = "Discovered" }, { value = "full", text = "Full map" } } },
}
CL.effects = { { value = "heat", text = "Heat" }, { value = "smooth", text = "Smooth" }, { value = "blade", text = "Blade light" } }
CL.qualities = { { value = "liquid", text = "Liquid" }, { value = "pulse", text = "Pulse" }, { value = "unstable", text = "Unstable" },
                 { value = "raw", text = "Raw" } }
-- the sliders: least, most, step (the numbers are his to change); nearby is the DLL's (0 none, 4000 at most)
CL.range = { nearby = { 0, 4000, 100 }, thick = { 50, 200, 5 }, width = { 20, 100, 5 } }
-- the bar's size and effect are kept in style.txt from now, but no loading screen draws them yet: said over their boxes
-- (nil once the DLL draws them)
CL.COMING = "kept; drawn in a later version"
CL.ART = "Interface\\CinderLoad\\"             -- the panel's pictures, in CinderLoad's pack (tools/make_panel_art.py)

function CL.Defaults()
    CL.map, CL.bar, CL.areas = "parchment", "retro", "discovered"
    CL.nearby, CL.thick, CL.width, CL.whole, CL.effect, CL.ember = 1200, 100, 0, nil, "heat", nil
    CL.quality = { pulse = 1, raw = 1 }
end

-- a number held to its range, whole (as the DLL keeps it): what style.txt or the CVar says, unchanged when it is in range
function CL.Hold(v, key)
    local r = CL.range[key]
    v = math.floor(v + 0.5)
    if v < r[1] then v = r[1] end
    if v > r[2] then v = r[2] end
    return v
end
-- a slider's value: on its steps, then held to its range (only what a slider gives is put on a step)
function CL.Clamp(v, key)
    local r = CL.range[key]
    return CL.Hold(math.floor((v - r[1]) / r[3] + 0.5) * r[3] + r[1], key)
end

-- the look as the DLL sends it (CL_LOOK) or as the CVar carries it: "nearby 1200 thick 100 width own whole off effect
-- heat colour class quality pulse raw"; a part missing or not ours: as it was. Numbers are held to their range only, never
-- put on a slider's step: Setup sends back what it read, so a login never rewrites style.txt (a hand-set "nearby 40" stays)
function CL.ReadLook(s)
    local w = {}
    for t in string.gfind(s or "", "[^%s]+") do table.insert(w, t) end
    local i, n = 1, table.getn(w)
    while i <= n do
        local k, v = w[i], w[i + 1]
        local num = tonumber(v or "")
        i = i + 1
        if k == "nearby" and num then CL.nearby = CL.Hold(num, "nearby"); i = i + 1
        elseif k == "thick" and num then CL.thick = CL.Hold(num, "thick"); i = i + 1
        elseif k == "width" and num then CL.width = CL.Hold(num, "width"); i = i + 1
        elseif k == "width" and v == "own" then CL.width = 0; i = i + 1
        elseif k == "whole" and (v == "on" or v == "off") then CL.whole = (v == "on") or nil; i = i + 1
        elseif k == "effect" and CL.Find(CL.effects, v) then CL.effect = v; i = i + 1
        elseif k == "colour" and (v == "class" or v == "ember") then CL.ember = (v == "ember") or nil; i = i + 1
        elseif k == "quality" then
            local q = {}
            while w[i] and (CL.Find(CL.qualities, w[i]) or w[i] == "none") do
                if w[i] ~= "none" then q[w[i]] = 1 end
                i = i + 1
            end
            CL.quality = q
        end
    end
end

function CL.Say(text)
    local f = DEFAULT_CHAT_FRAME
    if not f then f = ChatFrame1 end
    if f then f:AddMessage("CinderLoad: " .. text, 1, 1, 0) end      -- (a system message's yellow)
end

function CL.Find(list, value)
    for _, o in ipairs(list) do
        if o.value == value then return o end
    end
end
function CL.Setting(key)
    for _, s in ipairs(CL.settings) do
        if s.key == key then return s end
    end
end
function CL.Option(s, value)
    if s then return CL.Find(s.options, value) end
end

-- "loading map Parchment, loading bar Retro, map areas Discovered"
function CL.Describe()
    local t = ""
    for _, s in ipairs(CL.settings) do
        local o = CL.Option(s, CL[s.key])
        local name = CL[s.key]
        if o then name = o.text end
        if t ~= "" then t = t .. ", " end
        t = t .. s.name .. " " .. name
    end
    return t
end

-- The CVar's value: the three, then the reach and the look, in the order the DLL reads them (StyleCvarTail)
function CL.Value()
    local q = ""
    for _, o in ipairs(CL.qualities) do
        if CL.quality[o.value] then
            if q ~= "" then q = q .. " " end
            q = q .. o.value
        end
    end
    if q == "" then q = "none" end
    local width = "own"
    if CL.width ~= 0 then width = tostring(CL.width) end
    return CL.map .. " " .. CL.bar .. " " .. CL.areas .. " nearby " .. tostring(CL.nearby) .. " thick " .. tostring(CL.thick) ..
           " width " .. width .. " whole " .. (CL.whole and "on" or "off") .. " effect " .. CL.effect .. " colour " ..
           (CL.ember and "ember" or "class") .. " quality " .. q
end

-- The choice to the DLL, through the CVar it reads (" fault" last: a fault, so it stops too).
function CL.Save(fault)
    local v = CL.Value()
    if fault then v = v .. " fault" end
    SetCVar(CL.cvar, v)
end

function CL.Set(key, value)
    if key == "map" or key == "bar" or key == "areas" then
        if not CL.Option(CL.Setting(key), value) then return end
    end
    CL[key] = value
    CL.Save()
    CL.Refresh()
    return 1
end

function CL.Fault(where, err)
    if CL.broken then return end
    CL.broken = 1
    if CL.panel then pcall(CL.panel.Hide, CL.panel) end
    pcall(CL.Save, 1)
    CL.Say("its loading screen window met an error and turned itself off until the game starts again; the game's own "
           .. "windows are as they were. |cff9d9d9d(" .. tostring(where) .. ": " .. tostring(err) .. ")|r")
end

-- fn inside pcall, as a script handler (1.12's handlers read this and arg1) or the slash command (its text).
function CL.Safe(where, fn)
    return function(a1)
        if CL.broken then return end
        local ok, err = pcall(fn, a1)
        if not ok then CL.Fault(where, err) end
    end
end

-- /cinderload alone opens the window; /cinderload map (bar, areas) switches one; /cinderload map satellite sets one
-- (the value alone does too: /cinderload neo). Words only, at most two, nothing else: anything more or other changes
-- nothing.
CL.usage = "type /cinderload to open its window, or /cinderload map, /cinderload bar or /cinderload areas to switch, or "
           .. "/cinderload map parchment, /cinderload map satellite, /cinderload bar retro, /cinderload bar neo, "
           .. "/cinderload areas discovered or /cinderload areas full."
function CL.Slash(msg)
    local words = {}
    for w in string.gfind(string.lower(msg or ""), "[^%s]+") do table.insert(words, w) end
    local n = table.getn(words)
    if n == 0 then
        CL.Open()
        return
    end
    local key, value
    local s = CL.Setting(words[1])
    if s and n == 1 then
        key = s.key
    elseif s and n == 2 and CL.Option(s, words[2]) then
        key, value = s.key, words[2]
    elseif n == 1 then
        for _, t in ipairs(CL.settings) do
            if CL.Option(t, words[1]) then key, value = t.key, words[1] end
        end
    end
    if not key then
        CL.Say("nothing changed: " .. CL.usage)
        return
    end
    s = CL.Setting(key)
    if not value then
        value = s.options[1].value
        if CL[key] == value then value = s.options[2].value end
    end
    CL.Set(key, value)
    CL.Say(CL.Describe() .. ", from your next loading screen.")
end

-- ---- the window, in the mockup's units (760 x 608, from its top left, y down) ----
CL.W, CL.H, CL.L, CL.R, CL.CW = 760, 608, 26, 394, 340
CL.GOLD, CL.WHITE = { 1, 0.82, 0 }, { 1, 1, 1 }
CL.FONT, CL.TITLEFONT = "Fonts\\FRIZQT__.TTF", "Fonts\\MORPHEUS.TTF"
-- the backdrops' fill: the client's plain white (ChatFrameBackground, 248 grey and opaque in interface.MPQ), so a colour set
-- on it is the colour drawn; the tooltip's own (UI-Tooltip-Background) is a 54% grey 73% opaque, which the colour multiplies
CL.FILL = "Interface\\ChatFrame\\ChatFrameBackground"
CL.TIP = { bgFile = CL.FILL, edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", tile = 1,
           tileSize = 16, edgeSize = 16, insets = { left = 4, right = 4, top = 4, bottom = 4 } }
CL.FRAME8 = { edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 8, tile = 1, tileSize = 8,
              insets = { left = 2, right = 2, top = 2, bottom = 2 } }
CL.LINE = { edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", edgeSize = 2, tile = 1, tileSize = 2,
            insets = { left = 0, right = 0, top = 0, bottom = 0 } }

function CL.At(r, x, y, w, h, point, rel, relPoint)
    r:ClearAllPoints()
    r:SetPoint(point or "TOPLEFT", rel or CL.panel, relPoint or "TOPLEFT", x, -y)
    if w then r:SetWidth(w) end
    if h then r:SetHeight(h) end
    return r
end
-- words: anchor as the mockup's: TOPLEFT (la), TOPRIGHT (ra), TOP (ma), LEFT (lm), BOTTOMLEFT (ld), BOTTOMRIGHT (rd), CENTER (mm)
CL.ANCHOR = { la = "TOPLEFT", ra = "TOPRIGHT", ma = "TOP", lm = "LEFT", ld = "BOTTOMLEFT", rd = "BOTTOMRIGHT", mm = "CENTER" }
function CL.Text(parent, x, y, s, size, gold, anchor, font)
    local fs = parent:CreateFontString(nil, "OVERLAY", gold and "GameFontNormal" or "GameFontHighlight")
    fs:SetFont(font or CL.FONT, size)
    local c = gold and CL.GOLD or CL.WHITE
    fs:SetTextColor(c[1], c[2], c[3])
    fs:SetText(s)
    local a = CL.ANCHOR[anchor or "la"]
    if a == "LEFT" or a == "CENTER" then
        fs:SetPoint(a, CL.panel, "TOPLEFT", x, -y)
    else
        CL.At(fs, x, y, nil, nil, a)
    end
    if anchor == "ra" or anchor == "rd" then fs:SetJustifyH("RIGHT") elseif anchor == "ma" or anchor == "mm" then fs:SetJustifyH("CENTER")
    else fs:SetJustifyH("LEFT") end
    return fs
end
-- a picture from CinderLoad's pack; the pack older than the window: a dark plate instead
function CL.Picture(t, name)
    if not t:SetTexture(CL.ART .. name) then t:SetTexture(0.08, 0.08, 0.09, 1) end
end
-- a section's box (the tooltip's border, darker inside) and its gold title over it; what follows is made in it (its
-- child: a child frame is drawn over its parent's own art, so nothing in the box is ever under the box's backdrop)
function CL.Box(x, y, w, h, title)
    local b = CreateFrame("Frame", nil, CL.panel)
    CL.At(b, x, y, w, h)
    b:SetBackdrop(CL.TIP)
    b:SetBackdropColor(0.11, 0.11, 0.118, 0.92)
    b:SetBackdropBorderColor(0.8, 0.8, 0.8)
    CL.Text(CL.panel, x + 6, y - 4, title, 14, 1, "ld")
    CL.host = b
    return b
end
function CL.Line(x, y, w)
    local l = CreateFrame("Frame", nil, CL.host)
    CL.At(l, x, y, w, 4)
    l:SetBackdrop(CL.LINE)
    l:SetBackdropBorderColor(0.5, 0.5, 0.5)
end
-- a picture in a thin frame, clicked to choose it; chosen, its frame gold (CL.Card's frame redrawn by CL.Refresh)
function CL.Card(x, y, w, h, art, key, value)
    local b = CreateFrame("Button", nil, CL.host)
    CL.At(b, x, y, w, h)
    b:EnableMouse(1)
    local t = b:CreateTexture(nil, "ARTWORK")
    t:SetPoint("TOPLEFT", b, "TOPLEFT", 2, -2)
    t:SetPoint("BOTTOMRIGHT", b, "BOTTOMRIGHT", -2, 2)
    CL.Picture(t, art)
    local f = CreateFrame("Frame", nil, b)
    f:SetBackdrop(CL.FRAME8)
    b:SetScript("OnClick", CL.Safe("card", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        CL.Set(key, value)
    end))
    local c = { button = b, picture = t, frame = f, key = key, value = value }
    table.insert(CL.ui.cards, c)
    return c
end
function CL.Radio(x, y, key, value, label)
    local r = CreateFrame("CheckButton", "CinderLoadPanel" .. key .. value, CL.host, "UIRadioButtonTemplate")
    CL.At(r, x, y, 18, 18)
    local fs = getglobal(r:GetName() .. "Text")
    fs:SetFont(CL.FONT, 13)
    fs:SetTextColor(1, 1, 1)
    fs:SetText(label)
    fs:ClearAllPoints()
    fs:SetPoint("LEFT", r, "RIGHT", 6, 0)
    fs:SetJustifyH("LEFT")
    r:SetScript("OnClick", CL.Safe("radio", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        CL.Set(key, value)
    end))
    table.insert(CL.ui.radios, { button = r, key = key, value = value })
    return r
end
function CL.Check(x, y, size, name, label, fn)
    local c = CreateFrame("CheckButton", "CinderLoadPanel" .. name, CL.host, "UICheckButtonTemplate")
    CL.At(c, x, y, size, size)
    local fs = getglobal(c:GetName() .. "Text")
    fs:SetFont(CL.FONT, 13)
    fs:SetTextColor(1, 1, 1)
    fs:SetText(label)
    fs:ClearAllPoints()
    fs:SetPoint("LEFT", c, "RIGHT", 4, 0)
    fs:SetJustifyH("LEFT")
    c:SetScript("OnClick", CL.Safe("check " .. name, function()
        if c:GetChecked() then PlaySound("igMainMenuOptionCheckBoxOn") else PlaySound("igMainMenuOptionCheckBoxOff") end
        fn(c:GetChecked() and 1 or nil)
    end))
    return c
end
function CL.Slider(x, y, w, key, label, low, high)
    local s = CreateFrame("Slider", "CinderLoadPanel" .. key .. "Slider", CL.host, "OptionsSliderTemplate")
    CL.At(s, x, y + 20, w, 17)
    local r = CL.range[key]
    s:SetMinMaxValues(r[1], r[2])
    s:SetValueStep(r[3])
    local n = s:GetName()
    local t = getglobal(n .. "Text")
    t:SetFont(CL.FONT, 13)
    t:SetTextColor(1, 1, 1)
    t:SetText(label)
    t:ClearAllPoints()
    t:SetPoint("TOPLEFT", s, "TOPLEFT", 0, 20)
    t:SetJustifyH("LEFT")
    for _, e in ipairs({ { "Low", low, "TOPLEFT", 2 }, { "High", high, "TOPRIGHT", -2 } }) do
        local fs = getglobal(n .. e[1])
        fs:SetFont(CL.FONT, 12)
        fs:SetTextColor(1, 1, 1)
        fs:SetText(e[2])
        fs:ClearAllPoints()
        fs:SetPoint(e[3], s, "BOTTOM" .. string.sub(e[3], 4), e[4], -4)
        fs:SetJustifyH(string.sub(e[3], 4))
    end
    local v = s:CreateFontString(n .. "Value", "OVERLAY", "GameFontNormal")
    v:SetFont(CL.FONT, 13)
    v:SetTextColor(CL.GOLD[1], CL.GOLD[2], CL.GOLD[3])
    v:SetJustifyH("RIGHT")
    v:SetPoint("TOPRIGHT", s, "TOPRIGHT", 0, 20)
    s:SetScript("OnValueChanged", CL.Safe("slider " .. key, function()
        if CL.quiet then return end
        local value = CL.Clamp(s:GetValue(), key)
        if value ~= CL[key] then CL.Set(key, value) end
    end))
    CL.ui.sliders[key] = { slider = s, value = v }
    return s
end
function CL.PanelButton(x, y, name, label, fn)
    local b = CreateFrame("Button", "CinderLoadPanel" .. name, CL.host, "UIPanelButtonTemplate")
    CL.At(b, x, y, 120, 26)
    b:SetText(label)
    local fs = getglobal(b:GetName() .. "Text")
    if fs then fs:SetFont(CL.FONT, 13) end
    b:SetScript("OnClick", CL.Safe(name, fn))
    return b
end

function CL.Build()
    local p = getglobal("CinderLoadPanel")
    if p and CL.ui then CL.panel = p; return end
    p = CreateFrame("Frame", "CinderLoadPanel", UIParent)
    CL.panel = p
    CL.ui = { cards = {}, radios = {}, sliders = {}, effects = {}, qualities = {} }
    CL.host = p
    p:Hide()
    p:SetWidth(CL.W)
    p:SetHeight(CL.H)
    p:SetPoint("CENTER", UIParent, "CENTER", 0, 40)
    p:SetFrameStrata("DIALOG")
    if p.SetToplevel then p:SetToplevel(1) end           -- (raised over the game's windows when clicked, where the client can)
    p:EnableMouse(1)
    p:SetMovable(1)
    if p.SetClampedToScreen then p:SetClampedToScreen(1) end
    p:SetBackdrop({ bgFile = CL.FILL, edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
                    tile = 1, tileSize = 32, edgeSize = 32, insets = { left = 11, right = 12, top = 12, bottom = 11 } })
    p:SetBackdropColor(0, 0, 0, 0.88)            -- (the "plus": the dialog's black 88% opaque, not the art's 60%)
    p:SetScript("OnHide", CL.Safe("hide", function() PlaySound("igMainMenuClose") end))
    -- the title: the dialog's header, cut in three to fit its word, its plate darker (the "plus"), dragged to move it
    local W, L, R, CW = CL.W, CL.L, CL.R, CL.CW
    local head = CreateFrame("Frame", "CinderLoadPanelTitle", p)
    CL.At(head, W / 2 - 100, -16, 200, 48)
    head:EnableMouse(1)
    head:RegisterForDrag("LeftButton")
    head:SetScript("OnDragStart", CL.Safe("drag", function() p:StartMoving() end))
    head:SetScript("OnDragStop", CL.Safe("drop", function() p:StopMovingOrSizing() end))
    local u = { 59, 79, 176, 196 }
    for i = 1, 3 do
        local t = head:CreateTexture(nil, "BACKGROUND")
        t:SetTexture("Interface\\DialogFrame\\UI-DialogBox-Header")
        t:SetTexCoord(u[i] / 256, u[i + 1] / 256, 0, 40 / 64)
        local x, w = 0, 24
        if i == 2 then x, w = 24, 152 elseif i == 3 then x, w = 176, 24 end
        CL.At(t, x, 0, w, 48, nil, head)
    end
    local plate = head:CreateTexture(nil, "BORDER")
    plate:SetTexture(0, 0, 0, 0.55)
    CL.At(plate, 8.4, 10.8, 183.2, 26.4, nil, head)
    CL.ui.title = CL.Text(head, W / 2, 8.5, "CinderLoad", 18, 1, "mm", CL.TITLEFONT)      -- (the header's own: over it)
    local close = CreateFrame("Button", "CinderLoadPanelClose", p, "UIPanelCloseButton")
    CL.At(close, W - 36, 4, 32, 32)
    close:SetScript("OnClick", CL.Safe("close", function() p:Hide() end))
    CL.Text(p, W / 2, 40, "Drag the title to move this window.  Each change shows from your next loading screen.", 12, nil, "ma")

    -- Loading map, with Map areas and Nearby zones under it
    CL.Box(L, 82, CW, 274, "Loading map")
    CL.Card(L + 14, 96, 150, 80, "PanelParchment", "map", "parchment")
    CL.Card(L + 176, 96, 150, 80, "PanelSatellite", "map", "satellite")
    CL.Radio(L + 14, 184, "map", "parchment", "Parchment")
    CL.Radio(L + 176, 184, "map", "satellite", "Satellite")
    CL.Line(L + 10, 214, CW - 20)
    CL.Text(CL.host, L + 14, 226, "Map areas, on the parchment", 13, 1)
    CL.Radio(L + 14, 250, "areas", "discovered", "Discovered")
    CL.Radio(L + 176, 250, "areas", "full", "Full map")
    CL.Line(L + 10, 278, CW - 20)
    CL.Slider(L + 16, 288, CW - 32, "nearby", "Nearby zones, faded with distance", "Short", "Long")

    -- Loading bar: Retro's words, or Neo's four qualities
    CL.Box(L, 386, CW, 162, "Loading bar")
    CL.Card(L + 14, 400, 150, 31, "PanelRetro", "bar", "retro")
    CL.Card(L + 176, 400, 150, 31, "PanelNeo", "bar", "neo")
    CL.Radio(L + 14, 440, "bar", "retro", "Retro")
    CL.Radio(L + 176, 440, "bar", "neo", "Neo")
    CL.ui.words = {
        CL.Text(CL.host, L + 14, 474, "Retro: the game's own bar, in your class's colour.", 12),
        CL.Text(CL.host, L + 14, 494, "Neo: a blade of light in a metal frame, edge to edge,", 12),
        CL.Text(CL.host, L + 14, 512, "the RavenCraft logo on it.", 12) }
    CL.ui.neoTitle = CL.Text(CL.host, L + 14, 470, "Neo's qualities, any of them", 13, 1)
    for i, o in ipairs(CL.qualities) do
        local value = o.value
        local x, y = L + 14, 490
        if i == 2 or i == 4 then x = L + 176 end
        if i > 2 then y = 516 end
        local c = CL.Check(x, y, 26, "Quality" .. o.text, o.text, function(on)
            CL.quality[value] = on
            CL.Save()
            CL.Refresh()
        end)
        CL.ui.qualities[value] = c
    end

    -- Bar size
    CL.Box(R, 82, CW, 178, "Bar size")
    if CL.COMING then CL.ui.coming = { CL.Text(p, R + CW - 4, 78, CL.COMING, 12, nil, "rd") } end
    CL.Slider(R + 16, 94, CW - 32, "thick", "Bar thickness", "Thin", "Thick")
    CL.Slider(R + 16, 158, CW - 32, "width", "Bar width", "Narrow", "Wide")
    CL.ui.whole = CL.Check(R + 14, 222, 28, "Whole", "Whole width of the screen", function(on) CL.Set("whole", on) end)

    -- Texture / effect
    CL.Box(R, 300, CW, 96, "Texture / effect")
    if CL.COMING then table.insert(CL.ui.coming, CL.Text(p, R + CW - 4, 296, CL.COMING, 12, nil, "rd")) end
    for i, o in ipairs(CL.effects) do
        local x = R + 16 + (i - 1) * 106
        CL.ui.effects[o.value] = CL.Card(x, 312, 96, 24, "Panel" .. string.upper(string.sub(o.value, 1, 1)) .. string.sub(o.value, 2), "effect", o.value)
        CL.Text(CL.host, x + 48, 342, o.text, 12, nil, "ma")
    end
    CL.ui.colour = CL.Check(R + 14, 362, 28, "Colour", "In my class's colour", function(on) CL.Set("ember", not on or nil) end)

    -- Preview: the bottom of the screen in miniature, the bar at the chosen thickness and width (a still for now)
    CL.Box(R, 436, CW, 112, "Preview")
    CL.Text(p, R + CW - 4, 432, "a still here; it moves in game", 12, nil, "rd")
    local strip = CreateFrame("Frame", "CinderLoadPanelPreview", CL.host)
    CL.At(strip, R + 12, 448, CW - 24, 88)
    local ground = strip:CreateTexture(nil, "BACKGROUND")
    ground:SetAllPoints(strip)
    CL.ui.preview = { strip = strip, ground = ground, bar = {} }
    for i = 1, 3 do CL.ui.preview.bar[i] = strip:CreateTexture(nil, "ARTWORK") end
    local over = CreateFrame("Frame", nil, strip)                    -- (the fill: a child, so drawn over the bar's art)
    CL.At(over, 0, 0, CW - 24, 88, nil, strip)
    CL.ui.preview.fill = over:CreateTexture(nil, "ARTWORK")
    CL.ui.preview.rest = over:CreateTexture(nil, "ARTWORK")
    CL.ui.preview.core = over:CreateTexture(nil, "OVERLAY")
    local edge = CreateFrame("Frame", nil, strip)
    CL.At(edge, -2, -2, CW - 20, 92, nil, strip)
    edge:SetBackdrop(CL.FRAME8)
    edge:SetBackdropBorderColor(0.75, 0.75, 0.75)

    CL.host = p
    CL.PanelButton(L + 4, CL.H - 40, "Defaults", "Defaults", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        CL.Defaults()
        CL.Save()
        CL.Refresh()
    end)
    CL.PanelButton(W - 26 - 124, CL.H - 40, "CloseButton", "Close", function() p:Hide() end)
    pcall(table.insert, UISpecialFrames, "CinderLoadPanel")         -- (Escape closes it, as it closes the game's own)
end

-- the Retro bar's own width on this screen, percent: 512 of its art's texels, a texel the screen's height over 768
function CL.OwnWidth()
    local w, h = GetScreenWidth(), GetScreenHeight()
    if not w or not h or w <= 0 then return 28 end
    return 51200 / 768 * h / w
end

-- the preview: the bottom of the screen at the strip's width (316 for the screen's whole width), the bar on it as the
-- loading screen lays it (retro: 42 of 720 tall, centred 0.075 of the height up; neo: its frame band, 0.066 of the height,
-- on the bottom), at the chosen thickness and width; its pictures cut from real loading screens
function CL.Preview()
    local v = CL.ui.preview
    local sw, sh = 316, 88
    local aspect = 0.4219
    local w, h = GetScreenWidth(), GetScreenHeight()
    if w and h and w > 0 then aspect = h / w end
    local H = sw * aspect                                -- the screen's height at the strip's scale
    CL.Picture(v.ground, CL.map == "parchment" and "PreviewParchment" or "PreviewSatellite")
    local neo = CL.bar == "neo"
    local bh, cy, cap, ucap, art = 42 / 720 * H, 0.075 * H, 34 / 42, 34 / 476, "PreviewRetro"
    if neo then bh, cy, cap, ucap, art = 0.066 * H, 0.04 * H, 40 / 48, 40 / 1704, "PreviewNeo" end
    bh = bh * CL.thick / 100
    local bw = sw * CL.width / 100
    if CL.whole then bw = sw
    elseif CL.width == 0 then if neo then bw = sw else bw = sw * CL.OwnWidth() / 100 end end
    local cw = bh * cap
    if cw * 2 > bw then cw = bw / 2 end
    local x0, top = (sw - bw) / 2, sh - cy - bh / 2
    local parts = { { x0, cw, 0, ucap }, { x0 + cw, bw - 2 * cw, ucap, 1 - ucap }, { x0 + bw - cw, cw, 1 - ucap, 1 } }
    for i, q in ipairs(parts) do
        local t = v.bar[i]
        CL.Picture(t, art)
        t:SetTexCoord(q[3], q[4], 0, 1)
        CL.At(t, q[1], top, q[2], bh, nil, v.strip)
    end
    v.size = { x0, top, bw, bh }
    -- the fill in the bar's channel (retro: its art's texels 28 to 449 of 476 across, 9 to 34 of 42 down; neo: inside its
    -- frame's rails, rows 11 to 45 of 48, from 8 texels into each cap, where the blade's light lies), to 60% (a still), the
    -- way still to go dark
    local cl, cr, ct, ch
    if neo then cl, cr, ct, ch = x0 + cw * 8 / 40, x0 + bw - cw * 8 / 40, top + bh * 11 / 48, bh * 34 / 48
    else cl, cr, ct, ch = x0 + cw * 28 / 34, x0 + bw - cw * 27 / 34, top + bh * 9 / 42, bh * 25 / 42 end
    local c, fw = CL.FillColour(), (cr - cl) * 0.6
    v.fill:SetTexture(CL.FILL)
    if CL.effect == "heat" and v.fill.SetGradient then
        v.fill:SetGradient("HORIZONTAL", c[1] * 0.35, c[2] * 0.35, c[3] * 0.35, c[1], c[2], c[3])
    else
        v.fill:SetVertexColor(c[1], c[2], c[3])
    end
    CL.At(v.fill, cl, ct, fw, ch, nil, v.strip)
    v.rest:SetTexture(0.03, 0.03, 0.035, 1)
    CL.At(v.rest, cl + fw, ct, cr - cl - fw, ch, nil, v.strip)
    v.core:SetTexture(CL.FILL)
    v.core:SetVertexColor(0.75 + c[1] / 4, 0.75 + c[2] / 4, 0.75 + c[3] / 4)
    CL.At(v.core, cl, ct + ch * 0.3, fw, ch * 0.4, nil, v.strip)
    if CL.effect == "blade" then v.core:Show() else v.core:Hide() end
    v.colour = c
end

-- the fill's colour: the class's (as the DLL's kClassColour has them, the shaman's RavenCraft's blue) or CinderLoad's ember
CL.EMBER = { 1, 0.541, 0.165 }
CL.CLASS = { WARRIOR = { 0.78, 0.612, 0.431 }, PALADIN = { 0.961, 0.549, 0.729 }, HUNTER = { 0.671, 0.831, 0.451 },
             ROGUE = { 1, 0.961, 0.412 }, PRIEST = { 1, 1, 1 }, SHAMAN = { 0, 0.439, 0.871 }, MAGE = { 0.412, 0.8, 0.941 },
             WARLOCK = { 0.58, 0.51, 0.788 }, DRUID = { 1, 0.49, 0.039 } }
function CL.FillColour()
    if CL.ember or not UnitClass then return CL.EMBER end
    local _, token = UnitClass("player")
    return CL.CLASS[token or ""] or CL.EMBER
end

-- every control shows the choice
function CL.Refresh()
    if not CL.ui then return end
    CL.quiet = 1
    for _, c in ipairs(CL.ui.cards) do
        local on = CL[c.key] == c.value
        local g = 2
        if on then g = 3 end
        c.frame:ClearAllPoints()
        c.frame:SetPoint("TOPLEFT", c.button, "TOPLEFT", -g, g)
        c.frame:SetPoint("BOTTOMRIGHT", c.button, "BOTTOMRIGHT", g, -g)
        if on then c.frame:SetBackdropBorderColor(1, 0.85, 0.15) else c.frame:SetBackdropBorderColor(0.6, 0.6, 0.6) end
        c.on = on
    end
    for _, r in ipairs(CL.ui.radios) do r.button:SetChecked(CL[r.key] == r.value) end
    for key, s in pairs(CL.ui.sliders) do
        local value = CL[key]
        local text
        if key == "nearby" then
            text = tostring(value) .. " yards"
            if value == 0 then text = "off" end
        elseif key == "thick" then
            text = tostring(value) .. "%"
        else
            text = tostring(value) .. "% of the screen"
            if value == 0 then
                value = CL.Clamp(CL.OwnWidth(), "width")
                if CL.bar == "neo" then value = 100 end
                text = "each bar's own"
            end
            if CL.whole then value, text = 100, "the whole screen" end
        end
        s.slider:SetValue(value)
        s.value:SetText(text)
    end
    CL.ui.whole:SetChecked(CL.whole)
    local ws = CL.ui.sliders.width.slider
    if CL.whole then
        if ws.Disable then ws:Disable() end
        ws:SetAlpha(0.5)
    else
        if ws.Enable then ws:Enable() end
        ws:SetAlpha(1)
    end
    CL.ui.colour:SetChecked(not CL.ember)
    local neo = CL.bar == "neo"
    for _, fs in ipairs(CL.ui.words) do if neo then fs:Hide() else fs:Show() end end
    if neo then CL.ui.neoTitle:Show() else CL.ui.neoTitle:Hide() end
    for value, c in pairs(CL.ui.qualities) do
        c:SetChecked(CL.quality[value])
        if neo then c:Show() else c:Hide() end
    end
    CL.Preview()
    CL.quiet = nil
end

function CL.Open()
    CL.Build()
    CL.Refresh()
    if not CL.panel:IsShown() then
        CL.panel:Show()
        PlaySound("igMainMenuOpen")
    end
end

function CL.Setup()
    CL.Defaults()
    CL.map, CL.bar, CL.areas = CL_MAP, CL_BAR, CL_AREAS
    CL.ReadLook(CL_LOOK)
    CL.Save()                                        -- (first: the DLL hears that this ran)
    SLASH_CINDERLOAD1 = "/cinderload"
    SlashCmdList["CINDERLOAD"] = function(msg)
        if CL.broken then
            CL.Say("its loading screen window is off until the game starts again (it met an error).")
            return
        end
        CL.Safe("slash", CL.Slash)(msg)
    end
    if getglobal("CinderLoadPanel") and CL.ui then CL.panel = getglobal("CinderLoadPanel"); CL.Refresh() end
end

local ok, err = pcall(CL.Setup)
if not ok then CL.Fault("setup", err) end
