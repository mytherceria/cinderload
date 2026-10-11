-- CinderLoad's two switches, on the world map (M) and as /cinderload (his decision, 10 Oct 2026): the loading screen's
-- MAP (Parchment | Satellite) and its BAR (Retro | Neo), each on its own. Run inside the game by CinderLoad.dll, on the
-- game's own thread while no Lua is running (no add-on folder: the release is the DLL and its packs). The DLL puts one
-- line before this text, the choice it holds: local CL_MAP, CL_BAR = "parchment", "retro".
-- Written for the client's Lua 5.0: no #, no %, no ... in a body, no string methods, no string.match.
-- His rule for anything on the world map: if this breaks, the player still has the normal world map. So only frames of
-- its own are made, every handler runs inside pcall, a fault switches it all off (CL.broken, the CVar told "fault"),
-- and nothing of Blizzard's is replaced, hooked or moved.
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
-- The controls' form: "check" (Blizzard's check boxes, his pick) or "radio" (Blizzard's radio buttons). Another form is
-- one more builder in CL.forms taking the same arguments.
CL.form = "check"
CL.settings = {
    { key = "map", label = "Loading screen map:",
      options = {
          { value = "parchment", text = "Parchment", tip = "The zone you arrive in, painted as this map paints it." },
          { value = "satellite", text = "Satellite", tip = "The living map of Azeroth: ships, currents and weather." } } },
    { key = "bar", label = "Loading bar:",
      options = {
          { value = "retro", text = "Retro", tip = "The game's own loading bar, filling in your class's colour." },
          { value = "neo", text = "Neo", tip = "The blade of light in its metal frame, the RavenCraft logo on it." } } },
}
CL.controls = {}

function CL.Say(text)
    local f = DEFAULT_CHAT_FRAME
    if not f then f = ChatFrame1 end
    if f then f:AddMessage("|cffffd100CinderLoad:|r " .. text, 1, 1, 1) end
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

-- "Loading screen map: Parchment. Loading bar: Retro."
function CL.Describe()
    local t = ""
    for _, s in ipairs(CL.settings) do
        local o = CL.Option(s, CL[s.key])
        local name = CL[s.key]
        if o then name = o.text end
        if t ~= "" then t = t .. " " end
        t = t .. s.label .. " " .. name .. "."
    end
    return t
end

-- The choice to the DLL, through the CVar it reads (its third word: a fault, so it stops too).
function CL.Save(fault)
    local v = CL.map .. " " .. CL.bar
    if fault then v = v .. " fault" end
    SetCVar(CL.cvar, v)
end

-- Every control shows the choice.
function CL.Refresh()
    for _, c in ipairs(CL.controls) do
        if CL[c.key] == c.value then c.button:SetChecked(1) else c.button:SetChecked(nil) end
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
    CL.Say("its switches on the world map met an error and turned themselves off until the game starts again; your "
           .. "world map is as it was. |cff9d9d9d(" .. tostring(where) .. ": " .. tostring(err) .. ")|r")
end

-- fn inside pcall, as a script handler (1.12's handlers read this and arg1) or the slash command (its text).
function CL.Safe(where, fn)
    return function(a1)
        if CL.broken then return end
        local ok, err = pcall(fn, a1)
        if not ok then CL.Fault(where, err) end
    end
end

-- /cinderload: says the choice; /cinderload map (or bar) flips one; /cinderload map satellite sets one (the value alone
-- does too: /cinderload neo).
CL.usage = "/cinderload, /cinderload map, /cinderload bar, /cinderload map parchment|satellite, /cinderload bar retro|neo."
function CL.Slash(msg)
    local _, _, a, b = string.find(string.lower(msg or ""), "^%s*(%a*)%s*(%a*)")
    if a == nil then a = "" end
    if b == nil then b = "" end
    local key, value = a, b
    if a ~= "" and not CL.Setting(a) then
        for _, s in ipairs(CL.settings) do
            if CL.Option(s, a) then key = s.key; value = a end
        end
        if key == a then CL.Say("say it as " .. CL.usage); return end
        if b ~= "" then CL.Say("say it as " .. CL.usage); return end
    end
    if key == "" then
        CL.Say(CL.Describe() .. " Change them on the world map (M), or with " .. CL.usage)
        return
    end
    local s = CL.Setting(key)
    if value == "" then
        value = s.options[1].value
        if CL[key] == value then value = s.options[2].value end
    end
    if not CL.Set(key, value) then CL.Say("say it as " .. CL.usage); return end
    CL.Say(CL.Describe() .. " From your next loading screen.")
end

-- The builders: one setting's options from x across panel p, each a control of the form's; the x after them. A form is
-- one function here (a drop-down would be one more, taking the same arguments).
CL.forms = {}
CL.BOX = 24          -- the check box, as Blizzard's own world map later sized its one (Show Quest Objectives, Wrath)
CL.GAP = 12          -- between one setting's options
CL.SEP = 26          -- between the two settings
local function OptionButton(p, s, o, template, size, textGap, x)
    local name = "CinderLoadMap" .. s.key .. o.value
    local b = getglobal(name)
    if not b then b = CreateFrame("CheckButton", name, p, template) end
    b:SetWidth(size)
    b:SetHeight(size)
    b:ClearAllPoints()
    b:SetPoint("LEFT", p, "LEFT", x, 0)
    local text = getglobal(name .. "Text")
    text:SetText(o.text)
    local w = text:GetStringWidth()
    if b.SetHitRectInsets then b:SetHitRectInsets(0, -(w + textGap), 0, 0) end   -- (its word clicks it too)
    b:SetScript("OnClick", CL.Safe("click", function()
        CL.Set(s.key, o.value)                      -- (and every box shown again: the client unchecked this one if it was)
        if PlaySound then PlaySound("igMainMenuOptionCheckBoxOn") end
    end))
    b:SetScript("OnEnter", CL.Safe("tip", function()
        GameTooltip:SetOwner(this, "ANCHOR_TOP")
        GameTooltip:SetText(s.label .. " " .. o.text, 1, 1, 1)
        GameTooltip:AddLine(o.tip, 1, 0.82, 0, 1)
        GameTooltip:AddLine("From your next loading screen.", 0.75, 0.75, 0.75, 1)
        GameTooltip:Show()
    end))
    b:SetScript("OnLeave", CL.Safe("tip", function() GameTooltip:Hide() end))
    table.insert(CL.controls, { key = s.key, value = o.value, button = b })
    return x + size + textGap + w
end
local function ButtonRow(p, s, x, template, size, textGap)
    for i, o in ipairs(s.options) do
        if i > 1 then x = x + CL.GAP end
        x = OptionButton(p, s, o, template, size, textGap, x)
    end
    return x
end
CL.forms.check = function(p, s, x) return ButtonRow(p, s, x, "UICheckButtonTemplate", CL.BOX, -2) end
CL.forms.radio = function(p, s, x) return ButtonRow(p, s, x, "UIRadioButtonTemplate", 16, 5) end

-- One setting: its label, then its options in the form chosen.
function CL.Row(p, s, x)
    local name = "CinderLoadMap" .. s.key .. "Label"
    local label = getglobal(name)
    if not label then label = p:CreateFontString(name, "ARTWORK", "GameFontHighlightSmall") end
    label:SetText(s.label)
    label:ClearAllPoints()
    label:SetPoint("LEFT", p, "LEFT", x, 0)
    x = x + label:GetStringWidth() + 6
    local form = CL.forms[CL.form]
    if not form then form = CL.forms.check end
    return form(p, s, x)
end

-- Where the panel sits: the bottom right of the map's own frame, in the strip under the map (where Blizzard later put
-- its world map's check box); smaller when the map is in its window (this client's minimised map), as the map is.
function CL.Place(p)
    local guide = getglobal("WorldMapPositioningGuide")
    if not guide then guide = WorldMapFrame end
    local s = 1
    if WORLDMAP_WINDOWED == 1 then
        s = 0.7
        if type(WORLDMAP_WINDOWED_SCALE) == "number" and WORLDMAP_WINDOWED_SCALE > 0.3 and WORLDMAP_WINDOWED_SCALE <= 1 then
            s = WORLDMAP_WINDOWED_SCALE
        end
    end
    p:SetScale(s)
    p:ClearAllPoints()
    p:SetPoint("BOTTOMRIGHT", guide, "BOTTOMRIGHT", -16, 4)
    p:SetFrameLevel(WorldMapFrame:GetFrameLevel() + 8)
end

function CL.Build()
    local p = getglobal("CinderLoadMapPanel")
    if not p then p = CreateFrame("Frame", "CinderLoadMapPanel", WorldMapFrame) end
    CL.panel = p
    CL.controls = {}
    local x = 0
    for i, s in ipairs(CL.settings) do
        if i > 1 then x = x + CL.SEP end
        x = CL.Row(p, s, x)
    end
    p:SetWidth(x)
    p:SetHeight(CL.BOX)
    p:SetScript("OnShow", CL.Safe("show", function() CL.Place(this); CL.Refresh() end))
    CL.Place(p)
    CL.Refresh()
    p:Show()
end

function CL.Setup()
    CL.Save()                                        -- (first: the DLL hears that this ran)
    SLASH_CINDERLOAD1 = "/cinderload"
    SlashCmdList["CINDERLOAD"] = function(msg)
        if CL.broken then
            CL.Say("its switches are off until the game starts again (they met an error).")
            return
        end
        CL.Safe("slash", CL.Slash)(msg)
    end
    if WorldMapFrame then CL.Build() end
end

local ok, err = pcall(CL.Setup)
if not ok then CL.Fault("setup", err) end
