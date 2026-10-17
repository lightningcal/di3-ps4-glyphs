"""Regenerate the native hash table from reviewed, hash-only metadata (no game installation needed)."""
import json
from pathlib import Path
root = Path(__file__).resolve().parent.parent
names = json.loads((root/'assets/atlas.json').read_text())['cells']
records = json.loads((root/'assets/glyph-map.json').read_text())
hashes = {}
for record in records:
    index = names.index(record['glyph'])
    for key in ('bgraFingerprint', 'rgbaFingerprint'):
        h = record[key]
        if h in hashes and hashes[h] != index:
            raise ValueError('Conflicting texture identities: ' + h)
        hashes[h] = index
lines = ['// Hash identifiers only; no vanilla pixels. See assets/glyph-map.json.', '#pragma once',
         '#include <cstdint>', 'struct GlyphFingerprint { std::uint64_t hash; unsigned cell; };',
         'inline constexpr const wchar_t* kGlyphFiles[] = {']
lines += [f'    L"{name}.png",' for name in names]
lines += ['};', 'inline constexpr GlyphFingerprint kGlyphFingerprints[] = {']
lines += [f'    {{0x{h}ull, {i}}},' for h, i in sorted(hashes.items())]
lines += ['};']
(root/'src/glyph_fingerprints.hpp').write_text('\n'.join(lines)+'\n', encoding='utf8')
print(f'Generated {len(hashes)} fingerprints for {len(records)} game icon names.')
