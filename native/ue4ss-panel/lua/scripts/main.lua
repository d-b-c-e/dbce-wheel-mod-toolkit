-- DbcePanel: the F6 settings panel (STD-030) for a UE4SS game with the toolkit triple renderer. The panel is a native
-- DLL beside this script (ue4ss\Mods\DbcePanel\panel\dbce-ue4ss-panel.dll with its panel.ini; toolkit
-- native/ue4ss-panel) loaded once with package.loadlib; its luaopen function starts the panel's overlay and returns
-- nothing. Settings and drawing only: no input to the game, no gameplay or save change. F6 opens and closes it.
local function log(s) print("[DbcePanel] " .. s) end

-- This mod's folder, from the script's own path (...\DbcePanel\scripts\main.lua).
local function modDir()
  local info = debug and debug.getinfo and debug.getinfo(1, "S")   -- level 1: this function, in this file
  local src = info and info.source or ""
  if src:sub(1, 1) == "@" then return src:sub(2):match("^(.*)[\\/]scripts[\\/][^\\/]+$") end
  return nil
end

local dir = modDir()
if not dir then log("could not tell this mod's folder; panel not started") return end
local path = dir .. [[\panel\dbce-ue4ss-panel.dll]]
local start, err = package.loadlib(path, "luaopen_dbce_ue4ss_panel")
if not start then
  log("could not load the panel (" .. tostring(err) .. "): " .. path)
  return
end
local ok, e = pcall(start)
log(ok and "panel started (F6)" or ("panel start failed: " .. tostring(e)))
