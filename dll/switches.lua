-- CinderLoad's two choices, on the world map (M) and as /cinderload (his decisions, 10 Oct 2026): the loading screen's
-- MAP (Parchment | Satellite) and its BAR (Retro | Neo), each on its own. His form (C, picked from four drawn): two
-- drop-downs made exactly as the world map's own Continent and Zone ones are (UIDropDownMenuTemplate, the client's own
-- UIDropDownMenu functions), gold labels "Loading map" and "Loading bar" over them, at the left end of the map's top
-- bar, left of Continent, covering nothing. Run inside the game by CinderLoad.dll, on the game's own thread while no Lua
-- is running (no add-on folder: the release is the DLL and its packs). The DLL puts one line before this text, the
-- choice it holds: local CL_MAP, CL_BAR = "parchment", "retro".
-- Written for the client's Lua 5.0: no #, no %, no ... in a body, no string methods, no string.match.
-- His rule for anything on the world map: if this breaks, the player still has the normal world map. So only frames of
-- its own are made, every handler runs inside pcall, a fault switches it all off (CL.broken, the CVar told "fault"),
-- and no function of Blizzard's is replaced or hooked and no frame of the map's moved. The one thing it sets on a frame
-- not its own: the shared drop-down list's scale, right after the client's ToggleDropDownMenu opened it for one of
-- these, to this map's (the client opens every list at the UI's scale but the Continent and Zone ones, at 1; it sets
-- the scale again each time any list opens).
-- The DLL hears a choice through one CVar of its own, CinderLoadStyle ("parchment retro"), and keeps it in
-- Data\CinderLoad\style.txt: a DLL cannot give this client's Lua a function of its own (each C function's address is
-- checked against WoW.exe's code before it is called, 0x42A320 from luaD_precall 0x6F61A1).
local CL = CinderLoad
if type(CL) ~= "table" then CL = {}; CinderLoad = CL end
CL.up = 1
CL.map = CL_MAP
CL.bar = CL_BAR
CL.cvar = "CinderLoadStyle"
CL.broken = nil
CL.settings = {
    { key = "map", label = "Loading map", name = "loading map",
      options = {
          { value = "parchment", text = "Parchment", tip = "The zone you arrive in, painted as this map paints it." },
          { value = "satellite", text = "Satellite", tip = "The living map of Azeroth: ships, currents and weather." } } },
    { key = "bar", label = "Loading bar", name = "loading bar",
      options = {
          { value = "retro", text = "Retro", tip = "The game's own loading bar, filling in your class's colour." },
          { value = "neo", text = "Neo", tip = "The blade of light in its metal frame, the RavenCraft logo on it." } } },
}
-- Where they sit, in the world map's 1024 x 768 box (WorldMapPositioningGuide), from its top left: on the row of the
-- Continent drop-down (its TOP at the box's TOP, -80, -35: x 342 to 522), from x 32 (clear of the top bar's left
-- flourish), each the width WIDTH inside (its frame WIDTH + 50), the second over the first's right end by OVERLAP as Zone
-- is over Continent's: x 32 to 283, Continent's frame from 342.
CL.X, CL.Y, CL.WIDTH, CL.OVERLAP = 32, -35, 92, -33
CL.drops = {}

function CL.Say(text)
    local f = DEFAULT_CHAT_FRAME
    if not f then f = ChatFrame1 end
    if f then f:AddMessage("CinderLoad: " .. text, 1, 1, 0) end      -- (a system message's yellow)
end

function CL.Setting(key)
    for _, s in ipairs(CL.settings) do
        if s.key == key then return s end
    end
end

function CL.Option(s, value)
    if not s then return end
    for _, o in ipairs(s.options) do
        if o.value == value then return o end
    end
end

-- "loading map Parchment, loading bar Retro"
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

-- The choice to the DLL, through the CVar it reads (its third word: a fault, so it stops too).
function CL.Save(fault)
    local v = CL.map .. " " .. CL.bar
    if fault then v = v .. " fault" end
    SetCVar(CL.cvar, v)
end

-- The shared drop-down list is open, and for this drop-down.
function CL.ListOpen(f)
    return DropDownList1 and DropDownList1:IsShown() and UIDROPDOWNMENU_OPEN_MENU == f:GetName()
end

-- Every drop-down shows the choice. Its own list open: the client's UIDropDownMenu_SetSelectedValue, which redraws
-- that list's checks; otherwise only the drop-down's own value (what UIDropDownMenu_SetSelectedValue sets before it
-- redraws the shared list, which may be showing another drop-down's choices).
function CL.Refresh()
    for _, d in ipairs(CL.drops) do
        local o = CL.Option(d.setting, CL[d.setting.key])
        if o then
            local f = d.frame
            if CL.ListOpen(f) then
                UIDropDownMenu_SetSelectedValue(f, o.value)
            else
                f.selectedName = nil
                f.selectedID = nil
                f.selectedValue = o.value
            end
            UIDropDownMenu_SetText(o.text, f)
        end
    end
end

function CL.Set(key, value)
    local s = CL.Setting(key)
    if not CL.Option(s, value) then return end
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
    CL.Say("its loading screen choices on the world map met an error and turned themselves off until the game starts "
           .. "again; your world map is as it was. |cff9d9d9d(" .. tostring(where) .. ": " .. tostring(err) .. ")|r")
end

-- fn inside pcall, as a script handler (1.12's handlers read this and arg1), a menu's function (its arg1) or the slash
-- command (its text).
function CL.Safe(where, fn)
    return function(a1)
        if CL.broken then return end
        local ok, err = pcall(fn, a1)
        if not ok then CL.Fault(where, err) end
    end
end

-- /cinderload: says the choice; /cinderload map (or bar) switches one; /cinderload map satellite sets one (the value
-- alone does too: /cinderload neo). Words only, at most two, nothing else: anything more or other changes nothing.
CL.usage = "type /cinderload map or /cinderload bar to switch, or /cinderload map parchment, /cinderload map satellite, "
           .. "/cinderload bar retro or /cinderload bar neo."
function CL.Slash(msg)
    local words = {}
    for w in string.gfind(string.lower(msg or ""), "[^%s]+") do table.insert(words, w) end
    local n = table.getn(words)
    if n == 0 then
        CL.Say(CL.Describe() .. ". Type /cinderload map or /cinderload bar to switch.")
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

-- One drop-down's menu (the client calls it as the list opens: UIDropDownMenu_Initialize from ToggleDropDownMenu): its
-- two choices, the one chosen checked (the client's UIDropDownMenu_AddButton checks the one of the drop-down's value);
-- each, picked, sets it. The tips show as the client shows a menu's tips (with its beginner's tips on).
function CL.Menu(d)
    return CL.Safe("menu", function()
        for _, o in ipairs(d.setting.options) do
            local info = {}
            info.text = o.text
            info.value = o.value
            info.arg1 = o.value
            info.func = d.pick
            info.tooltipTitle = o.text
            info.tooltipText = o.tip .. " From your next loading screen."
            UIDropDownMenu_AddButton(info, 1)
        end
    end)
end
function CL.Pick(d)
    return CL.Safe("pick", function(value)
        if not CL.Option(d.setting, value) then return end
        CL.Set(d.setting.key, value)
    end)
end
-- Its arrow clicked: what the template's own button does (ToggleDropDownMenu, its sound), inside pcall, then the list's
-- scale made this map's, as the client makes it for Continent and Zone (see the top).
function CL.Open(d)
    return CL.Safe("open", function()
        ToggleDropDownMenu(1, nil, d.frame)
        local list = DropDownList1
        if list and CL.ListOpen(d.frame) then
            local over = 1
            local p = list:GetParent()
            if p then over = p:GetEffectiveScale() end
            local s = d.frame:GetEffectiveScale() / over
            if math.abs(list:GetScale() - s) > 0.0001 then list:SetScale(s) end
        end
        PlaySound("igMainMenuOptionCheckBoxOn")
    end)
end

-- The map's own top bar is there: its Continent drop-down shown and not see-through (this client's small, windowed map
-- keeps it but sets it so, WorldMapFrame_Minimize, and hides the bar's art). Read from the drop-down as it is, not from
-- WORLDMAP_WINDOWED: minimising the open map shows it again before WORLDMAP_WINDOWED says so.
function CL.OnTopBar()
    local c = WorldMapContinentDropDown
    if not c then return WORLDMAP_WINDOWED ~= 1 end
    if not c:IsVisible() then return nil end
    return c:GetAlpha() > 0.05
end

-- Shown on the full map's top bar, as Continent and Zone are; hidden with them on the windowed map.
function CL.Place()
    local on = CL.OnTopBar()
    for _, d in ipairs(CL.drops) do
        if on and not d.frame:IsShown() then d.frame:Show() end
        if not on and d.frame:IsShown() then d.frame:Hide() end
    end
end

function CL.Build()
    local guide = getglobal("WorldMapPositioningGuide")
    if not guide then guide = WorldMapFrame end
    local p = getglobal("CinderLoadMapPanel")
    if not p then p = CreateFrame("Frame", "CinderLoadMapPanel", WorldMapFrame) end
    CL.panel = p
    p:ClearAllPoints()
    p:SetPoint("TOPLEFT", guide, "TOPLEFT", CL.X, CL.Y)
    p:SetWidth(2 * (CL.WIDTH + 50) + CL.OVERLAP)
    p:SetHeight(32)
    CL.drops = {}
    local prev
    for _, s in ipairs(CL.settings) do
        local name = "CinderLoadMap" .. s.key .. "DropDown"
        local f = getglobal(name)
        if not f then f = CreateFrame("Frame", name, p, "UIDropDownMenuTemplate") end
        f:ClearAllPoints()
        if prev then f:SetPoint("LEFT", prev, "RIGHT", CL.OVERLAP, 0) else f:SetPoint("TOPLEFT", p, "TOPLEFT", 0, 0) end
        local label = getglobal(name .. "Label")
        if not label then label = f:CreateFontString(name .. "Label", "OVERLAY", "GameFontNormalSmall") end
        label:SetText(s.label)
        label:ClearAllPoints()
        label:SetPoint("TOPLEFT", f, "TOPLEFT", 20, 10)
        local d = { setting = s, frame = f }
        d.pick = CL.Pick(d)
        UIDropDownMenu_SetWidth(CL.WIDTH, f)
        if DropDownList1 and DropDownList1:IsShown() then
            f.initialize = CL.Menu(d)                 -- (a list open: the client's Initialize would empty it; it is set later)
        else
            UIDropDownMenu_Initialize(f, CL.Menu(d))
        end
        getglobal(name .. "Button"):SetScript("OnClick", CL.Open(d))
        table.insert(CL.drops, d)
        prev = f
    end
    -- placed as the map shows, and once more on its first frame (whatever ran after its OnShow)
    CL.again = CL.Safe("again", function()
        this:SetScript("OnUpdate", nil)
        CL.Place()
    end)
    p:SetScript("OnShow", CL.Safe("show", function()
        CL.Place()
        this:SetScript("OnUpdate", CL.again)
    end))
    CL.Refresh()
    CL.Place()
    p:Show()
end

function CL.Setup()
    CL.Save()                                        -- (first: the DLL hears that this ran)
    SLASH_CINDERLOAD1 = "/cinderload"
    SlashCmdList["CINDERLOAD"] = function(msg)
        if CL.broken then
            CL.Say("its loading screen choices are off until the game starts again (they met an error).")
            return
        end
        CL.Safe("slash", CL.Slash)(msg)
    end
    if WorldMapFrame then CL.Build() end
end

local ok, err = pcall(CL.Setup)
if not ok then CL.Fault("setup", err) end
