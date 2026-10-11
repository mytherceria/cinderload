-- The far end of test_switch.c's run of the whole path (TestSwitchesPath): the game's UI, stubbed as tests/wow_ui.lua
-- stubs it (with the client's own drop-down and world map code when framexml=<dir> is given), driven over stdin as the
-- DLL and a player would drive it. Each request is the CVar as the client holds it ("cvar <value>" or "nocvar"), then one
-- command; the answer is any "info <words>" lines, the CVar as the UI left it, then "end".
--   exec <n>              n bytes of Lua follow: run as FrameScript_Execute runs a chunk (an error caught, as its handler does)
--   fresh                 a new UI (a login or a /reload): the CVar kept, everything else gone
--   openmap, closemap, minimize, maximize
--   open, close           /cinderload alone (the window); the window shut
--   pick <key> <value>    a radio of the window's clicked (map, bar, areas), or an effect's picture (effect heat)
--   slide <key> <value>   a slider of the window's dragged (nearby, thick, width)
--   tick <name>           a check box of the window's clicked (Whole, Colour, QualityLiquid ...)
--   drag                  the window dragged by its title
--   slash <words>         /cinderload <words>
--   text <key>            info: what that control shows, and whether the window shows
--   map                   info: anything of ours on the world map, anything of the game's changed
--   errors                info: the UI's Lua errors so far
--   lua5.1 tests/lua_bridge.lua [framexml=<dir>]
local here = string.gsub(arg[0], "[^/]*$", "")
if here == "" then here = "./" end
local UI = dofile(here .. "wow_ui.lua")
local fx
for i = 1, table.getn(arg) do
    local _, _, v = string.find(arg[i], "^framexml=(.+)$")
    if v then fx = v end
end
getmetatable("").__index = nil
local world, env
local held                                     -- the CVar as the client holds it (nil: not made)
local function Fresh()
    world = UI.Fresh({ cvar = held or false, frameXml = fx })
    env = world.env
end
Fresh()
local function Reply(info)
    for _, l in ipairs(info) do io.write("info ", l, "\n") end
    local v = world.cvars.CinderLoadStyle
    if v == nil then io.write("nocvar\n") else io.write("cvar ", v, "\n") end
    io.write("end\n")
    io.flush()
end
while true do
    local line = io.read("*l")
    if not line then break end
    local _, _, cmd, rest = string.find(line, "^(%S+)%s?(.*)$")
    if cmd == "cvar" then
        held = rest
        world.cvars.CinderLoadStyle = rest
    elseif cmd == "nocvar" then
        held = nil
        world.cvars.CinderLoadStyle = nil
    else
        local info = {}
        if cmd == "exec" then
            local code = io.read(tonumber(rest))
            local ok, err = UI.Run50(code, "CinderLoad")
            if not ok then table.insert(world.errors, tostring(err)) end
        elseif cmd == "fresh" then
            Fresh()
        elseif cmd == "openmap" then
            if not env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end
            UI.Frame()
        elseif cmd == "closemap" then
            if env.WorldMapFrame:IsVisible() then env.ToggleWorldMap() end
        elseif cmd == "minimize" then
            env.WorldMapFrame_Minimize()
            UI.Frame()
        elseif cmd == "maximize" then
            env.WorldMapFrame_Maximize()
            UI.Frame()
        elseif cmd == "open" then
            if env.SlashCmdList.CINDERLOAD then env.SlashCmdList.CINDERLOAD("") end
        elseif cmd == "close" then
            if env.CinderLoadPanel then env.CinderLoadPanel:Hide() end
        elseif cmd == "pick" then                                   -- a radio of the window's (or an effect's picture)
            local _, _, key, value = string.find(rest, "^(%S+) (%S+)$")
            local picked = false
            local CL = env.CinderLoad
            local r = env["CinderLoadPanel" .. tostring(key) .. tostring(value)]
            if r then picked = UI.Click(r)
            elseif CL and CL.ui then
                for _, c in ipairs(CL.ui.cards) do if c.key == key and c.value == value then picked = UI.Click(c.button) end end
            end
            table.insert(info, picked and "picked" or "not picked")
        elseif cmd == "slide" then                                  -- a slider of the window's to a value
            local _, _, key, value = string.find(rest, "^(%S+) (%S+)$")
            local sl = env["CinderLoadPanel" .. tostring(key) .. "Slider"]
            table.insert(info, sl and UI.Slide(sl, tonumber(value)) and "slid" or "not slid")
        elseif cmd == "tick" then                                   -- a check box of the window's clicked
            local c = env["CinderLoadPanel" .. rest]
            table.insert(info, c and UI.Click(c) and (c:GetChecked() and "ticked" or "unticked") or "not there")
        elseif cmd == "drag" then
            local t = env.CinderLoadPanelTitle
            table.insert(info, t and UI.Drag(t, 40, -20) and "moved" or "not moved")
        elseif cmd == "text" then                                   -- what a control of the window's shows, and whether it shows
            local CL = env.CinderLoad
            local p = env.CinderLoadPanel
            local out
            if CL and CL.ui and CL.ui.sliders[rest] then out = CL.ui.sliders[rest].value:GetText()
            elseif CL and CL.ui then
                for _, r in ipairs(CL.ui.radios) do
                    if r.key == rest and r.button:GetChecked() then out = env[r.button:GetName() .. "Text"]:GetText() end
                end
            end
            if out then table.insert(info, tostring(out) .. (p:IsVisible() and " shown" or " hidden")) end
        elseif cmd == "slash" then
            world.chat = {}
            if env.SlashCmdList.CINDERLOAD then env.SlashCmdList.CINDERLOAD(rest) end
            for _, t in ipairs(world.chat) do table.insert(info, t.text) end
        elseif cmd == "map" then                                    -- the world map: anything of ours on it
            local n = 0
            for k in pairs(env) do if type(k) == "string" and string.find(k, "^CinderLoadMap") then n = n + 1 end end
            for _, f in ipairs(world.made) do
                local q = f.parent
                while q do if q == env.WorldMapFrame and not f.blizzard then n = n + 1 end; q = q.parent end
            end
            table.insert(info, "ours on the map " .. n .. ", touched " .. table.getn(world.touched))
        elseif cmd == "errors" then
            table.insert(info, tostring(table.getn(world.errors)) .. " " .. table.concat(world.errors, " | "))
        else
            table.insert(info, "unknown command " .. tostring(cmd))
        end
        Reply(info)
    end
end
