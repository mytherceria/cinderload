-- The far end of test_switch.c's run of the whole path (TestSwitchesPath): the game's UI, stubbed as tests/wow_ui.lua
-- stubs it (with the client's own drop-down and world map code when framexml=<dir> is given), driven over stdin as the
-- DLL and a player would drive it. Each request is the CVar as the client holds it ("cvar <value>" or "nocvar"), then one
-- command; the answer is any "info <words>" lines, the CVar as the UI left it, then "end".
--   exec <n>              n bytes of Lua follow: run as FrameScript_Execute runs a chunk (an error caught, as its handler does)
--   fresh                 a new UI (a login or a /reload): the CVar kept, everything else gone
--   openmap, closemap, minimize, maximize
--   pick <key> <value>    the drop-down's arrow clicked, then that choice in its list
--   slash <words>         /cinderload <words>
--   text <key>            info: the drop-down's words, and whether it shows
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
        elseif cmd == "pick" then
            local _, _, key, value = string.find(rest, "^(%S+) (%S+)$")
            local arrow = env["CinderLoadMap" .. tostring(key) .. "DropDownButton"]
            local picked = false
            if arrow and UI.Click(arrow) and env.DropDownList1:IsShown() then
                for j = 1, env.DropDownList1.numButtons do
                    local b = env["DropDownList1Button" .. j]
                    if b.value == value then picked = UI.Click(b) end
                end
            end
            table.insert(info, picked and "picked" or "not picked")
        elseif cmd == "slash" then
            world.chat = {}
            if env.SlashCmdList.CINDERLOAD then env.SlashCmdList.CINDERLOAD(rest) end
            for _, t in ipairs(world.chat) do table.insert(info, t.text) end
        elseif cmd == "text" then
            local d = env["CinderLoadMap" .. rest .. "DropDown"]
            if d then table.insert(info, tostring(env[d:GetName() .. "Text"]:GetText()) .. (d:IsVisible() and " shown" or " hidden")) end
        elseif cmd == "errors" then
            table.insert(info, tostring(table.getn(world.errors)) .. " " .. table.concat(world.errors, " | "))
        else
            table.insert(info, "unknown command " .. tostring(cmd))
        end
        Reply(info)
    end
end
