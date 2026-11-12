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
OUT = PROJECT / 'src' / 'additions' / 'overlay' / 'ac_reinfo_tables.h'
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
        if len(curr) + len(item) > 116:
            lines.append(curr.rstrip())
            curr = '    ' + item
        else:
            curr += item
    if curr.strip():
        lines.append(curr.rstrip())
    lines.append('};')
    return lines


def emit_scenes(scenes):
    lines = [f'static const AcSceneEntry k_ac_scenes[] = {{  // {len(scenes)} stage scenes']
    curr = '    '
    for sid, aid, name in scenes:
        item = f'{{0x{sid:04x}, 0x{aid:04x}, {c_literal(name)}}}, '
        if len(curr) + len(item) > 116:
            lines.append(curr.rstrip())
            curr = '    ' + item
        else:
            curr += item
    if curr.strip():
        lines.append(curr.rstrip())
    lines.append('};')
    return lines


def emit_scenario_steps(steps):
    lines = [f'static const AcScenarioStep k_ac_scenario_steps[] = {{  // {len(steps)} File_Scenario steps']
    curr = '    '
    for h, fnum, pid, aid in steps:
        item = f'{{0x{h:08x}u, {fnum}, 0x{pid:04x}, 0x{aid:04x}}}, '
        if len(curr) + len(item) > 118:
            lines.append(curr.rstrip())
            curr = '    ' + item
        else:
            curr += item
    if curr.strip():
        lines.append(curr.rstrip())
    lines.append('};')
    return lines


def read_dat_member(dat, target):
    cnt, off_tbl, _ext, name_tbl, sz_tbl = struct.unpack_from('<4x5I', dat, 0)
    stride = struct.unpack_from('<I', dat, name_tbl)[0]
    for i in range(cnt):
        end = dat.find(b'\0', name_tbl + 4 + i * stride)
        name = dat[name_tbl + 4 + i * stride:end].decode('ascii', 'replace')
        if name == target:
            foff = struct.unpack_from('<I', dat, off_tbl + i * 4)[0]
            fsz = struct.unpack_from('<I', dat, sz_tbl + i * 4)[0]
            return dat[foff:foff + fsz].decode('shift_jis', 'replace')
    raise SystemExit(f'{target} not found in core.dat')


def scene_to_area(sid, fnum):
    if (sid & 0xF000) == 0x6000:
        return 0x5010
    hi = (sid >> 8) & 0xFF
    table = {
        0x11: 0x5210 if fnum == 7 else (0x5120 if fnum == 8 else 0x5220),
        0x12: 0x51C0, 0x13: 0x5130, 0x14: 0x51A0, 0x15: 0x5230, 0x16: 0x5110,
        0x18: 0x5120, 0x19: 0x5150, 0x1A: 0x5180, 0x1B: 0x51D0, 0x1C: 0x5200,
    }
    return table.get(hi, 0)


def extract_scenario_steps():
    import zlib
    off, sz = romfs_lookup(ROMFS, '/core/core.pkz')
    core_dat = next(zstd_decompress(b) for n, b in read_pkz(ROMFS, off, sz) if n == 'core.dat')
    sc_csv = read_dat_member(core_dat, 'ScenarioCounter.csv')
    pas_csv = read_dat_member(core_dat, 'PlannerAutoSaveList.csv')
    pas_map = {}
    scenes = {}
    for line in pas_csv.splitlines():
        p = [x.strip() for x in line.split(',')]
        if len(p) >= 7 and p[0].startswith('file'):
            sid = int(p[5], 16)
            fnum = int(p[0][4:6]) if p[0][4:6].isdigit() else 0
            pas_map[p[0]] = sid
            scenes.setdefault(sid, scene_to_area(sid, fnum))

    in_scen, steps = False, []
    for line in sc_csv.splitlines():
        p = line.strip().split(',')
        if p[0] == 'File_Scenario':
            in_scen = True
            continue
        if p[0] != '':
            if in_scen:
                break
            continue
        if in_scen and len(p) >= 2 and p[1]:
            sname = p[1]
            praw = p[2] if len(p) > 2 else ''
            comment = p[3] if len(p) > 3 else ''
            pid = int(praw[:4], 16) if len(praw) >= 4 and all(c in '0123456789abcdefABCDEF' for c in praw[:4]) else 0
            fnum = int(sname[4:6]) if sname.startswith('file') and sname[4:6].isdigit() else 0
            fnum = fnum if fnum <= 12 else 0
            aid = scene_to_area(pas_map.get(sname, 0), fnum)
            if not aid and fnum:
                for kw, kaid in (('亜空間', 0x5010), ('ノア空間', 0x5010), ('警察署', 0x5150),
                                 ('大通り', 0x5210 if fnum == 7 else 0x5220), ('メゾフォレスト', 0x5180),
                                 ('モール', 0x51A0), ('地下鉄', 0x51D0), ('研究所', 0x5110),
                                 ('下水道', 0x5230), ('アジト', 0x5120), ('九龍', 0x5120), ('広場', 0x51C0)):
                    if kw in comment:
                        aid = kaid
                        break
            h = zlib.crc32(sname.lower().encode('ascii')) & 0x7FFFFFFF
            steps.append([h, fnum, pid, aid])

    for i in range(1, len(steps)):
        if steps[i][1] == steps[i - 1][1] and steps[i][1] != 0:
            if (steps[i][2] == 0 or (steps[i][2] & 0xFF) >= 0x50) and 0 < (steps[i - 1][2] & 0xFF) < 0x50:
                steps[i][2] = steps[i - 1][2]
            if steps[i][3] == 0:
                steps[i][3] = steps[i - 1][3]
    for i in range(len(steps) - 2, -1, -1):
        if steps[i][1] == steps[i + 1][1] and steps[i][1] != 0:
            if steps[i][2] == 0 or (steps[i][2] & 0xFF) >= 0x50:
                steps[i][2] = steps[i + 1][2]
            if steps[i][3] == 0:
                steps[i][3] = steps[i + 1][3]
    return steps, scenes


def main():
    if not ROMFS.is_file():
        raise SystemExit(f'{ROMFS} missing (run setup_game first)')
    offset, size = romfs_lookup(ROMFS, '/Text/Text.pkz')
    members = {name: zstd_decompress(blob) for name, blob in read_pkz(ROMFS, offset, size)}

    menu = Ptd(members['menu_text_USen.bin'])
    words = Ptd(members['GameWord_USen.bin'])
    core_txt = Ptd(members['Core_text_USen.bin'])

    areas = {k: ascii_text(join_lines(v)) for k, v in menu.find_value_table('MENU_DATA_AREA_NAME_').items()}
    bosses = {k: ascii_text(join_lines(v)) for k, v in words.find_value_table('EVENT_BOSS_NAME_').items()}
    purposes = {k.lower(): ascii_text(join_lines(v)) for k, v in core_txt.find_value_table('CORE_PURPOSE_FILE_').items()}

    missions = {}
    for src in (words.find_value_table('FILE_MISSION_'), menu.find_value_table('MENU_MISSION_NAME_')):
        for k, v in src.items():
            if len(k) == 4 and all(c in '0123456789abcdefABCDEF' for c in k):
                missions[k.lower()] = ascii_text(join_lines(v))

    files = {
        '1': 'Startup', '2': 'Awake', '3': 'Link', '4': 'Siege',
        '5': 'Accord', '6': 'Complicit', '7': 'Wild', '8': 'Peace',
        '9': 'Salvation', 'a': 'Madness', 'b': 'Reckoning', 'c': 'Rebirth',
    }
    legions = {
        '0': 'Sword Legion', '1': 'Arm Legion', '2': 'Beast Legion',
        '3': 'Axe Legion', '4': 'Arrow Legion',
    }
    steps, scene_areas = extract_scenario_steps()
    scene_list = [(sid, aid, areas.get(f'{aid:04x}', '')) for sid, aid in sorted(scene_areas.items())]

    lines = [
        "// [extra] Id -> display-name tables extracted from the game's own archives.",
        '// Generated by tools/qa/reinfo_tables.py - do not hand-edit.',
        '#pragma once', '',
        'struct AcNameEntry {', '    unsigned id;', '    const char *name;', '};', '',
        'struct AcSceneEntry {', '    unsigned scene_id;', '    unsigned area_id;', '    const char *case_name;', '};', '',
        'struct AcScenarioStep {',
        '    unsigned step_hash;',
        '    unsigned char file_num;',
        '    unsigned short purpose_id;',
        '    unsigned short area_id;',
        '};', '',
    ]
    for tbl, cname, desc in ((areas, 'k_ac_areas', f'{len(areas)} areas'),
                             (missions, 'k_ac_missions', f'{len(missions)} missions'),
                             (bosses, 'k_ac_bosses', f'{len(bosses)} bosses'),
                             (files, 'k_ac_files', f'{len(files)} story files'),
                             (legions, 'k_ac_legions', f'{len(legions)} legion types'),
                             (purposes, 'k_ac_purposes', f'{len(purposes)} case objectives')):
        lines += emit(tbl, cname, desc) + ['']
    lines += emit_scenes(scene_list) + ['']
    lines += emit_scenario_steps(steps) + ['']
    OUT.write_text('\n'.join(lines), encoding='utf-8')
    print(f'wrote {OUT.relative_to(PROJECT)} ({len(lines)} lines)')


if __name__ == '__main__':
    sys.exit(main())
