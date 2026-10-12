-- CinderLoad's choices in game: /cinderload opens a window of its own (his design, 11 Oct 2026: "you type that and a
-- moveable vanilla-plus looking menu pops up on screen"), the mockup he approved (PANEL-MOCKUP, 11 Oct): the loading
-- screen's map (Parchment | Satellite) and the parchment's map areas (Discovered | Full map) and how its zones nearby
-- fade (four ways, his pick of 11 Oct: each zone whole, each zone's nearest third, a circle of 3,600 or 1,800 yards), the
-- loading bar (Retro | Neo), its thickness and width (sliders) or the whole width of the screen (Retro's alone: Neo is
-- always the whole width), its texture / effect (Heat | Smooth | Blade light) in the class's colour or not, Neo's four
-- qualities (Liquid, Pulse, Unstable, Raw: any of them), a moving preview, Defaults and Close; dragged by its title.
-- Nothing on the world map: the map belongs to CinderMap. /cinderload map, bar and areas (and their values) work as
-- before.
-- Run inside the game by CinderLoad.dll, on the game's own thread while no Lua is running (no add-on folder: the release
-- is the DLL and its packs). The DLL puts two lines before this text, the choice it holds:
--   local CL_MAP, CL_BAR, CL_AREAS = "parchment", "retro", "full"
--   local CL_LOOK = "fade zone thick 100 width own whole off effect heat neofill blade colour class quality pulse raw"
-- Written for the client's Lua 5.0: no #, no %, no ... in a body, no string methods, no string.match.
-- If this breaks, the game's own UI is unharmed: only frames of its own are made, every handler runs inside pcall, a
-- fault switches it all off (CL.broken, the CVar told "fault"), no function of Blizzard's is replaced or hooked and no
-- frame of the game's is changed (its one entry in UISpecialFrames lets Escape close it, as every panel's does).
-- The DLL hears a choice through one CVar of its own, CinderLoadStyle ("parchment retro full fade zone thick 100 ..."),
-- and keeps it in Data\CinderLoad\style.txt: a DLL cannot give this client's Lua a function of its own (each
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
-- how the zones nearby fade (style.txt's fade line): his four, the first his default
CL.fades = { { value = "zone", text = "Per zone, whole zone" }, { value = "third", text = "Per zone, nearest third" },
             { value = "3600", text = "Circle, 3,600 yd" }, { value = "1800", text = "Circle, 1,800 yd" } }
-- the sliders: least, most, step (the numbers are his to change)
CL.range = { thick = { 50, 200, 5 }, width = { 20, 100, 5 } }
-- with Neo picked, over Bar size: why its width is greyed (his, 11 Oct: Neo's ends have no metal caps)
CL.NEOWIDE = "Neo: always whole width (no metal end caps)"
-- The window's pictures are made here, at run time, from the client's own textures and CinderLoad's own pack: none is
-- shipped (his rules: the parchment's art is read from the player's own client, never shipped; no Blizzard assets).
-- The parchment: the client's painting of the Eastern Kingdoms (12 tiles, 4 x 3 of 256, 1002 x 668 of it painted), by
-- the candle's light as the DLL lights it (ParchMesh: 1, 0.84, 0.62 times the light). The living map: CinderLoad's own
-- stitched world (CinderMap, in its pack). Retro's frame: the client's casting-bar frame, the nearest it has: vanilla's
-- loading-bar frame is hidden in game by CinderLoad's pack (its Loading-BarBorder.blp is clear, so the game's own bar
-- shows CinderLoad's fire); its art 432 x 60 of 512 x 128 from 40, 34, caps 40 wide, its channel 68 to 444 across and 57
-- to 73 down. Neo's frame and logo: CinderLoad's own (CinderBar, bar.txt; CinderLogo, logo.txt). The fills: drawn.
CL.PAINTING = "Interface\\WorldMap\\Azeroth\\Azeroth"
CL.LIVING = "Interface\\Glues\\LoadingScreens\\CinderMap"
CL.RETROFRAME = "Interface\\CastingBar\\UI-CastingBar-Border"
CL.NEOFRAME, CL.NEOLOGO = "Interface\\Glues\\LoadingScreens\\CinderBar", "Interface\\Glues\\LoadingScreens\\CinderLogo"
-- what each picture shows: the parchment's in its painting's pixels (x0, y0, x1, y1), the living map's in its texture's
-- (u0, u1, v0, v1): the map's picture, the bar's ground and the preview's (the bottom of a loading screen)
CL.CROP = { parchment = { card = { 380, 430, 620, 555 }, bar = { 380, 440, 680, 496 }, strip = { 300, 520, 800, 659 } },
            satellite = { card = { 0.6982, 0.7959, 0.5469, 0.5977 }, bar = { 0.708, 0.7959, 0.6055, 0.6218 },
                          strip = { 0.6836, 0.8301, 0.6348, 0.6755 } } }

-- everything as it begins (his defaults, 11 Oct: the parchment, the full map, each zone fading whole; Retro)
function CL.Defaults()
    CL.map, CL.bar, CL.areas, CL.fade = "parchment", "retro", "full", "zone"
    CL.thick, CL.width, CL.whole, CL.ember = 100, 0, nil, nil
    CL.retrofx, CL.neofx = "heat", "blade"                 -- (each bar keeps its own fill: his pick, "Neo keeps its blade")
    CL.effect = CL.retrofx
    CL.quality = { pulse = 1, raw = 1 }
end
-- the fill shown and chosen is the bar's own: CL.effect follows CL.bar
function CL.BarFill()
    if CL.bar == "neo" then CL.effect = CL.neofx else CL.effect = CL.retrofx end
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

-- the look as the DLL sends it (CL_LOOK) or as the CVar carries it: "fade zone thick 100 width own whole off effect heat
-- neofill blade colour class quality pulse raw" (effect Retro's fill, neofill Neo's); a part missing or not ours: as it was. Numbers are held to their range only, never
-- put on a slider's step: Setup sends back what it read, so a login never rewrites style.txt (a hand-set "thick 123" stays)
function CL.ReadLook(s)
    local w = {}
    for t in string.gfind(s or "", "[^%s]+") do table.insert(w, t) end
    local i, n = 1, table.getn(w)
    while i <= n do
        local k, v = w[i], w[i + 1]
        local num = tonumber(v or "")
        i = i + 1
        if k == "fade" and CL.Find(CL.fades, v) then CL.fade = v; i = i + 1
        elseif k == "thick" and num then CL.thick = CL.Hold(num, "thick"); i = i + 1
        elseif k == "width" and num then CL.width = CL.Hold(num, "width"); i = i + 1
        elseif k == "width" and v == "own" then CL.width = 0; i = i + 1
        elseif k == "whole" and (v == "on" or v == "off") then CL.whole = (v == "on") or nil; i = i + 1
        elseif k == "effect" and CL.Find(CL.effects, v) then CL.retrofx = v; i = i + 1
        elseif k == "neofill" and CL.Find(CL.effects, v) then CL.neofx = v; i = i + 1
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

-- The CVar's value: the three, then the fade and the look, in the order the DLL reads them (StyleCvarTail)
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
    return CL.map .. " " .. CL.bar .. " " .. CL.areas .. " fade " .. CL.fade .. " thick " .. tostring(CL.thick) ..
           " width " .. width .. " whole " .. (CL.whole and "on" or "off") .. " effect " .. CL.retrofx .. " neofill " .. CL.neofx .. " colour " ..
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
    if key == "effect" then                              -- (the fill picked is the bar's own: kept for it)
        if CL.bar == "neo" then CL.neofx = value else CL.retrofx = value end
    end
    CL.BarFill()
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
-- ---- a scene: a picture made of textures (its ground, the bar's channel and fill, the bar's frame over them in a child
-- frame, Neo's logo over all), each from a pool; a picture not in the client (an older pack): a dark plate instead ----
function CL.Scene(parent, x, y, w, h, name, rel)
    local f = CreateFrame("Frame", name, parent)
    CL.At(f, x, y, w, h, nil, rel or parent)
    local over = CreateFrame("Frame", nil, f)
    CL.At(over, 0, 0, w, h, nil, f)
    return { frame = f, over = over, w = w, h = h, pools = {} }
end
function CL.SceneBegin(s) for _, p in pairs(s.pools) do p.n = 0 end end
function CL.SceneEnd(s)
    for _, p in pairs(s.pools) do
        for i = p.n + 1, table.getn(p.list) do p.list[i]:Hide() end
    end
end
function CL.Tex(s, pool, layer)
    local p = s.pools[pool]
    if not p then
        p = { n = 0, list = {} }
        s.pools[pool] = p
    end
    p.n = p.n + 1
    local t = p.list[p.n]
    if not t then
        local owner = s.frame
        if pool == "frame" or pool == "logo" then owner = s.over end
        t = owner:CreateTexture(nil, layer)
        p.list[p.n] = t
    end
    t:Show()
    return t
end
-- part u0..u1, v0..v1 of a picture (file, or a colour { r, g, b, a }) at x, y, w x h in the scene, cut to the scene's
-- edges (its picture cut the same), in colour c; the texture, or nil when none of it is in the scene
function CL.Quad(s, pool, layer, file, x, y, w, h, u0, u1, v0, v1, c)
    if not (w > 0 and h > 0) then return end
    local ax, ay, bx, by = math.max(x, 0), math.max(y, 0), math.min(x + w, s.w), math.min(y + h, s.h)
    if bx <= ax or by <= ay then return end
    local t = CL.Tex(s, pool, layer)
    if type(file) == "table" then t:SetTexture(file[1], file[2], file[3], file[4])
    elseif not t:SetTexture(file) then t:SetTexture(0.08, 0.08, 0.09, 1) end
    local du, dv = (u1 - u0) / w, (v1 - v0) / h
    t:SetTexCoord(u0 + (ax - x) * du, u0 + (bx - x) * du, v0 + (ay - y) * dv, v0 + (by - y) * dv)
    CL.At(t, ax, ay, bx - ax, by - ay, nil, s.frame)
    c = c or CL.WHITE
    t:SetVertexColor(c[1], c[2], c[3])
    return t
end
-- the ground: the parchment's painting (its crop in the painting's pixels) or the living map (its crop of CinderMap), at
-- light (1 bright, 0.7 the foot of a loading screen, as dim as the window's preview has always shown it)
function CL.Ground(s, map, crop, light)
    if map ~= "parchment" then
        CL.Quad(s, "ground", "BACKGROUND", CL.LIVING, 0, 0, s.w, s.h, crop[1], crop[2], crop[3], crop[4], { light, light, light })
        return
    end
    local c, sx, sy = { light, light * 0.84, light * 0.62 }, s.w / (crop[3] - crop[1]), s.h / (crop[4] - crop[2])
    for i = 1, 12 do
        local ty = math.floor((i - 1) / 4)
        local tx = i - 1 - ty * 4
        local ax, ay = math.max(crop[1], tx * 256), math.max(crop[2], ty * 256)
        local bx, by = math.min(crop[3], tx * 256 + 256), math.min(crop[4], ty * 256 + 256)
        if bx > ax and by > ay then
            CL.Quad(s, "ground", "BACKGROUND", CL.PAINTING .. tostring(i), (ax - crop[1]) * sx, (ay - crop[2]) * sy, (bx - ax) * sx,
                    (by - ay) * sy, (ax - tx * 256) / 256, (bx - tx * 256) / 256, (ay - ty * 256) / 256, (by - ty * 256) / 256, c)
        end
    end
end
-- the fill (drawn: the bar's own effect in the chosen colour) in a channel at x, y, w x h, to share of its length, the way
-- still to go dark: Heat dark at its tail to the colour at its tip, Smooth even, Blade light the colour round a near-white core
function CL.Fill(s, x, y, w, h, share, effect, c)
    local fw = w * share
    local t = CL.Quad(s, "fill", "ARTWORK", CL.FILL, x, y, fw, h, 0, 1, 0, 1, c)
    if t and effect == "heat" and t.SetGradient then
        t:SetGradient("HORIZONTAL", c[1] * 0.35, c[2] * 0.35, c[3] * 0.35, c[1], c[2], c[3])
    end
    CL.Quad(s, "fill", "ARTWORK", { 0.03, 0.03, 0.035, 1 }, x + fw, y, w - fw, h, 0, 1, 0, 1)
    if effect == "blade" then
        CL.Quad(s, "fill", "OVERLAY", CL.FILL, x, y + h * 0.3, fw, h * 0.4, 0, 1, 0, 1, { 0.75 + c[1] / 4, 0.75 + c[2] / 4, 0.75 + c[3] / 4 })
    end
    return t
end
-- Retro at x0, top, bw x bh (its frame as seen), its fill to share
function CL.RetroBar(s, x0, top, bw, bh, share, effect, c)
    local k = bh / 60
    local cap = 40 * k
    if cap * 2 > bw then cap = bw / 2 end
    s.channel = { x0 + 28 * k, top + 23 * k, bw - 56 * k, 16 * k }
    CL.Fill(s, s.channel[1], s.channel[2], s.channel[3], s.channel[4], share, effect, c)
    local F, v0, v1 = CL.RETROFRAME, 34 / 128, 94 / 128
    CL.Quad(s, "frame", "ARTWORK", F, x0, top, cap, bh, 40 / 512, 80 / 512, v0, v1)
    CL.Quad(s, "frame", "ARTWORK", F, x0 + cap, top, bw - 2 * cap, bh, 80 / 512, 432 / 512, v0, v1)
    CL.Quad(s, "frame", "ARTWORK", F, x0 + bw - cap, top, cap, bh, 432 / 512, 472 / 512, v0, v1)
end
-- Neo from x0 to x1 standing on bottom, its rail band band tall (its pieces 313 rows of the sheet's 512, the band their
-- rows 223 to 313, its channel 236 to 301; caps 422 wide, the rail 128 repeated, the centre 40), its fill to share; its
-- logo logoH tall, its foot foot up, centred (logo.txt: 4, 4 to 1020, 484 of 1024 x 512)
function CL.NeoBar(s, x0, x1, bottom, band, share, effect, c, logoH, foot)
    local k = band / 90
    local top, ph = bottom - 313 * k, 313 * k
    s.channel = { x0, top + 236 * k, x1 - x0, 65 * k }
    CL.Fill(s, x0, top + 236 * k, x1 - x0, 65 * k, share, effect, c)
    local F, v1, cap, rail, mid = CL.NEOFRAME, 313 / 512, 422 * k, 128 * k, 40 * k
    local cx = (x0 + x1 - mid) / 2
    for side = 0, 1 do                                  -- (the rails: the left cap to the centre, the centre to the right cap)
        local a, b = x0 + cap, cx
        if side == 1 then a, b = cx + mid, x1 - cap end
        local x = a
        while x < b - 0.01 do
            local w = math.min(rail, b - x)
            CL.Quad(s, "frame", "ARTWORK", F, x, top, w, ph, 426.5 / 1024, (426.5 + 127 * w / rail) / 1024, 0, v1)
            x = x + rail
        end
    end
    CL.Quad(s, "frame", "ARTWORK", F, x0, top, cap, ph, 0.5 / 1024, 421.5 / 1024, 0, v1)
    CL.Quad(s, "frame", "ARTWORK", F, cx, top, mid, ph, 558.5 / 1024, 597.5 / 1024, 0, v1)
    CL.Quad(s, "frame", "ARTWORK", F, x1 - cap, top, cap, ph, 602.5 / 1024, 1023.5 / 1024, 0, v1)
    if logoH then
        local lw = logoH * 2.117
        CL.Quad(s, "logo", "OVERLAY", CL.NEOLOGO, (x0 + x1 - lw) / 2, bottom - foot - logoH, lw, logoH, 4 / 1024, 1020 / 1024, 4 / 512, 484 / 512)
    end
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
-- a picture in a thin frame, clicked to choose it; chosen, its frame gold (CL.Card's frame and its picture, draw(scene),
-- redrawn by CL.Refresh)
function CL.Card(x, y, w, h, draw, key, value)
    local b = CreateFrame("Button", nil, CL.host)
    CL.At(b, x, y, w, h)
    b:EnableMouse(1)
    local sc = CL.Scene(b, 2, 2, w - 4, h - 4)
    local f = CreateFrame("Frame", nil, b)
    f:SetBackdrop(CL.FRAME8)
    b:SetScript("OnClick", CL.Safe("card", function()
        PlaySound("igMainMenuOptionCheckBoxOn")
        CL.Set(key, value)
    end))
    local c = { button = b, scene = sc, draw = draw, frame = f, key = key, value = value }
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
-- the red buttons' words at the window's size in each state: font objects of the window's own, gold at rest and white
-- under the mouse as the template's are (a size set on the words alone gives way to the template's GameFontHighlight
-- when the mouse is over it, and stays at its 12)
function CL.ButtonFont(name, c)
    local f = getglobal(name)
    if not f then f = CreateFont(name) end
    f:SetFont(CL.FONT, 13)
    f:SetTextColor(c[1], c[2], c[3])
    return f
end
CL.GLOW = 0.4
function CL.PanelButton(x, y, name, label, fn)
    local b = CreateFrame("Button", "CinderLoadPanel" .. name, CL.host, "UIPanelButtonTemplate")
    CL.At(b, x, y, 120, 26)
    b:SetTextFontObject(CL.ButtonFont("CinderLoadButtonFont", CL.GOLD))
    b:SetHighlightFontObject(CL.ButtonFont("CinderLoadButtonFontHighlight", CL.WHITE))
    -- its glow under the mouse dimmed to CL.GLOW (his 7:1: the white words over the template's full glow measured 4.3:1
    -- at the brightest pixel behind them, at 0.4 of it 8.2:1, as the gold at rest is 8.1:1)
    local hl = b:GetHighlightTexture()
    if hl then hl:SetVertexColor(CL.GLOW, CL.GLOW, CL.GLOW) end
    b:SetText(label)
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

    -- Loading map, with Map areas and how the zones nearby fade under it
    CL.Box(L, 82, CW, 274, "Loading map")
    for i, m in ipairs({ "parchment", "satellite" }) do
        local map = m
        CL.Card(L + 14 + (i - 1) * 162, 96, 150, 80, function(sc) CL.Ground(sc, map, CL.CROP[map].card, 1) end, "map", map)
    end
    CL.Radio(L + 14, 184, "map", "parchment", "Parchment")
    CL.Radio(L + 176, 184, "map", "satellite", "Satellite")
    CL.Line(L + 10, 214, CW - 20)
    CL.Text(CL.host, L + 14, 226, "Map areas, on the parchment", 13, 1)
    CL.Radio(L + 14, 250, "areas", "discovered", "Discovered")
    CL.Radio(L + 176, 250, "areas", "full", "Full map")
    CL.Line(L + 10, 278, CW - 20)
    CL.Text(CL.host, L + 14, 286, "Nearby zones, faded with distance", 13, 1)
    for i, o in ipairs(CL.fades) do                              -- (each zone's two down the left, the circles' on the right)
        local x, y = L + 14, 306
        if i > 2 then x = L + 200 end
        if i == 2 or i == 4 then y = 328 end
        CL.Radio(x, y, "fade", o.value, o.text)
    end

    -- Loading bar: Retro's words, or Neo's four qualities
    CL.Box(L, 386, CW, 162, "Loading bar")
    CL.Card(L + 14, 400, 150, 31, function(sc)                       -- (Retro on the parchment, Neo on the living map)
        CL.Ground(sc, "parchment", CL.CROP.parchment.bar, 1)
        CL.RetroBar(sc, 7, 3.5, sc.w - 14, 20, 0.6, CL.retrofx, CL.LightOf("retro", CL.retrofx))
    end, "bar", "retro")
    CL.Card(L + 176, 400, 150, 31, function(sc)
        CL.Ground(sc, "satellite", CL.CROP.satellite.bar, 1)
        CL.NeoBar(sc, sc.w / 2 - 300, sc.w / 2 + 300, sc.h, 11, 0.6, CL.neofx, CL.LightOf("neo", CL.neofx), 11 * 0.085 / 0.066, 11 * 0.004 / 0.066)
    end, "bar", "neo")
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
    CL.Slider(R + 16, 94, CW - 32, "thick", "Bar thickness", "Thin", "Thick")
    CL.Slider(R + 16, 158, CW - 32, "width", "Bar width", "Narrow", "Wide")
    CL.ui.whole = CL.Check(R + 14, 222, 28, "Whole", "Whole width of the screen", function(on) CL.Set("whole", on) end)
    CL.ui.neoWide = CL.Text(p, R + CW - 4, 78, CL.NEOWIDE, 12, nil, "rd")     -- (shown with Neo: its width greyed)

    -- Texture / effect
    CL.Box(R, 300, CW, 96, "Texture / effect")
    for i, o in ipairs(CL.effects) do
        local x = R + 16 + (i - 1) * 106
        local fx = o.value
        CL.ui.effects[fx] = CL.Card(x, 312, 96, 24, function(sc) CL.Fill(sc, 0, 0, sc.w, sc.h, 1, fx, CL.LightOf(CL.bar, fx)) end, "effect", fx)
        CL.Text(CL.host, x + 48, 342, o.text, 12, nil, "ma")
    end
    CL.ui.colour = CL.Check(R + 14, 362, 28, "Colour", "In my class's colour", function(on) CL.Set("ember", not on or nil) end)

    -- Preview: the bottom of the screen in miniature, the bar at the chosen thickness and width (a still for now)
    CL.Box(R, 436, CW, 112, "Preview")
    CL.Text(p, R + CW - 4, 432, "a still here; it moves in game", 12, nil, "rd")
    local sc = CL.Scene(CL.host, R + 12, 448, CW - 24, 88, "CinderLoadPanelPreview", p)
    local strip = sc.frame
    CL.ui.preview = sc
    CL.ui.previewFrame = strip
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
-- loading screen lays it (retro: 42 of 720 tall, centred 0.075 of the height up at every thickness; neo: its frame band,
-- 0.066 of the height, standing on the bottom at every thickness, always the whole width), at the chosen thickness and
-- width; its pictures cut from real loading screens
function CL.Preview()
    local v = CL.ui.preview
    local sw, sh = v.w, v.h
    local aspect = 0.4219
    local w, h = GetScreenWidth(), GetScreenHeight()
    if w and h and w > 0 then aspect = h / w end
    local H, t = sw * aspect, CL.thick / 100             -- the screen's height at the strip's scale; the thickness
    CL.SceneBegin(v)
    CL.Ground(v, CL.map, CL.CROP[CL.map].strip, 0.7)
    local c = CL.LightOf(CL.bar, CL.effect)
    if CL.bar == "neo" then                               -- (always the whole width, standing on the bottom: FrameLayout)
        local band = 0.066 * H * t
        CL.NeoBar(v, 0, sw, sh, band, 0.6, CL.effect, c, 0.085 * H * t, 0.004 * H * t)
        v.size = { 0, sh - band, sw, band }
    else                                                  -- (as the loading screen lays it: its centre 0.075 of the height up)
        local bh = 42 / 720 * H * t
        local bw = sw * CL.width / 100
        if CL.whole then bw = sw elseif CL.width == 0 then bw = sw * CL.OwnWidth() / 100 end
        local x0, top = (sw - bw) / 2, sh - 0.075 * H - bh / 2
        CL.RetroBar(v, x0, top, bw, bh, 0.6, CL.effect, c)
        v.size = { x0, top, bw, bh }
    end
    CL.SceneEnd(v)
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
-- a light's colour from a class's, as the DLL's SaberVivid makes it for the blade: the same hue, richer (its saturation
-- 45% of the way to whole) and as bright as it goes; white stays white
function CL.Vivid(c)
    local hi, lo = math.max(c[1], c[2], c[3]), math.min(c[1], c[2], c[3])
    if hi <= 0 then return { 1, 1, 1 } end
    local sat, k = (hi - lo) / hi, 1
    if sat > 0 then k = 1 + (1 - sat) * 0.45 / sat end
    local out = {}
    for i = 1, 3 do out[i] = math.min(1, math.max(0, (hi - (hi - c[i]) * k) / hi)) end
    return out
end
-- the colour a bar's fill is drawn in, as the loading screen draws it: Retro's Heat and Smooth the colour itself, the
-- blade's light (Neo's, and Blade light on Retro) the colour made vivid
function CL.LightOf(bar, effect)
    local c = CL.FillColour()
    if bar == "neo" or effect == "blade" then return CL.Vivid(c) end
    return c
end

-- greyed: it takes no click, and only its art is dimmed (a slider's track and thumb, a check box's box; its tick shows
-- the client's disabled one), its words as bright as ever (his 7:1: greyed words at half their light measured 6.2:1)
CL.DIM = 0.5
function CL.Grey(c, off)
    local k = 1
    if off then
        k = CL.DIM
        if c.Disable then c:Disable() end
    elseif c.Enable then
        c:Enable()
    end
    if c.GetThumbTexture and c:GetThumbTexture() then
        c:GetThumbTexture():SetVertexColor(k, k, k)
        c:SetBackdropColor(k, k, k)
        c:SetBackdropBorderColor(k, k, k)
    elseif c.GetNormalTexture and c:GetNormalTexture() then
        c:GetNormalTexture():SetVertexColor(k, k, k)
    end
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
        CL.SceneBegin(c.scene)
        c.draw(c.scene)
        CL.SceneEnd(c.scene)
    end
    for _, r in ipairs(CL.ui.radios) do r.button:SetChecked(CL[r.key] == r.value) end
    local neo = CL.bar == "neo"
    for key, s in pairs(CL.ui.sliders) do
        local value = CL[key]
        local text
        if key == "thick" then
            text = tostring(value) .. "%"
        else
            text = tostring(value) .. "% of the screen"
            if value == 0 then value, text = CL.Clamp(CL.OwnWidth(), "width"), "each bar's own" end
            if CL.whole or neo then value, text = 100, "the whole screen" end    -- (Neo: always)
        end
        s.slider:SetValue(value)
        s.value:SetText(text)
    end
    -- the width: Retro's alone; with Neo picked, its slider and the whole width greyed and still, the note saying why
    CL.ui.whole:SetChecked(CL.whole or neo)
    CL.Grey(CL.ui.sliders.width.slider, CL.whole or neo)
    CL.Grey(CL.ui.whole, neo)
    if neo then CL.ui.neoWide:Show() else CL.ui.neoWide:Hide() end
    CL.ui.colour:SetChecked(not CL.ember)
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
    CL.BarFill()
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
