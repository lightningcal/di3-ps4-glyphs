# Validation record

Windows Win32 Release build.

- Followed the current published CrabeLoader mod development, native plugin and
  texture/VFS guides. Native API header v1 copied unmodified.
- CMake build succeeds. CTest: **3/3 passed** (fingerprints, Lua settings, real D3D11 renderer).
- Real GPU test uses the built ASI with an SDK host and a local vanilla glyph.
  Initial-data and deferred-upload paths bind a 209x209 replacement when enabled
  and restore the identical original shader view when disabled. The test allows
  120 ms for the 50 ms poll and Windows clock/scheduling granularity.
- Lua checks cover saved choice, reload, failed save, missing native plugin,
  invalid JSON, first-open menu recovery, no repeated rebuild, and shutdown cleanup.
- All 36 PNG assets: RGBA, 209x209, transparent background, padded unclipped bounds.
- ZIP: 36 PNGs, Lua entry point, manifest, Win32 ASI, license. No vanilla textures,
  compiled game scripts, or machine-local settings included.
- Actual installed game: native plugin initializes and matches **50** texture
  instances; loader accepts `local.di3-ps4-glyphs 0.1.0` with no rejected mods.
- Exercised real `SettingsMenu.NextEnumValue`: returned `PlayStation 4`, bridge
  `di3.ps4.enabled=1`, disk preference `{"playstation4":true}`. Preference reloads
  after game restart.
- Observed the loader attaching its settings wrapper after the first menu build;
  implemented and tested a mod-local missing-row refresh to handle this.
- The live game has not supplied a confirmed controller-glyph draw while testing
  from the background (`substitutions=0`). A foreground visual/controller check
  remains necessary; GPU binding tests do not establish every in-game prompt's
  appearance or whether any specific screen batches/copies glyphs into an atlas.

## 0.1.1 correction after visual bug report

- User screenshots established that the original implementation did not replace
  the visible A/B legend despite a PlayStation setting, and showed an extra empty panel.
- The extra panel was diagnostic Options instance 9 created in the previous test.
  It was closed explicitly; the final restart did not create any diagnostic screen.
- The initial renderer intercepted only immediate contexts. D3D11 deferred
  contexts have different runtime entry points; the game records its real draws
  through those methods. Added hooks for deferred PSSetShaderResources and
  UpdateSubresource, with the correct original dispatch for each context type.
- Regression test now draws a pixel from the actual glyph texture and reads the
  GPU output back, covering immediate contexts and executed deferred command
  lists, initial/later uploads, PlayStation enabled, and Xbox disabled.
  CTest: **3/3 passed**.
- Hidden persistent menus are excluded from the compatibility refresh. A Lua
  regression check verifies they are not rebuilt or resurrected.
- Live game check: ready/enabled/active, **1,684 substitutions**,
  **one Options instance**, visible=true. This supersedes the earlier zero-draw result.
- The setting is saved as playstation4=true. Native ASI, main.lua and manifest
  are verified to match the 0.1.1 package. Original loader repository stays clean.

## 0.1.2 displayed-label correction

- Live diagnostic confirmed the visible Options row belonged to the inherited
  InGameSettingsMenu record (`Xbox`), while the SettingsMenu callback updated a
  separate record (`PlayStation 4`). Renderer state was already correct.
- The setter now updates the actual screen listData row and queues a next-tick
  repaint instead of rebuilding reentrantly inside the Flash input callback.
- The periodic visible-menu check also repairs stale labels on reopened screens.
- Regression tests cover separate inherited row objects, both selection directions,
  next-tick repaint, no repeated repaint, and recovery of a stale reopened label.
- CTest: 3/3 passed. Deployed via a mod-only Lua reload; native ASI unchanged.

## 0.1.3 options layout

The glyph option uses MenuIcon_Controls, matching Controller bindings, and is
placed immediately after Crabe_PadBindings. Selection follows the selected row
when repositioning an already-open list. The native renderer is unchanged.

## 0.1.4 focus preservation on glyph changes

Menu.Rebuild rebuilds the entire AvaList and resets the start selection. The
Button_LR_Toggle input handler already calls GetItemData(id, "enumValue") and
SetEnumText after NextEnumValue returns. Removed the extra queued menu rebuild
for normal glyph changes; the callback still updates the physical row immediately.
The regression mock now reproduces the real rebuild-to-first-row behavior and
checks selection is preserved in both toggle directions and after the periodic poll.

## 0.1.5 - controller icon widget fix

The game's decompiled Button_LR_Toggle has no iconClip, FetchIcon, or SetButtonIcon;
setting row.icon cannot display an image. The native BaseTextButton_IN3 used by
widgetType Button has all three and loads ui/icons/MenuIcon_Controls. Use that
widget for the glyph row, with the choice in the label. Select toggles the setting
and calls Menu.SetElement for that row, which updates its label and icon without
rebuilding the list or moving focus. Settings tests exercise both Select directions,
bridge/persistence updates, exact per-row Flash calls, and focus preservation.

Live game console check: the row is widgetType Button, has
MenuIcon_Controls, and immediately follows Crabe_PadBindings. Select changed
PlayStation 4 to Xbox and back, updating bridge values 1 -> 0 -> 1 while
selectedIndex remained 5. The saved preference was restored to PlayStation 4.
User-provided screenshots in the README confirm the controller icon, current
choice label, skill-menu glyphs, and gameplay prompts.
