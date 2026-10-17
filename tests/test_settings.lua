-- Exercise the mod's real settings callbacks, persistence failures, and safe shutdown.
local options, values, disk, registered, logs
local function reset(native)
    options, values, disk, registered, logs = {}, {['di3.ps4.ready']=native and '1' or '0'}, nil, nil, {}
    Crabe = {
        write=function(s) logs[#logs+1]=s end,
        Plugins={get=function(k) return values[k] end, set=function(k,v) values[k]=v; return true end},
        Storage={load=function() return disk end, save=function(_,v) disk=v; return true end},
        Settings={onBuild=function() return true end, addOption=function(screen,entry) options[screen]=entry; return true end},
        Mod={register=function(def) registered=def end},
    }
    assert(loadfile('mod/main.lua'))()
    registered.onInit()
end
reset(true)
local row=assert(options.SettingsMenu)
assert(options.InGameSettingsMenu.id==row.id)
assert(row.get()==1 and values['di3.ps4.enabled']=='0')
row.set(nil,nil,2)
assert(row.get()==2 and disk.playstation4 and values['di3.ps4.enabled']=='1')
registered.onShutdown()
assert(values['di3.ps4.enabled']=='0')
registered.onInit()
assert(row.get()==2 and values['di3.ps4.enabled']=='1')
row.set(nil,nil,1)
assert(row.get()==1 and not disk.playstation4)
Crabe.Storage.save=function() return false,'read-only disk' end
row.set(nil,nil,2)
assert(row.get()==1 and values['di3.ps4.enabled']=='0')
reset(false)
assert(not options.SettingsMenu)
reset(true)
Crabe.Storage.load=function() error('invalid JSON') end
registered.onInit()
assert(options.SettingsMenu.get()==1)
reset(true)
local screen = { listData = { { id = 'vanilla' } }, builds = 0, rebuilds = 0 }
function screen:BuildList()
    self.builds = self.builds + 1
    self.listData = { { id = 'vanilla' }, options.SettingsMenu }
end
function screen:Rebuild() self.rebuilds = self.rebuilds + 1; self.selectedIndex = 1 end
UI = {
    GetHandleFromName = function(name) return name == 'SettingsMenu' and 5 or -1 end,
    GetScreenFromScreenHandle = function() return screen end,
}
registered.onUpdate(0.1)
assert(screen.builds == 0)
registered.onUpdate(0.2)
assert(screen.builds == 1 and screen.rebuilds == 1 and #screen.listData == 2)
registered.onUpdate(0.3)
assert(screen.builds == 1, 'correct menus must not rebuild repeatedly')
-- Inherited settings wrappers may return a different row object from the registered choice.
local displayed = { id = 'PS4Glyphs_ControllerStyle', enumValue = 'Xbox' }
screen.listData = { { id = 'vanilla' }, displayed }
local beforeRepaint = screen.rebuilds
screen.selectedIndex = 2
displayed.icon = "MenuIcon_Controls"
displayed.widgetType = "Button"
options.SettingsMenu.set(screen, displayed.id, 2)
assert(displayed.enumValue == 'PlayStation 4', 'the displayed row must update immediately')
assert(screen.rebuilds == beforeRepaint, 'do not rebuild reentrantly inside the input callback')
registered.onUpdate(0.01)
assert(screen.rebuilds == beforeRepaint and screen.selectedIndex == 2, 'changing glyphs must not rebuild the list or reset focus')
options.SettingsMenu.set(screen, displayed.id, 1)
assert(displayed.enumValue == 'Xbox')
registered.onUpdate(0.01)
assert(screen.rebuilds == beforeRepaint and screen.selectedIndex == 2)
registered.onUpdate(0.3)
assert(screen.rebuilds == beforeRepaint and screen.selectedIndex == 2, 'the periodic poll must preserve focus after a toggle')
-- Select must toggle the icon-capable button in place and delegate other rows.
assert(displayed.widgetType == 'Button' and displayed.icon == 'MenuIcon_Controls')
local flashCalls = 0
function screen:CallFlashFunction(method, id, text)
    assert(method == '_root.SetElement' and id == '2')
    assert(text == 'Controller glyphs: PlayStation 4' or text == 'Controller glyphs: Xbox')
    flashCalls = flashCalls + 1
end
screen:SelectItem('2')
assert(displayed.text == 'Controller glyphs: PlayStation 4' and disk.playstation4)
screen:SelectItem('2')
assert(displayed.text == 'Controller glyphs: Xbox' and not disk.playstation4)
assert(flashCalls == 2 and screen.selectedIndex == 2 and screen.rebuilds == beforeRepaint)
-- A stale row on a reopened menu must also recover from the current saved choice.
displayed.enumValue = 'PlayStation 4'
registered.onUpdate(0.3)
assert(displayed.enumValue == 'Xbox' and screen.rebuilds == beforeRepaint + 1)
registered.onShutdown()
assert(#screen.listData == 1 and screen.listData[1].id == 'vanilla')
assert(values['di3.ps4.enabled'] == '0')
screen.listData = { { id = 'vanilla' } }
function screen:IsVisible() return false end
registered.onUpdate(0.3)
assert(screen.builds == 1, 'hidden persistent menus must not rebuild or be resurrected')
UI = nil
print('Lua settings checks passed: persistence, reload, failures, first-open recovery, menu cleanup.')
