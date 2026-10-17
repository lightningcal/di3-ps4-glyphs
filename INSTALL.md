# Install PlayStation 4 Controller Glyphs v0.1.5

Requires Disney Infinity 3.0: Gold Edition (Steam, Windows/x86) and CrabeLoader
v1.1.0 or newer with native ASI plugin support. A working controller/input setup
is still required; this mod changes displayed button glyphs.

1. Close the game.
2. Install CrabeLoader with CrabeInstaller.exe from:
   https://github.com/LucasLhomme/CrabeLoader/releases/latest
   For manual installation or another existing ASI loader, follow:
   https://github.com/LucasLhomme/CrabeLoader/blob/v1.1.0/README.md#installation
3. Download di3-ps4-glyphs-0.1.5.zip from:
   https://github.com/lightningcal/di3-ps4-glyphs/releases/tag/v0.1.5
   Use the named ZIP under Assets, not the automatic Source code archives.
4. Extract the ZIP into the game folder containing DisneyInfinity3.exe, merging
   the included mods folder. The plugin must end up at:
   mods/ps4-glyphs/PS4Glyphs.asi
   Its sibling files must include main.lua, mod.json, and glyphs/*.png.
   Do not create mods/mods/ps4-glyphs.
5. If native plugins were disabled, set asi = true in the [plugins] section of
   Crabe/crabe.toml and remove PS4Glyphs.asi from its skip list.
6. Launch the game. Under Options, select Controller glyphs: Xbox (below
   Controller bindings) to switch to PlayStation 4. Select again to switch back.

CrabeMenu is optional. Restart after installing or replacing PS4Glyphs.asi or
artwork; F4 reloads only Lua. Preferences are saved immediately to
mods/ps4-glyphs/settings.json.

Update: close the game, then extract the new distribution over the existing mod.
Your settings.json is preserved because it is not included in the ZIP.
Uninstall: close the game and remove only mods/ps4-glyphs.

If the option is missing, check loader.log for PS4Glyphs.asi and
"PS4 glyph renderer ready", confirm CrabeLoader is at least v1.1.0, check the
folder layout above, and confirm native plugins are enabled.
