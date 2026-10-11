-- A stubbed 1.12 UI for CinderLoad's world map switches (dll/switches.lua): frames with 1.12's method names only, laid out
-- as the client lays them out (anchors, sizes, scales; GetLeft and GetCenter as it answers them), shown and hidden with
-- their OnShow and OnHide, scripts run with this and arg1, the world map built as WorldMapFrame.xml builds it and the
-- drop-down lists as UIDropDownMenu.xml does. The drop-down functions and the world map's own (ToggleWorldMap,
-- WorldMapFrame_Minimize and _Maximize, the Continent and Zone menus) are either this file's stand-ins, written from the
-- client's behaviour, or, given a folder holding the client's own UIDropDownMenu.lua and WorldMapFrame.lua (read out of
-- its archives by test_switch.c), the client's code itself.
--   local UI = dofile("tests/wow_ui.lua"); local world = UI.Fresh({ ... })
local UI = {}
UI.SW, UI.SH = 768 * 5120 / 2160, 768          -- his screen, 5120 x 2160: 768 units tall at scale 1
UI.widths = {}                                 -- text -> its width in FRIZQT__ at height 10 (else about 5.5 a letter)
local FONTS = {
    GameFontNormal = { size = 12, color = { 1, 0.82, 0 } },
    GameFontNormalSmall = { size = 10, color = { 1, 0.82, 0 } },
    GameFontHighlight = { size = 12, color = { 1, 1, 1 } },
    GameFontHighlightSmall = { size = 10, color = { 1, 1, 1 } },
    GameFontDisableSmall = { size = 10, color = { 0.5, 0.5, 0.5 } },
}
function UI.TextWidth(t, size)
    t = t or ""
    local w = UI.widths[t] or string.len(t) * 5.5
    return w * (size or 10) / 10
end

local world                                    -- the session: UI.Fresh makes it
local R = {}                                   -- the regions' methods
local Meta = { __index = R }
local OURS = "CinderLoad"                      -- our chunk's source name: setters it calls on the game's frames are noted

-- ---- the game's frames, and who changes them ----
local function Note(f, what)
    if not f.blizzard then return end
    local info = debug.getinfo(3, "S")
    if info and info.source == OURS then table.insert(world.touched, (f.name or "?") .. ":" .. what) end
end

local function New(kind, name, parent)
    local f = setmetatable({ kind = kind, name = name, parent = parent, points = {}, w = 0, h = 0, scale = 1, alpha = 1,
                             shown = true, scripts = {}, kids = {}, level = 1, strata = "MEDIUM", mouse = false, id = 0,
                             blizzard = world.building }, Meta)
    if parent then
        table.insert(parent.kids, f)
        f.level = (parent.level or 0) + 1
        f.strata = parent.strata
    end
    if name then world.env[name] = f end
    table.insert(world.made, f)
    return f
end
UI.New = New

-- ---- scripts: 1.12's this and arg1; an error is caught as the client's error handler catches it ----
function UI.Run(f, h, a1, a2)
    local fn = f.scripts[h]
    if not fn then return end
    local env = world.env
    local t, x1, x2 = env.this, env.arg1, env.arg2
    env.this, env.arg1, env.arg2 = f, a1, a2
    local ok, err = pcall(fn)
    env.this, env.arg1, env.arg2 = t, x1, x2
    if not ok then table.insert(world.errors, tostring(err)) end
end

-- ---- shown, visible ----
function R:IsShown() return self.shown and 1 or nil end
function R:IsVisible()
    if not self.shown then return nil end
    if self.parent then return self.parent:IsVisible() end
    return 1
end
local function FireShow(f)
    UI.Run(f, "OnShow")
    for _, k in ipairs(f.kids) do if k.shown then FireShow(k) end end
end
local function FireHide(f)
    UI.Run(f, "OnHide")
    for _, k in ipairs(f.kids) do if k.shown then FireHide(k) end end
end
function R:Show()
    Note(self, "Show")
    if self.shown then return end
    self.shown = true
    if self:IsVisible() then FireShow(self) end
end
function R:Hide()
    Note(self, "Hide")
    if not self.shown then return end
    local was = self:IsVisible()
    self.shown = false
    if was then FireHide(self) end
end
function R:GetParent() return self.parent end
function R:SetParent(p)
    Note(self, "SetParent")
    if type(p) == "string" then p = world.env[p] end
    if self.parent then
        for i, k in ipairs(self.parent.kids) do if k == self then table.remove(self.parent.kids, i); break end end
    end
    self.parent = p
    if p then table.insert(p.kids, self) end
end
function R:GetName() return self.name end
function R:SetID(i) self.id = i end
function R:GetID() return self.id end
function R:SetAlpha(a) Note(self, "SetAlpha"); self.alpha = a end
function R:GetAlpha() return self.alpha end
function R:SetScale(s) Note(self, "SetScale"); self.scale = s end
function R:GetScale() return self.scale end
local function Eff(f)
    local s = 1
    while f do s = s * (f.scale or 1); f = f.parent end
    return s
end
function R:GetEffectiveScale() return Eff(self) end
function R:SetFrameLevel(l) Note(self, "SetFrameLevel"); self.level = l end
function R:GetFrameLevel() return self.level end
function R:SetFrameStrata(s) Note(self, "SetFrameStrata"); self.strata = s end
function R:GetFrameStrata() return self.strata end
function R:SetScript(h, fn) Note(self, "SetScript " .. h); self.scripts[h] = fn end
function R:GetScript(h) return self.scripts[h] end
function R:EnableMouse(on) Note(self, "EnableMouse"); self.mouse = on and on ~= 0 end
function R:IsMouseEnabled() return self.mouse and 1 or nil end
function R:EnableKeyboard(on) self.keyboard = on end
function R:EnableMouseWheel(on) self.wheel = on end
function R:SetMovable(on) Note(self, "SetMovable"); self.movable = on end
function R:RegisterForDrag() end
function R:RegisterForClicks() end
function R:StartMoving() end
function R:StopMovingOrSizing() end
function R:SetClampedToScreen(on) self.clamped = on end
function R:SetToplevel(on) self.toplevel = on end
function R:RegisterEvent(e) self.events = self.events or {}; self.events[e] = 1 end
function R:UnregisterEvent(e) if self.events then self.events[e] = nil end end
function R:UnregisterAllEvents() self.events = {} end
function R:SetHitRectInsets(l, r, t, b) self.hit = { l, r, t, b } end
function R:SetBackdrop(b) self.backdrop = b end
function R:SetBackdropColor() end
function R:SetBackdropBorderColor() end

-- ---- layout ----
local PT = { TOPLEFT = { 0, 1 }, TOP = { 0.5, 1 }, TOPRIGHT = { 1, 1 }, LEFT = { 0, 0.5 }, CENTER = { 0.5, 0.5 },
             RIGHT = { 1, 0.5 }, BOTTOMLEFT = { 0, 0 }, BOTTOM = { 0.5, 0 }, BOTTOMRIGHT = { 1, 0 } }
function R:SetWidth(w) Note(self, "SetWidth"); self.w = w end
function R:SetHeight(h) Note(self, "SetHeight"); self.h = h end
function R:ClearAllPoints() Note(self, "ClearAllPoints"); self.points = {} end
function R:SetPoint(p, a, b, c, d)
    Note(self, "SetPoint")
    if not PT[p] then error("SetPoint: no such point " .. tostring(p)) end
    local rel, relp, x, y
    if type(a) == "number" then x, y = a, b
    elseif type(b) == "number" then rel, x, y = a, b, c
    else rel, relp, x, y = a, b, c, d end
    if type(rel) == "string" then
        local name = rel
        rel = world.env[name]
        if not rel then error("SetPoint: couldn't find region named '" .. name .. "'") end
    end
    if relp and not PT[relp] then error("SetPoint: no such point " .. tostring(relp)) end
    for i, q in ipairs(self.points) do if q.p == p then table.remove(self.points, i); break end end
    table.insert(self.points, { p = p, rel = rel, relp = relp or p, x = x or 0, y = y or 0 })
end
function R:SetAllPoints(rel)
    Note(self, "SetAllPoints")
    if type(rel) == "string" then rel = world.env[rel] end
    self.points = { { p = "TOPLEFT", rel = rel, relp = "TOPLEFT", x = 0, y = 0 },
                    { p = "BOTTOMRIGHT", rel = rel, relp = "BOTTOMRIGHT", x = 0, y = 0 } }
end
function R:GetPoint(i)
    local q = self.points[i or 1]
    if q then return q.p, q.rel or self.parent, q.relp, q.x, q.y end
end
function R:GetNumPoints() return table.getn(self.points) end
local function RawW(f)
    if f.kind == "FontString" and f.w == 0 then return f:GetStringWidth() end
    return f.w
end
local function RawH(f)
    if f.kind == "FontString" and f.h == 0 then return f.size or 10 end
    return f.h
end
-- left, bottom, right, top on the screen (units at scale 1, y up), or nil when it has no place
function UI.Rect(f, depth)
    depth = (depth or 0) + 1
    if depth > 60 then error("an anchor loop at " .. tostring(f.name)) end
    if f == world.screen then return 0, 0, UI.SW, UI.SH end
    if table.getn(f.points) == 0 then return nil end
    local es = Eff(f)
    local W, H = RawW(f) * es, RawH(f) * es
    local L, Rt, HC, T, B, VC
    for _, q in ipairs(f.points) do
        local rel = q.rel or f.parent or world.screen
        local rl, rb, rr, rt = UI.Rect(rel, depth)
        if not rl then return nil end
        local rx = rl + (rr - rl) * PT[q.relp][1] + q.x * es
        local ry = rb + (rt - rb) * PT[q.relp][2] + q.y * es
        local ax, ay = PT[q.p][1], PT[q.p][2]
        if ax == 0 then L = rx elseif ax == 1 then Rt = rx else HC = rx end
        if ay == 0 then B = ry elseif ay == 1 then T = ry else VC = ry end
    end
    local l, r, b, t
    if L and Rt then l, r = L, Rt elseif L then l, r = L, L + W elseif Rt then l, r = Rt - W, Rt elseif HC then l, r = HC - W / 2, HC + W / 2 end
    if B and T then b, t = B, T elseif B then b, t = B, B + H elseif T then b, t = T - H, T elseif VC then b, t = VC - H / 2, VC + H / 2 end
    if not l or not b then return nil end
    return l, b, r, t
end
function R:GetLeft() local l = UI.Rect(self); if l then return l / Eff(self) end end
function R:GetBottom() local _, b = UI.Rect(self); if b then return b / Eff(self) end end
function R:GetRight() local _, _, r = UI.Rect(self); if r then return r / Eff(self) end end
function R:GetTop() local _, _, _, t = UI.Rect(self); if t then return t / Eff(self) end end
function R:GetCenter()
    local l, b, r, t = UI.Rect(self)
    if l then local es = Eff(self); return (l + r) / 2 / es, (b + t) / 2 / es end
end
function R:GetWidth()
    local l, _, r = UI.Rect(self)
    local n = 0
    for _, q in ipairs(self.points) do if PT[q.p][1] ~= 0.5 then n = n + 1 end end
    if l and n >= 2 then return (r - l) / Eff(self) end
    return RawW(self)
end
function R:GetHeight()
    local _, b, _, t = UI.Rect(self)
    local n = 0
    for _, q in ipairs(self.points) do if PT[q.p][2] ~= 0.5 then n = n + 1 end end
    if b and n >= 2 then return (t - b) / Eff(self) end
    return RawH(self)
end

-- ---- textures and font strings ----
function R:CreateTexture(name, layer)
    local t = New("Texture", name, self)
    t.layer = layer
    return t
end
function R:SetTexture(p) Note(self, "SetTexture"); self.texture = p end
function R:GetTexture() return self.texture end
function R:SetTexCoord(a, b, c, d) self.texcoord = { a, b, c, d } end
function R:SetVertexColor(r, g, b, a) self.vertex = { r, g, b, a } end
function R:SetBlendMode(m) self.blend = m end
function R:CreateFontString(name, layer, template)
    if template and not FONTS[template] then error("no such font: " .. tostring(template)) end
    local fs = New("FontString", name, self)
    fs.layer = layer
    UI.Font(fs, template or "GameFontNormal")
    return fs
end
function UI.Font(fs, template)
    local F = FONTS[template]
    fs.font, fs.size = template, F.size
    fs.color = { F.color[1], F.color[2], F.color[3] }
end
function R:SetText(t)
    Note(self, "SetText")
    if self.kind == "Button" then
        if self.textRegion then self.textRegion:SetText(t) else self.text = t end
        return
    end
    self.text = t
end
function R:GetText()
    if self.kind == "Button" and self.textRegion then return self.textRegion.text end
    return self.text
end
function R:GetStringWidth() return UI.TextWidth(self.text, self.size) end
function R:SetJustifyH(j) self.justifyH = j end
function R:SetJustifyV(j) self.justifyV = j end
function R:SetTextColor(r, g, b)
    if self.kind == "Button" then if self.textRegion then self.textRegion.color = { r, g, b } end; return end
    self.color = { r, g, b }
end
function R:SetFont(file, size)
    if self.kind == "Button" then if self.textRegion and size then self.textRegion.size = size end; return end
    self.fontFile = file
    if size then self.size = size end
end
function R:GetFont() return "Fonts\\FRIZQT__.TTF", self.size or 10, "" end
-- ---- buttons ----
function R:SetHighlightTextColor(r, g, b) self.highlightColor = { r, g, b } end
function R:SetDisabledTextColor(r, g, b) self.disabledColor = { r, g, b } end
function R:Enable() self.disabled = nil end
function R:Disable() self.disabled = 1 end
function R:IsEnabled() return not self.disabled and 1 or nil end
function R:LockHighlight() self.lockedHighlight = 1 end
function R:UnlockHighlight() self.lockedHighlight = nil end
function R:SetNormalTexture(t) self.normalTexture = t end
function R:SetPushedTexture(t) self.pushedTexture = t end
function R:SetHighlightTexture(t) self.highlightTexture = t end
function R:SetDisabledTexture(t) self.disabledTexture = t end
function R:SetChecked(c) if c == 0 or c == false then c = nil end; self.checked = c end
function R:GetChecked() return self.checked end

-- ---- the client's templates, as its XML builds them ----
local function Tex(parent, name, file, w, h)
    local t = parent:CreateTexture(name, "ARTWORK")
    t.texture, t.w, t.h = file, w, h
    return t
end
-- UIDropDownMenuTemplate (UIDropDownMenuTemplates.xml)
function UI.DropDownTemplate(f)
    local n = f.name
    f.w, f.h = 40, 32
    local L = Tex(f, n .. "Left", "Interface\\Glues\\CharacterCreate\\CharacterCreate-LabelFrame", 25, 64)
    L.points = { { p = "TOPLEFT", rel = f, relp = "TOPLEFT", x = 0, y = 17 } }
    L.texcoord = { 0, 0.1953125, 0, 1 }
    local M = Tex(f, n .. "Middle", L.texture, 115, 64)
    M.points = { { p = "LEFT", rel = L, relp = "RIGHT", x = 0, y = 0 } }
    M.texcoord = { 0.1953125, 0.8046875, 0, 1 }
    local Rt = Tex(f, n .. "Right", L.texture, 25, 64)
    Rt.points = { { p = "LEFT", rel = M, relp = "RIGHT", x = 0, y = 0 } }
    Rt.texcoord = { 0.8046875, 1, 0, 1 }
    local T = f:CreateFontString(n .. "Text", "ARTWORK", "GameFontHighlightSmall")
    T.justifyH, T.h = "RIGHT", 10
    T.points = { { p = "RIGHT", rel = Rt, relp = "RIGHT", x = -43, y = 2 } }
    local b = New("Button", n .. "Button", f)
    b.w, b.h, b.mouse = 24, 24, true
    b.points = { { p = "TOPRIGHT", rel = Rt, relp = "TOPRIGHT", x = -16, y = -18 } }
    b.normalTexture = "Interface\\ChatFrame\\UI-ChatIcon-ScrollDown-Up"
    local nt = Tex(b, n .. "ButtonNormalTexture", "Interface\\ChatFrame\\UI-ChatIcon-ScrollDown-Up", 24, 24)
    nt.points = { { p = "RIGHT", rel = b, relp = "RIGHT", x = 0, y = 0 } }
    for _, s in ipairs({ "Pushed", "Disabled", "Highlight" }) do
        local x = Tex(b, n .. "Button" .. s .. "Texture", "", 24, 24)
        x.shown = false
    end
    local env = world.env
    b.scripts.OnClick = function() env.ToggleDropDownMenu(); env.PlaySound("igMainMenuOptionCheckBoxOn") end
    f.scripts.OnHide = function() env.CloseDropDownMenus() end
end
-- UIDropDownListTemplate and its 40 UIDropDownMenuButtonTemplate buttons (UIDropDownMenu.xml)
local function DropDownList(i)
    local env = world.env
    local l = New("Button", "DropDownList" .. i, nil)
    l.w, l.h, l.id, l.strata, l.toplevel, l.mouse, l.shown = 180, 10, i, "FULLSCREEN_DIALOG", 1, true, false
    l.level = 1
    local bd = New("Frame", l.name .. "Backdrop", l)
    bd.points = { { p = "TOPLEFT", rel = l, relp = "TOPLEFT", x = 0, y = 0 }, { p = "BOTTOMRIGHT", rel = l, relp = "BOTTOMRIGHT", x = 0, y = 0 } }
    local mb = New("Frame", l.name .. "MenuBackdrop", l)
    mb.points = bd.points
    for j = 1, 40 do
        local b = New("Button", l.name .. "Button" .. j, l)
        b.w, b.h, b.id, b.mouse = 128, 16, j, true
        local hl = Tex(b, b.name .. "Highlight", "Interface\\QuestFrame\\UI-QuestTitleHighlight", 0, 0)
        hl.points = { { p = "TOPLEFT", rel = b, relp = "TOPLEFT", x = 0, y = 0 }, { p = "BOTTOMRIGHT", rel = b, relp = "BOTTOMRIGHT", x = 0, y = 0 } }
        hl.shown = false
        local ck = Tex(b, b.name .. "Check", "Interface\\Buttons\\UI-CheckBox-Check", 24, 24)
        ck.points = { { p = "LEFT", rel = b, relp = "LEFT", x = 0, y = 0 } }
        local ic = Tex(b, b.name .. "Icon", nil, 15, 15)
        ic.shown = false
        local sw = New("Button", b.name .. "ColorSwatch", b)
        sw.w, sw.h, sw.shown = 16, 16, false
        Tex(sw, sw.name .. "NormalTexture", "Interface\\ChatFrame\\ChatFrameColorSwatch", 16, 16)
        local ea = New("Button", b.name .. "ExpandArrow", b)
        ea.w, ea.h, ea.shown = 16, 16, false
        local ib = New("Button", b.name .. "InvisibleButton", b)
        ib.shown = false
        local nt = b:CreateFontString(b.name .. "NormalText", "ARTWORK", "GameFontHighlightSmall")
        nt.justifyH = "LEFT"
        nt.points = { { p = "CENTER", rel = b, relp = "CENTER", x = 0, y = 0 } }
        b.textRegion = nt
        b.shown = false
        b.scripts.OnClick = function() env.UIDropDownMenuButton_OnClick() end
        b.scripts.OnEnter = function()
            local this = env.this
            if this.hasArrow then env.ToggleDropDownMenu(this:GetParent():GetID() + 1, this.value)
            else env.CloseDropDownMenus(this:GetParent():GetID() + 1) end
            env[this:GetName() .. "Highlight"]:Show()
            env.UIDropDownMenu_StopCounting(this:GetParent())
            if this.tooltipTitle then env.GameTooltip_AddNewbieTip(this.tooltipTitle, 1.0, 1.0, 1.0, this.tooltipText, 1) end
        end
        b.scripts.OnLeave = function()
            local this = env.this
            env[this:GetName() .. "Highlight"]:Hide()
            env.UIDropDownMenu_StartCounting(this:GetParent())
            env.GameTooltip:Hide()
        end
    end
    l.scripts.OnClick = function() env.this:Hide() end
    l.scripts.OnEnter = function() env.UIDropDownMenu_StopCounting(env.this) end
    l.scripts.OnLeave = function() env.UIDropDownMenu_StartCounting(env.this) end
    l.scripts.OnUpdate = function() env.UIDropDownMenu_OnUpdate(env.arg1) end
    l.scripts.OnShow = function()
        local this = env.this
        for k = 1, env.UIDROPDOWNMENU_MAXBUTTONS do
            if not this.noResize then env[this:GetName() .. "Button" .. k]:SetWidth(this.maxWidth) end
        end
        if not this.noResize then this:SetWidth(this.maxWidth + 15) end
        this.showTimer = nil
        if this:GetID() > 1 then this.parent = env["DropDownList" .. (this:GetID() - 1)] end
    end
    l.scripts.OnHide = function()
        if i < 3 then env.CloseDropDownMenus(i + 1) end
        env.wipe(env.OPEN_DROPDOWNMENUS[i])
    end
    return l
end

-- ---- this file's stand-ins for the client's drop-down functions (its UIDropDownMenu.lua's behaviour) ----
local function StubDropDowns(env)
    env.UIDROPDOWNMENU_MAXBUTTONS, env.UIDROPDOWNMENU_MAXLEVELS = 40, 3
    env.UIDROPDOWNMENU_BUTTON_HEIGHT, env.UIDROPDOWNMENU_BORDER_HEIGHT = 16, 15
    env.UIDROPDOWNMENU_MENU_LEVEL, env.UIDROPDOWNMENU_SHOW_TIME = 1, 2
    env.OPEN_DROPDOWNMENUS = { {}, {}, {} }
    local G = env
    function env.UIDropDownMenu_Initialize(frame, init, displayMode, level)
        frame = frame or G.this
        if frame:GetName() ~= G.UIDROPDOWNMENU_OPEN_MENU then G.UIDROPDOWNMENU_MENU_LEVEL = 1 end
        G.UIDROPDOWNMENU_INIT_MENU = frame:GetName()
        for i = 1, 3 do
            local list = G["DropDownList" .. i]
            if i >= G.UIDROPDOWNMENU_MENU_LEVEL or frame:GetName() ~= G.UIDROPDOWNMENU_OPEN_MENU then
                list.numButtons, list.maxWidth = 0, 0
                for j = 1, 40 do G["DropDownList" .. i .. "Button" .. j]:Hide() end
            end
        end
        frame:SetHeight(32)
        if init then frame.initialize = init; init(level) end
    end
    function env.UIDropDownMenu_AddButton(info, level)
        level = level or 1
        local list = G["DropDownList" .. level]
        local index = list.numButtons + 1
        if index > 40 then error("Too many buttons in UIDropDownMenu") end
        list.numButtons = index
        local b = G[list:GetName() .. "Button" .. index]
        local nt = G[b:GetName() .. "NormalText"]
        b:Enable()
        if info.text then
            b:SetText(info.text)
            b:SetFont("Fonts\\FRIZQT__.TTF", G.UIDROPDOWNMENU_DEFAULT_TEXT_HEIGHT)
            local width = nt:GetWidth() + 60
            if info.notCheckable then width = width - 30 end
            if width > list.maxWidth then list.maxWidth = width end
        else
            b:SetText("")
        end
        for _, k in ipairs({ "func", "owner", "keepShownOnClick", "tooltipTitle", "tooltipText", "arg1", "arg2", "hasArrow", "notCheckable" }) do
            b[k] = info[k]
        end
        b.value = info.value or info.text
        nt:ClearAllPoints()
        if info.notCheckable then nt:SetPoint("LEFT", b, "LEFT", 0, 0) else nt:SetPoint("LEFT", b, "LEFT", 27, 0) end
        local x = info.notCheckable and 15 or 17
        b:SetPoint("TOPLEFT", b:GetParent(), "TOPLEFT", x, -((b:GetID() - 1) * 16) - 15)
        local frame = G[G.UIDROPDOWNMENU_OPEN_MENU or ""] or G[G.UIDROPDOWNMENU_INIT_MENU or ""]
        if frame then
            if frame.selectedName then if b:GetText() == frame.selectedName then info.checked = 1 end
            elseif frame.selectedID then if b:GetID() == frame.selectedID then info.checked = 1 end
            elseif frame.selectedValue then if b.value == frame.selectedValue then info.checked = 1 end end
        end
        if info.checked then b:LockHighlight(); G[b:GetName() .. "Check"]:Show()
        else b:UnlockHighlight(); G[b:GetName() .. "Check"]:Hide() end
        b.checked = info.checked
        if info.disabled or info.isTitle or info.notClickable then b:Disable() end
        list:SetHeight(index * 16 + 30)
        b:Show()
    end
    function env.UIDropDownMenu_Refresh(frame, useValue, level)
        level = level or G.UIDROPDOWNMENU_MENU_LEVEL
        local most = 0
        for i = 1, 40 do
            local b = G["DropDownList" .. level .. "Button" .. i]
            local on
            if frame.selectedName then on = b:GetText() == frame.selectedName
            elseif frame.selectedID then on = b:GetID() == frame.selectedID
            elseif frame.selectedValue then on = b.value == frame.selectedValue end
            if on then
                G.UIDropDownMenu_SetText(useValue and b.value or b:GetText(), frame)
                b:LockHighlight(); G[b:GetName() .. "Check"]:Show()
            else
                b:UnlockHighlight(); G[b:GetName() .. "Check"]:Hide()
            end
            if b:IsShown() then
                local w = G[b:GetName() .. "NormalText"]:GetWidth() + 60
                if w > most then most = w end
            end
        end
        for i = 1, 40 do G["DropDownList" .. level .. "Button" .. i]:SetWidth(most) end
        G["DropDownList" .. level]:SetWidth(most + 15)
    end
    function env.UIDropDownMenu_SetSelectedValue(frame, value, useValue)
        frame.selectedName, frame.selectedID, frame.selectedValue = nil, nil, value
        G.UIDropDownMenu_Refresh(frame, useValue)
    end
    function env.UIDropDownMenu_SetSelectedID(frame, id, useValue)
        frame.selectedName, frame.selectedID, frame.selectedValue = nil, id, nil
        G.UIDropDownMenu_Refresh(frame, useValue)
    end
    function env.UIDropDownMenu_GetSelectedValue(frame) return frame.selectedValue end
    function env.UIDropDownMenu_GetSelectedID(frame) return frame.selectedID end
    function env.UIDropDownMenu_GetSelectedName(frame) return frame.selectedName end
    function env.UIDropDownMenu_SetWidth(width, frame)
        frame = frame or G.this
        G[frame:GetName() .. "Middle"]:SetWidth(width)
        frame:SetWidth(width + 50)
        G[frame:GetName() .. "Text"]:SetWidth(width - 25)
        frame.noResize = 1
    end
    function env.UIDropDownMenu_SetText(text, frame) G[(frame or G.this):GetName() .. "Text"]:SetText(text) end
    function env.UIDropDownMenu_GetText(frame) return G[(frame or G.this):GetName() .. "Text"]:GetText() end
    function env.UIDropDownMenu_ClearAll(frame)
        frame.selectedID, frame.selectedName, frame.selectedValue = nil, nil, nil
        G.UIDropDownMenu_SetText("", frame)
    end
    function env.UIDropDownMenu_OnUpdate(elapsed)
        local this = G.this
        if not this.showTimer or not this.isCounting then return
        elseif this.showTimer < 0 then this:Hide(); this.showTimer = nil; this.isCounting = nil
        else this.showTimer = this.showTimer - elapsed end
    end
    function env.UIDropDownMenu_StartCounting(frame)
        if frame.parent then G.UIDropDownMenu_StartCounting(frame.parent) else frame.showTimer = 2; frame.isCounting = 1 end
    end
    function env.UIDropDownMenu_StopCounting(frame)
        if frame.parent then G.UIDropDownMenu_StopCounting(frame.parent) else frame.isCounting = nil end
    end
    function env.UIDropDownMenuButton_OnClick()
        local this = G.this
        if not this.func then return end
        this.func(this.arg1, this.arg2)
        if this.keepShownOnClick then
            if this.checked then G[this:GetName() .. "Check"]:Hide(); this.checked = nil
            else G[this:GetName() .. "Check"]:Show(); this.checked = 1 end
        else
            this:GetParent():Hide()
        end
        G.PlaySound("UChatScrollButton")
    end
    function env.HideDropDownMenu(level) G["DropDownList" .. level]:Hide() end
    function env.CloseDropDownMenus(level)
        for i = level or 1, 3 do G["DropDownList" .. i]:Hide() end
    end
    function env.ToggleDropDownMenu(level, value, dropDownFrame, anchorName, xOffset, yOffset)
        level = level or 1
        G.UIDROPDOWNMENU_MENU_LEVEL, G.UIDROPDOWNMENU_MENU_VALUE = level, value
        local list = G["DropDownList" .. level]
        local temp = dropDownFrame or G.this:GetParent()
        if list:IsVisible() and G.UIDROPDOWNMENU_OPEN_MENU == temp:GetName() then list:Hide(); return end
        local s = 1
        if G.GetCVar("useUiScale") == "1" and temp ~= G.WorldMapContinentDropDown and temp ~= G.WorldMapZoneDropDown then
            s = tonumber(G.GetCVar("uiscale"))
        end
        list:SetScale(s)
        list:Hide()
        dropDownFrame = dropDownFrame or G.this:GetParent()
        G.UIDROPDOWNMENU_OPEN_MENU = dropDownFrame:GetName()
        list:ClearAllPoints()
        list:SetPoint("TOPLEFT", anchorName or (G.UIDROPDOWNMENU_OPEN_MENU .. "Left"), "BOTTOMLEFT", xOffset or 8, yOffset or 22)
        G[list:GetName() .. "Backdrop"]:Show()
        G[list:GetName() .. "MenuBackdrop"]:Hide()
        G.UIDropDownMenu_Initialize(dropDownFrame, dropDownFrame.initialize, nil, level)
        if list.numButtons == 0 then return end
        list:Show()
        local x, y = list:GetCenter()
        if not x or not y then list:Hide(); return end
    end
end

-- ---- this file's stand-ins for the world map's own functions (its WorldMapFrame.lua's, in its order) ----
local function StubWorldMap(env)
    local G = env
    env.WORLDMAP_WINDOWED = env.WORLDMAP_WINDOWED or 0
    env.WORLDMAP_WINDOWED_SCALE = 0.7
    function env.ToggleWorldMap()
        if G.WorldMapFrame:IsVisible() then G.HideUIPanel(G.WorldMapFrame) else G.ShowUIPanel(G.WorldMapFrame) end
    end
    function env.WorldMapFrame_Update() G.WorldMapFrameTitle:SetText(G.WORLD_MAP) end
    function env.WorldMapFrame_Minimize()
        if G.WORLDMAP_WINDOWED == 0 then G.ToggleWorldMap() end      -- (the map closed first: its OnHide)
        G.WorldMapFrame:SetParent(G.UIParent)
        G.WorldMapFrame:SetHeight(521)
        G.WorldMapFrame:SetWidth(720)
        G.WorldMapFrame:SetFrameLevel(0)
        G.WorldMapPositioningGuide:ClearAllPoints()
        G.WorldMapPositioningGuide:SetPoint("TOPLEFT", G.WorldMapFrame, "TOPLEFT", 0, 1)
        G.WorldMapPositioningGuide:SetPoint("BOTTOMRIGHT", G.WorldMapFrame, "BOTTOMRIGHT", 1, 0)
        G.WorldMapDetailFrame:SetScale(G.WORLDMAP_WINDOWED_SCALE)
        G.WorldMapContinentDropDown:SetAlpha(0)
        G.WorldMapContinentDropDownButton:EnableMouse(0)
        G.WorldMapZoneDropDown:SetAlpha(0)
        G.WorldMapZoneDropDownButton:EnableMouse(0)
        if G.WORLDMAP_WINDOWED == 0 then G.ToggleWorldMap() end      -- (shown again before WORLDMAP_WINDOWED says so)
        G.WORLDMAP_WINDOWED = 1
        G.WorldMapFrame:ClearAllPoints()
        G.WorldMapFrame:SetPoint("CENTER", G.UIParent, 0, 20)
        G.WorldMapFrame_Update()
    end
    function env.WorldMapFrame_Maximize()
        G.ToggleWorldMap()
        G.WorldMapFrame:SetParent(nil)
        G.WorldMapFrame:ClearAllPoints()
        G.WorldMapFrame:SetAllPoints()
        G.WorldMapPositioningGuide:ClearAllPoints()
        G.WorldMapPositioningGuide:SetPoint("CENTER", G.WorldMapFrame)
        G.WorldMapDetailFrame:SetScale(1)
        G.WorldMapContinentDropDown:SetAlpha(1)
        G.WorldMapContinentDropDownButton:EnableMouse(1)
        G.WorldMapZoneDropDown:SetAlpha(1)
        G.WorldMapZoneDropDownButton:EnableMouse(1)
        G.WORLDMAP_WINDOWED = 0
        G.ToggleWorldMap()
        G.WorldMapFrame_Update()
    end
    function env.WorldMapContinentsDropDown_OnLoad()
        G.UIDropDownMenu_Initialize(G.this, G.WorldMapContinentsDropDown_Initialize)
        G.UIDropDownMenu_SetWidth(130)
    end
    function env.WorldMapContinentsDropDown_Initialize()
        local names = { G.GetMapContinents() }
        for i, n in ipairs(names) do
            G.UIDropDownMenu_AddButton({ text = n, func = G.WorldMapContinentButton_OnClick,
                                         checked = G.UIDropDownMenu_GetSelectedID(G.WorldMapContinentDropDown) == i })
        end
    end
    function env.WorldMapZoneDropDown_OnLoad()
        G.UIDropDownMenu_Initialize(G.this, G.WorldMapZoneDropDown_Initialize)
        G.UIDropDownMenu_SetWidth(130)
    end
    function env.WorldMapZoneDropDown_Initialize()
        local names = { G.GetMapZones(G.GetCurrentMapContinent()) }
        for i, n in ipairs(names) do
            G.UIDropDownMenu_AddButton({ text = n, func = G.WorldMapZoneButton_OnClick,
                                         checked = G.UIDropDownMenu_GetSelectedID(G.WorldMapZoneDropDown) == i })
        end
    end
    function env.WorldMapContinentButton_OnClick()
        G.UIDropDownMenu_SetSelectedID(G.WorldMapContinentDropDown, G.this:GetID())
        G.SetMapZoom(G.this:GetID())
    end
    function env.WorldMapZoneButton_OnClick()
        G.UIDropDownMenu_SetSelectedID(G.WorldMapZoneDropDown, G.this:GetID())
        G.SetMapZoom(G.GetCurrentMapContinent(), G.this:GetID())
    end
end

-- ---- the world map, as WorldMapFrame.xml builds it (the whole screen's: WorldMapFrame_Maximize's state) ----
local function WorldMapFrames(env)
    local wm = New("Frame", "WorldMapFrame", nil)
    wm.strata, wm.shown, wm.mouse = "FULLSCREEN", false, true
    wm:SetAllPoints()
    local black = Tex(wm, "BlackoutWorld", nil, 1024, 768)
    black.points = { { p = "BOTTOMLEFT", rel = wm, relp = "BOTTOMLEFT", x = 0, y = 0 } }
    for i = 1, 12 do
        local t = Tex(wm, "WorldMapFrameTexture" .. i, nil, 256, 256)
        t.points = { { p = "TOPLEFT", rel = wm, relp = "CENTER", x = -512 + 256 * math.fmod(i - 1, 4), y = 384 - 256 * math.floor((i - 1) / 4) } }
    end
    for i = 1, 8 do local t = Tex(wm, "WorldMapFrameMinimizedTexture" .. i, nil, 32, 32); t.shown = false end
    local title = wm:CreateFontString("WorldMapFrameTitle", "ARTWORK", "GameFontNormal")
    title.text = env.WORLD_MAP
    title.points = { { p = "TOP", rel = wm, relp = "TOP", x = 0, y = -5 } }
    local guide = New("Frame", "WorldMapPositioningGuide", wm)
    guide.w, guide.h = 1024, 768
    guide.points = { { p = "CENTER", rel = wm, relp = "CENTER", x = 0, y = 0 } }
    for _, d in ipairs({ { "WorldMapContinentDropDown", 1, "Continent" }, { "WorldMapZoneDropDown", 2, "Zone" } }) do
        local f = New("Frame", d[1], wm)
        f.id = d[2]
        UI.DropDownTemplate(f)
        local label = f:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
        label.text = d[3]
        label.points = { { p = "TOPLEFT", rel = f, relp = "TOPLEFT", x = 20, y = 10 } }
        f.label = label
    end
    env.WorldMapContinentDropDown.points = { { p = "TOP", rel = guide, relp = "TOP", x = -80, y = -35 } }
    env.WorldMapZoneDropDown.points = { { p = "LEFT", rel = env.WorldMapContinentDropDown, relp = "RIGHT", x = -33, y = 0 } }
    env.WorldMapContinentDropDown.scripts.OnShow = function() env.WorldMapContinentsDropDown_OnLoad() end
    env.WorldMapZoneDropDown.scripts.OnShow = function() env.WorldMapZoneDropDown_OnLoad() end
    local zoom = New("Button", "WorldMapZoomOutButton", wm)
    zoom.w, zoom.h = 110, 22
    zoom.points = { { p = "LEFT", rel = env.WorldMapZoneDropDown, relp = "RIGHT", x = -10, y = 3 } }
    for _, n in ipairs({ "WorldMapMagnifyingGlassButton", "WorldMapFrameMinimizeButton", "WorldMapFrameMaximizeButton",
                         "WorldMapFrameCloseButton" }) do
        local b = New("Button", n, wm)
        b.w, b.h = 32, 32
    end
    env.WorldMapFrameMaximizeButton.shown = false
    local detail = New("Frame", "WorldMapDetailFrame", wm)
    detail.w, detail.h = 1002, 668
    detail.points = { { p = "TOPLEFT", rel = guide, relp = "TOP", x = -502, y = -69 } }
    for i = 1, 12 do Tex(detail, "WorldMapDetailTile" .. i, nil, 256, 256) end
    local button = New("Button", "WorldMapButton", detail)
    button.w, button.h = 1002, 668
    button.points = { { p = "TOPLEFT", rel = detail, relp = "TOPLEFT", x = 0, y = 0 } }
    New("Frame", "WorldMapTooltip", nil).shown = false
end

-- ---- a session: env (the client's globals, 5.0's library only), the screen, the UI ----
function UI.Fresh(opts)
    opts = opts or {}
    world = { env = {}, touched = {}, made = {}, chat = {}, sounds = {}, cvars = {}, errors = {}, building = true }
    local env = world.env
    for _, k in ipairs({ "assert", "error", "ipairs", "pairs", "next", "pcall", "rawget", "rawset", "setmetatable",
                         "getmetatable", "tonumber", "tostring", "type", "unpack", "xpcall" }) do env[k] = _G[k] end
    env.string = { find = string.find, format = string.format, gsub = string.gsub, lower = string.lower, upper = string.upper,
                   len = string.len, sub = string.sub, rep = string.rep, byte = string.byte, char = string.char, gfind = string.gmatch }
    env.table = { insert = table.insert, remove = table.remove, getn = table.getn, concat = table.concat, sort = table.sort,
                  setn = function() end }
    env.math = { floor = math.floor, ceil = math.ceil, abs = math.abs, max = math.max, min = math.min, mod = math.fmod }
    env._G = env
    env.getglobal = function(n) return env[n] end
    env.setglobal = function(n, v) env[n] = v end
    env.strlower, env.strsub, env.strlen, env.strfind = string.lower, string.sub, string.len, string.find
    env.tinsert, env.tremove, env.getn = table.insert, table.remove, table.getn
    env.mod, env.floor, env.ceil, env.abs = math.fmod, math.floor, math.ceil, math.abs
    env.wipe = function(t) for k in pairs(t) do t[k] = nil end; return t end
    env.tContains = function(t, x) for _, v in pairs(t) do if v == x then return 1 end end end
    env.message = function(m) error("message(): " .. tostring(m)) end
    env.CreateFrame = function(kind, name, parent, template)
        if kind ~= "Frame" and kind ~= "Button" and kind ~= "CheckButton" then error("CreateFrame: " .. tostring(kind)) end
        if template and not name then error("a template's $parent names need a name") end
        if type(parent) == "string" then parent = env[parent] end
        local f = New(kind, name, parent)
        world.creates = (world.creates or 0) + 1
        if template == "UIDropDownMenuTemplate" then UI.DropDownTemplate(f)
        elseif template then error("no such template here: " .. tostring(template)) end
        return f
    end
    env.PlaySound = function(s) table.insert(world.sounds, s) end
    env.SetCVar = function(n, v)
        if world.cvars[n] == nil then error("Couldn't find CVar named '" .. tostring(n) .. "'") end
        world.cvars[n] = tostring(v)
    end
    env.GetCVar = function(n)
        if world.cvars[n] == nil then error("Couldn't find CVar named '" .. tostring(n) .. "'") end
        return world.cvars[n]
    end
    world.cvars.useUiScale = opts.useUiScale or "1"
    world.cvars.uiscale = opts.uiScale or "0.82"
    if opts.cvar ~= false then world.cvars.CinderLoadStyle = opts.cvar or "parchment retro" end
    env.SlashCmdList = {}
    env.GRAY_FONT_COLOR = { r = 0.5, g = 0.5, b = 0.5 }
    env.HIGHLIGHT_FONT_COLOR = { r = 1, g = 1, b = 1 }
    env.NORMAL_FONT_COLOR = { r = 1, g = 0.82, b = 0 }
    env.STANDARD_TEXT_FONT = "Fonts\\FRIZQT__.TTF"
    env.WORLD_MAP = "World Map"
    env.CONTINENTS_LENGTH = 2
    env.SHOW_NEWBIE_TIPS = opts.newbie or "1"
    env.UISpecialFrames, env.UIMenus = {}, {}
    env.UIPanelWindows = { WorldMapFrame = { area = "full", pushable = 0 } }
    env.ShowUIPanel = function(f) if f then f:Show() end end
    env.HideUIPanel = function(f) if f then f:Hide() end end
    env.GetCursorPosition = function() return 0, 0 end
    -- the map's data
    local map = { c = 2, z = 0 }
    world.map = map
    env.GetMapContinents = function() return "Kalimdor", "Eastern Kingdoms" end
    env.GetMapZones = function(c)
        if c == 1 then return "Ashenvale", "Darkshore" elseif c == 2 then return "Elwynn Forest", "Westfall" end
    end
    env.SetMapZoom = function(c, z) map.c, map.z = c, z or 0 end
    env.GetCurrentMapContinent = function() return map.c end
    env.GetCurrentMapZone = function() return map.z end
    env.GetMapInfo = function() return "Azeroth", 768 end
    env.GetNumMapLandmarks = function() return 0 end
    env.GetNumMapOverlays = function() return 0 end
    env.SetMapToCurrentZone = function() end
    -- the screen and UIParent
    world.screen = setmetatable({ name = "(the screen)", points = {}, kids = {}, scale = 1, shown = true }, Meta)
    env.UIParent = New("Frame", "UIParent", nil)
    env.UIParent.scale = (env.GetCVar("useUiScale") == "1") and tonumber(env.GetCVar("uiscale")) or 1
    env.UIParent:SetAllPoints()
    env.GetScreenWidth = function() return env.UIParent:GetWidth() end
    env.GetScreenHeight = function() return env.UIParent:GetHeight() end
    -- the chat and the tooltip
    env.DEFAULT_CHAT_FRAME = New("Frame", "ChatFrame1", env.UIParent)
    env.DEFAULT_CHAT_FRAME.AddMessage = function(self, t, r, g, b) table.insert(world.chat, { text = t, r = r, g = g, b = b }) end
    local tip = New("Frame", "GameTooltip", env.UIParent)
    tip.shown, tip.lines = false, {}
    function tip:SetOwner(o, a) if world.tipFails then error("tooltip broke") end; self.owner = o; self.anchor = a; self.lines = {} end
    function tip:SetText(t) self.lines = { t } end
    function tip:AddLine(t) table.insert(self.lines, t) end
    env.GameTooltip_SetDefaultAnchor = function(t, owner) t:SetOwner(owner, "ANCHOR_NONE") end
    env.GameTooltip_AddNewbieTip = function(normal, r, g, b, newbie, noNormal)
        if env.SHOW_NEWBIE_TIPS == "1" then
            env.GameTooltip_SetDefaultAnchor(env.GameTooltip, env.this)
            if normal then env.GameTooltip:SetText(normal, r, g, b); env.GameTooltip:AddLine(newbie, 1, 0.82, 0, 1)
            else env.GameTooltip:SetText(newbie, r, g, b, 1, 1) end
            env.GameTooltip:Show()
        elseif not noNormal then
            env.GameTooltip:SetOwner(env.this, "ANCHOR_RIGHT")
            env.GameTooltip:SetText(normal, r, g, b)
        end
    end
    -- the drop-down functions and lists; the world map's functions and frames (the client's own code when given)
    StubDropDowns(env)
    StubWorldMap(env)
    world.real = nil
    if opts.frameXml then
        local loaded = 0
        for _, f in ipairs({ "UIDropDownMenu.lua", "WorldMapFrame.lua" }) do
            local fh = io.open(opts.frameXml .. "/" .. f, "rb")
            if fh then
                local src = fh:read("*a")
                fh:close()
                local fn, err = loadstring(src, "@Interface\\FrameXML\\" .. f)
                if not fn then error(f .. ": " .. tostring(err)) end
                setfenv(fn, env)
                fn()
                loaded = loaded + 1
            end
        end
        world.real = loaded == 2
    end
    for i = 1, 3 do DropDownList(i) end
    env.UIDROPDOWNMENU_DEFAULT_TEXT_HEIGHT = 10                      -- (DropDownList1's OnLoad: its buttons' font height)
    WorldMapFrames(env)
    if opts.noMap then env.WorldMapFrame = nil; env.WorldMapPositioningGuide = nil end
    if opts.noGuide then env.WorldMapPositioningGuide = nil end
    world.building = false
    return world
end

-- ---- driving it, as a player would ----
function UI.Click(b, button)                -- the client's click: only on a visible, mouse-enabled button
    if not (b:IsVisible() and b.mouse and not b.disabled) then return false end
    if b.kind == "CheckButton" then if b.checked then b.checked = nil else b.checked = 1 end end
    UI.Run(b, "OnClick", button or "LeftButton")
    return true
end
function UI.Enter(f) UI.Run(f, "OnEnter") end
function UI.Leave(f) UI.Run(f, "OnLeave") end
function UI.Frame(elapsed)                  -- one frame: OnUpdate on every visible frame
    local list = {}
    for _, f in ipairs(world.made) do if f.scripts and f.scripts.OnUpdate and f:IsVisible() then table.insert(list, f) end end
    for _, f in ipairs(list) do UI.Run(f, "OnUpdate", elapsed or 0.016) end
end
function UI.Run50(src, name)               -- a chunk as FrameScript_Execute runs it, in this session
    local fn, err = loadstring(src, name or OURS)
    if not fn then return false, err end
    setfenv(fn, world.env)
    return pcall(fn)
end
function UI.World() return world end
-- where a region is, in the world map box's units (its 1024 x 768, from its top left, y down), as the mock measures
function UI.Box(f)
    local l, b, r, t = UI.Rect(f)
    if not l then return nil end
    local gl, gb, gr, gt = UI.Rect(world.env.WorldMapPositioningGuide)
    local k = (gr - gl) / 1024
    return (l - gl) / k, (gt - t) / k, (r - gl) / k, (gt - b) / k
end
return UI
