-- UI and persistence run on the Lua thread; native rendering uses the plugin bridge.
local KEY = "di3.ps4.enabled"
local FILE = "mods/ps4-glyphs/settings.json"
local enabled = false
local setChoice
local selectionHooks = {}

local function updateRowLabels(screen)
    if not screen or type(screen.listData) ~= "table" then return false end
    local changed = false
    local label = enabled and "PlayStation 4" or "Xbox"
    for _, row in ipairs(screen.listData) do
        if row.id == "PS4Glyphs_ControllerStyle" then
            local text = "Controller glyphs: " .. label
            if row.enumValue ~= label or row.text ~= text then changed = true end
            row.enumValue, row.text = label, text
        end
    end
    return changed
end

-- LR_Toggle has no icon clip and never reads its icon field. Use the game's
-- icon-capable Button class, showing the choice in its label and toggling on Select.
-- SetElement updates this button in place, so changing the choice preserves focus.
local function arrangeRow(screen)
    local list = screen and screen.listData
    if type(list) ~= "table" then return false end
    local own, bindings
    for i, row in ipairs(list) do
        if row.id == "PS4Glyphs_ControllerStyle" then own = i end
        if row.id == "Crabe_PadBindings" then bindings = i end
    end
    if not own then return false end
    local row = list[own]
    local changed = updateRowLabels(screen)
    changed = row.icon ~= "MenuIcon_Controls" or row.widgetType ~= "Button" or changed
    row.icon, row.widgetType, row.buttonState = "MenuIcon_Controls", "Button", ""
    if not selectionHooks[screen] then
        local original = screen.SelectItem
        local wrapper = function(self, opt, ...)
            local selected = self.listData and self.listData[self.selectedIndex or tonumber(opt)]
            if selected and selected.id == "PS4Glyphs_ControllerStyle" then
                setChoice(self, selected.id, enabled and 1 or 2)
                return
            end
            if original then return original(self, opt, ...) end
        end
        selectionHooks[screen] = { original = original, wrapper = wrapper }
        screen.SelectItem = wrapper
    end
    if bindings and own ~= bindings + 1 then
        local selected = screen.selectedIndex and list[screen.selectedIndex]
        table.remove(list, own)
        if own < bindings then bindings = bindings - 1 end
        table.insert(list, bindings + 1, row)
        if selected then
            for i, item in ipairs(list) do
                if item == selected then screen.selectedIndex = i break end
            end
        end
        changed = true
    end
    return changed
end

local function ready()
    return Crabe.Plugins and Crabe.Plugins.get("di3.ps4.ready") == "1"
end

setChoice = function(screen, _, index)
    if not ready() then
        Crabe.write("! [PS4 Glyphs] Native renderer unavailable; restart after installing the ASI.")
        return
    end
    local nextEnabled = index == 2
    local ok, err = Crabe.Storage.save(FILE, { playstation4 = nextEnabled })
    if not ok then
        Crabe.write("! [PS4 Glyphs] Could not save setting: " .. tostring(err))
        return
    end
    if Crabe.Plugins.set(KEY, nextEnabled and "1" or "0") then
        enabled = nextEnabled
        updateRowLabels(screen)
        if screen and screen.CallFlashFunction then
            for i, row in ipairs(screen.listData or {}) do
                if row.id == "PS4Glyphs_ControllerStyle" then
                    screen:CallFlashFunction("_root.SetElement", tostring(i), row.text)
                end
            end
        end
    end
end

-- Some loader builds attach their BuildList wrapper after a persistent menu's first build.
-- Refresh only a visible screen missing our row; never rebuild an already-correct menu.
local elapsed = 0
local function refreshVisibleMenus(remove)
    if not UI or not UI.GetHandleFromName or not UI.GetScreenFromScreenHandle then return end
    for _, name in ipairs({ "SettingsMenu", "InGameSettingsMenu" }) do
        local handle = UI.GetHandleFromName(name, 0)
        local screen = handle and handle ~= -1 and UI.GetScreenFromScreenHandle(handle)
        if screen and (not screen.IsVisible or screen:IsVisible()) and type(screen.listData) == "table" then
            local found = false
            for i = #screen.listData, 1, -1 do
                if screen.listData[i].id == "PS4Glyphs_ControllerStyle" then
                    found = true
                    if remove then table.remove(screen.listData, i) end
                end
            end
            local changed = not remove and updateRowLabels(screen)
            if not remove then changed = arrangeRow(screen) or changed end
            if remove and found then
                if screen.Rebuild then screen:Rebuild() end
            elseif not remove and not found and screen.BuildList then
                screen:BuildList()
                arrangeRow(screen)
                if screen.Rebuild then screen:Rebuild() end
            elseif not remove and found and changed then
                if screen.Rebuild then screen:Rebuild() end
            end
        end
    end
end

Crabe.Mod.register({
    name = "PlayStation 4 Controller Glyphs",
    onInit = function()
        if not ready() then
            Crabe.write("! [PS4 Glyphs] Native renderer unavailable; mod disabled. See loader.log.")
            return
        end
        local ok, saved = pcall(Crabe.Storage.load, FILE)
        if not ok then Crabe.write("! [PS4 Glyphs] Invalid settings; using Xbox glyphs.") end
        enabled = ok and type(saved) == "table" and saved.playstation4 == true
        Crabe.Plugins.set(KEY, enabled and "1" or "0")
        for _, screen in ipairs({ "SettingsMenu", "InGameSettingsMenu" }) do
            assert(Crabe.Settings.addOption(screen, {
                id = "PS4Glyphs_ControllerStyle",
                text = "Controller glyphs",
                icon = "MenuIcon_Controls",
                widgetType = "LR_Toggle",
                choices = { "Xbox", "PlayStation 4" },
                get = function() return enabled and 2 or 1 end,
                set = setChoice,
            }), "Could not register controller glyph option")
            Crabe.Settings.onBuild(screen, arrangeRow)
        end
        Crabe.write("[PS4 Glyphs] Options > Controller glyphs: Xbox / PlayStation 4")
    end,
    onUpdate = function(dt)
        elapsed = elapsed + (dt or 0)
        if not ready() or elapsed < 0.25 then return end
        elapsed = 0
        refreshVisibleMenus(false)
    end,
    onShutdown = function()
        refreshVisibleMenus(true)
        for screen, hook in pairs(selectionHooks) do
            if screen.SelectItem == hook.wrapper then screen.SelectItem = hook.original end
        end
        selectionHooks = {}
        if Crabe.Plugins then Crabe.Plugins.set(KEY, "0") end
    end,
})
