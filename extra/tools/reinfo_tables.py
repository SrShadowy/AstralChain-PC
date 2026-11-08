#!/usr/bin/env python3
"""Generate extra/ui/overlay/ac_reinfo_tables.h from the game's own text archives.

Reads game/romfs.bin -> /Text/Text.pkz -> menu_text_USen.bin (area and mission
names) and GameWord_USen.bin (boss names), then writes the C tables used by the
RE Info overlay (extra/ui/overlay/ac_reinfo.cpp).

PTD layout (little endian): "PTD\\0" + u32 version + u32 count at +0x0C +
u32 key_table_off at +0x10. Every record is 16 bytes {hash, self-relative string
offset, chars, bytes}; the string sits at record_off + rel_offset, is UTF-16LE
biased by 0x2626 and NUL terminated, and bytes == chars * 2.

The key table (at key_table_off, stride 16) holds the hashed key names; the value
tables are separate 16-byte-stride runs elsewhere in the file. Those runs can
start on any 2-byte boundary (the boss names start at an odd 4-byte offset), so
the scanner below steps by 2 - stepping by 4 silently skips whole tables.

Dev tool, not part of the build: run it after updating romfs.bin.
    python3 extra/tools/reinfo_tables.py
"""
from pathlib import Path
import struct
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[2]
ROMFS = PROJECT / 'game' / 'romfs.bin'
OUT = PROJECT / 'extra' / 'ui' / 'overlay' / 'ac_reinfo_tables.h'
BIAS = 0x2626
NONE = 0xFFFFFFFF
LIMIT = 20000  # longest plausible single string (characters)


# ---- RomFS -------------------------------------------------------------
def romfs_lookup(tree, wanted):
    """Return (file_offset, size) of `wanted` ('/Text/Text.pkz') in the image."""
    with tree.open('rb') as f:
        fields = struct.unpack('<10Q', f.read(80))
        dir_meta, file_meta, data_off = fields[3:6], fields[7:9], fields[9]
        f.seek(dir_meta[0]); D = f.read(dir_meta[1])
        f.seek(file_meta[0]); F = f.read(file_meta[1])

    def dent(rel):
        return struct.unpack_from('<6I', D, rel)

    def fent(rel):
        return struct.unpack_from('<IIQQII', F, rel)

    def path_of(name_bytes, prefix):
        return prefix + '/' + name_bytes.decode('utf-8', 'replace')

    def walk(drel, prefix, depth):
        child = dent(drel)[2]
        entry = child
        while entry not in (NONE, 0):
            p, sib, _c, fil, _h, namelen = dent(entry)
            name = D[entry + 24:entry + 24 + namelen]
            path = path_of(name, prefix)
            if depth < 4:
                found = walk(entry, path, depth + 1)
                if found:
                    return found
            entry = sib
        files = dent(drel)[3]
        entry = files
        while entry not in (NONE, 0):
            p, sib, doff, size, _h, namelen = fent(entry)
            if path_of(F[entry + 32:entry + 32 + namelen], prefix) == wanted:
                return data_off + doff, size
            entry = sib
        return None

    found = walk(0, '', 0)
    if not found:
        raise SystemExit(f'{wanted} not found in {tree}')
    return found


# ---- PKZ ---------------------------------------------------------------
def read_pkz(romfs, offset, size):
    """Yield (member_name, compressed_bytes) for every member of the PKZ."""
    with romfs.open('rb') as f:
        f.seek(offset)
        magic, version, declared, count, table_off, names_size = struct.unpack('<4sIQIIQ', f.read(32))
        if magic != b'pkzl' or version != 0x10000:
            raise SystemExit('unsupported PKZ header (run setup_game first)')
        if declared != size:
            raise SystemExit('PKZ size does not match its RomFS extent')
        f.seek(offset + table_off)
        table = f.read(count * 32)
        names = f.read(names_size)
        if not names.startswith(b'ZStandard\0'):
            raise SystemExit('unsupported PKZ compression method')
        for i in range(count):
            name_off, _expanded, data_off, packed = struct.unpack_from('<QQQQ', table, i * 32)
            end = names.find(b'\0', name_off)
            name = names[name_off:end].decode('utf-8')
            f.seek(offset + data_off)
            yield name, f.read(packed)


def zstd_decompress(blob):
    try:
        import zstandard
        return zstandard.ZstdDecompressor().decompress(blob, max_output_size=64 << 20)
    except ImportError:
        return subprocess.run(['zstd', '-d', '-q', '-c'], input=blob,
                              stdout=subprocess.PIPE, check=True).stdout


# ---- PTD ---------------------------------------------------------------
class Ptd:
    def __init__(self, data):
        if data[:4] != b'PTD\0':
            raise SystemExit('not a PTD text archive')
        self.data = data
        self.count = struct.unpack_from('<I', data, 12)[0]
        self.key_off = struct.unpack_from('<I', data, 16)[0]

    def string_at(self, off):
        d, out, i = self.data, [], off
        if off < 0 or off + 1 >= len(d):
            return None
        while i + 1 < len(d):
            ch = (struct.unpack_from('<H', d, i)[0] - BIAS) & 0xFFFF
            if ch == 0:
                return ''.join(out)
            if ch < 0x09:
                return None
            out.append(chr(ch))
            if len(out) > LIMIT:
                return None
            i += 2
        return None

    def record(self, base, k):
        off = base + 16 * k
        if off + 16 > len(self.data):
            return None
        h, rel, chars, size = struct.unpack_from('<4I', self.data, off)
        if not 1 <= chars <= LIMIT or size != chars * 2:
            return None
        return h, self.string_at(off + rel)

    def keys(self):
        """[(hash, key_string)] in key-table order."""
        out = []
        for k in range(self.count):
            rec = self.record(self.key_off, k)
            out.append(rec if rec else (0, None))
        return out

    def find_value_table(self, prefix):
        """{id: name} for every key named prefix + id.

        The archive stores the values in separate 16-byte records whose first
        word is the key hash; those records can sit on any 2-byte boundary (the
        boss-name table starts at 0x988a, which a 4-byte scan misses - that is
        how earlier extraction landed in the hash-only copy at 0xa448, where
        every string is empty). One pass over the file buckets every parsable
        record by hash, then each key picks its own string.
        """
        keymap = dict(self.keys())
        wanted = {h for h, s in keymap.items() if s and s.startswith(prefix)}
        d, start = self.data, self.key_off + 16 * self.count
        found = {}
        for pos in range(start, len(d) - 16, 2):
            h, rel, chars, size = struct.unpack_from('<4I', d, pos)
            if h not in wanted or size != chars * 2 or not 1 <= chars <= LIMIT:
                continue
            text = self.string_at(pos + rel)
            if text:
                found.setdefault(h, set()).add(text)
        result = {}
        for h in wanted:
            texts = found.get(h)
            if not texts:
                continue
            if len(texts) > 1:
                raise SystemExit(f'{keymap[h]}: ambiguous values {sorted(texts)}')
            result[keymap[h][len(prefix):]] = next(iter(texts))
        return result


# ---- emit --------------------------------------------------------------
def join_lines(text):
    """Collapse the game's hard line breaks: 'ASTRAEUS-\\r\\nCAPANEUS'."""
    parts = text.replace('\r\n', '\n').split('\n')
    out = parts[0]
    for part in parts[1:]:
        out += part if out.endswith('-') else ' ' + part
    return ' '.join(out.split())


# shortcut: the overlay uses ImGui's default font (Latin-1 only), so Greek
# letters in the archive would draw as '?'. Transliterate until a font with
# Greek coverage is shipped; update here when that happens.
TRANSLITERATE = {'α': 'alpha', 'β': 'beta', 'γ': 'gamma', 'δ': 'delta', 'Δ': 'delta',
                 'ε': 'epsilon', 'μ': 'mu', 'Ω': 'omega', 'Σ': 'sigma'}


def ascii_text(text):
    out = []
    for ch in text:
        if ch in TRANSLITERATE:
            prev = out[-1] if out else ''
            out.append(TRANSLITERATE[ch] if prev in (' ', '') else ' ' + TRANSLITERATE[ch])
        else:
            out.append(ch)
    return ''.join(out)


def c_literal(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def emit(table, name, title):
    lines = [f'static const AcNameEntry {name}[] = {{  // {title}']
    curr = '    '
    for key in sorted(table):
        item = f'{{0x{key}, {c_literal(table[key])}}}, '
        if len(curr) + len(item) > 100:
            lines.append(curr.rstrip())
            curr = '    ' + item
        else:
            curr += item
    if curr.strip():
        lines.append(curr.rstrip())
    lines.append('};')
    return lines


def emit_scenes(scenes):
    lines = ['static const AcSceneEntry k_ac_scenes[] = {  // 54 scenes (canonical area & case name)']
    curr = '    '
    for sid, aid, name in scenes:
        item = f'{{0x{sid:04x}, 0x{aid:04x}, {c_literal(name)}}}, '
        if len(curr) + len(item) > 100:
            lines.append(curr.rstrip())
            curr = '    ' + item
        else:
            curr += item
    if curr.strip():
        lines.append(curr.rstrip())
    lines.append('};')
    return lines



def main():
    if not ROMFS.is_file():
        raise SystemExit(f'{ROMFS} missing (run setup_game first)')
    offset, size = romfs_lookup(ROMFS, '/Text/Text.pkz')
    members = {name: zstd_decompress(blob) for name, blob in read_pkz(ROMFS, offset, size)}

    menu = Ptd(members['menu_text_USen.bin'])
    words = Ptd(members['GameWord_USen.bin'])
    print(f'menu_text_USen.bin: {menu.count} keys; GameWord_USen.bin: {words.count} keys')

    areas = menu.find_value_table('MENU_DATA_AREA_NAME_')
    menu_missions = menu.find_value_table('MENU_MISSION_NAME_')
    file_missions = words.find_value_table('FILE_MISSION_')
    bosses = words.find_value_table('EVENT_BOSS_NAME_')

    areas = {k: ascii_text(join_lines(v)) for k, v in areas.items()}
    bosses = {k: ascii_text(join_lines(v)) for k, v in bosses.items()}

    # Merge campaign missions (GameWord FILE_MISSION_) and postgame (menu MENU_MISSION_NAME_).
    missions = {}
    for k, v in file_missions.items():
        if len(k) == 4 and all(c in '0123456789abcdefABCDEF' for c in k):
            missions[k.lower()] = ascii_text(join_lines(v))
    for k, v in menu_missions.items():
        if len(k) == 4 and all(c in '0123456789abcdefABCDEF' for c in k):
            missions[k.lower()] = ascii_text(join_lines(v))

    files = {
        '1': 'Startup', '2': 'Awake', '3': 'Link', '4': 'Siege',
        '5': 'Accord', '6': 'Complicit', '7': 'Wild', '8': 'Peace',
        '9': 'Salvation', 'a': 'Madness', 'b': 'Reckoning', 'c': 'Rebirth',
    }

    legions = {
        '0': 'Sword Legion', '1': 'Arrow Legion', '2': 'Arm Legion',
        '3': 'Beast Legion', '4': 'Axe Legion',
    }

    # Sanity checks.
    if len(areas) != 22:
        raise SystemExit(f'area: expected 22 names, got {len(areas)}')
    if len(bosses) != 82:
        raise SystemExit(f'boss: expected 82 names, got {len(bosses)}')
    if len(missions) < 290:
        raise SystemExit(f'missions: expected >= 290, got {len(missions)}')

    lines = [
        "// [extra] Id -> display-name tables extracted from the game's own text archives.",
        '// Source: romfs Text.pkz -> menu_text_USen.bin and GameWord_USen.bin.',
        '// Generated by extra/tools/reinfo_tables.py - do not hand-edit.',
        '#pragma once',
        '',
        'struct AcNameEntry {',
        '    unsigned id;',
        '    const char *name;',
        '};',
        '',
        'struct AcSceneEntry {',
        '    unsigned scene_id;',
        '    unsigned area_id;',
        '    const char *case_name;',
        '};',
        '',
    ]
    lines += emit(areas, 'k_ac_areas', f'{len(areas)} areas')
    lines.append('')
    lines += emit(missions, 'k_ac_missions', f'{len(missions)} missions (campaign + olive)')
    lines.append('')
    lines += emit(bosses, 'k_ac_bosses', f'{len(bosses)} boss name ids')
    lines.append('')
    lines += emit(files, 'k_ac_files', f'{len(files)} story files')
    lines.append('')
    lines += emit(legions, 'k_ac_legions', f'{len(legions)} legion types')
    lines.append('')

    scene_areas = {
        0x1100: 0x51f0, 0x1101: 0x51f0,
        0x1200: 0x5150, 0x1201: 0x5220, 0x1202: 0x5010,
        0x1300: 0x5150, 0x1301: 0x5180, 0x1302: 0x5010, 0x1303: 0x5180, 0x1304: 0x5010,
        0x1400: 0x5150, 0x1401: 0x51c0, 0x1402: 0x5010, 0x1403: 0x51c0, 0x1404: 0x51a0, 0x1405: 0x51a0,
        0x1500: 0x5150, 0x1501: 0x5110, 0x1502: 0x51d0, 0x1503: 0x51d0, 0x1504: 0x51d0, 0x1505: 0x5010, 0x1506: 0x5010,
        0x1600: 0x5150, 0x1601: 0x5130, 0x1602: 0x5130, 0x1603: 0x5130, 0x1604: 0x5130, 0x1605: 0x5130, 0x1606: 0x5130, 0x1607: 0x5010, 0x1608: 0x5010,
        0x1700: 0x5210, 0x1701: 0x5210, 0x1702: 0x5210, 0x1703: 0x5010,
        0x1800: 0x5150, 0x1801: 0x5200, 0x1802: 0x5200, 0x1803: 0x5200, 0x1804: 0x5010,
        0x1900: 0x5150, 0x1901: 0x51c0, 0x1902: 0x51c0, 0x1903: 0x51c0, 0x1904: 0x5010,
        0x1a00: 0x5230, 0x1a01: 0x5110, 0x1a02: 0x5110, 0x1a03: 0x5010,
        0x1b00: 0x5100, 0x1b01: 0x5100, 0x1b02: 0x5010,
        0x1c00: 0x5150,
    }
    cases = menu.find_value_table('MENU_SELECT_CASE_NAME_')
    scene_list = []
    for sid, aid in sorted(scene_areas.items()):
        if sid == 0x1c00:
            cname = 'Securing the Ark'
        else:
            k = f'{((sid >> 8) - 0x10):02d}{sid & 0xff:02x}'
            cname = ascii_text(join_lines(cases.get(k, '')))
        scene_list.append((sid, aid, cname))

    lines += emit_scenes(scene_list)
    lines.append('')
    OUT.write_text('\n'.join(lines), encoding='utf-8')
    print(f'wrote {OUT.relative_to(PROJECT)} ({len(lines)} lines): '
          f'{len(areas)} areas, {len(missions)} missions, {len(bosses)} bosses, '
          f'{len(files)} files, {len(legions)} legions, {len(scene_list)} scenes')


if __name__ == '__main__':
    sys.exit(main())
