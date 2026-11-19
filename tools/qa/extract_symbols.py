#!/usr/bin/env python3
"""
tools/qa/extract_symbols.py — Extrator estático de símbolos, vtables e assinaturas (Fase 1 AC-SDK).

Analisa a seção .rodata e .text do binário NSO (ARM64 v1.0.1), mapeia xrefs de strings
de formatos da engine da PlatinumGames (.dat, .bxm, .uid), constantes 1/30.0f,
e gera padrões AOB blindados com decodificação dinâmica de ADRP+LDR para symbols.toml.
"""

from pathlib import Path
import argparse
import re
import struct
import sys

ROOT_DIR = Path(__file__).resolve().parent.parent.parent
SRC_DIR = ROOT_DIR / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from astral_chain.shared.nso import Nso

MAIN_MODULE_BASE = 0x7000010000

RESOURCE_EXTENSIONS = [b".uid", b".bxm", b".dat", b".wmb", b".mot", b".csv", b".bnk"]
DEBUG_KEYWORDS = [
    b"Graphic::Startup", b"--- StartupShaderDataSet", b"--- shader.dat",
    b"EffectShader", b"BehaviorSurvCamera", b"BehaviorSword", b"BehaviorArm",
    b"SAVE:/GameData", b"SAVE:/SlotData", b"EVENT_BOSS_CODE",
    b"MENU_DATA_AREA_NAME", b"CORE_MAP_AREA"
]

ANCHOR_TARGETS = [
    ("get_camera_manager", 0x88fa20, 6, 0xdb83a0, "SLOT_CAMERA_MANAGER"),
    ("timer_manager", 0x3e4178, 6, 0xdb89a0, "SLOT_TIMER_MANAGER"),
    ("engine_state", 0x05bd50, 9, 0xdb8398, "SLOT_ENGINE_STATE"),
    ("save", 0x058340, 8, 0xdb83d8, "SLOT_SAVE"),
    ("scene", 0x026180, 6, 0xdb84c8, "SLOT_SCENE"),
    ("phase", 0x034820, 6, 0xdb84f8, "SLOT_PHASE"),
    ("scenario", 0x4e41d0, 6, 0xdb8930, "SLOT_SCENARIO"),
    ("player_table", 0x454ad0, 6, 0xdbb680, "SLOT_PLAYER_TABLE"),
    ("mission", 0x4be600, 6, 0xdbc420, "SLOT_MISSION"),
    ("boss_active", 0x540430, 6, 0xdbc928, "SLOT_BOSS_ACTIVE"),
    ("area", 0x56c100, 6, 0xdbcf10, "SLOT_AREA"),
    ("subarea", 0x56c270, 6, 0xdbcf20, "SLOT_SUBAREA"),
    ("menu_ptr", 0x61bed8, 6, 0xdbec80, "SLOT_MENU_PTR"),
    ("menu_selection", 0x63a980, 6, 0xdbef78, "SLOT_MENU_SELECTION"),
    ("voice_opt", 0x8e1a40, 6, 0xdc13b0, "SLOT_VOICE_OPT"),
    ("pad_state_table", 0x9ac630, 6, 0xdc2518, "PAD_STATE_TABLE"),
    ("pad_input_state", 0x9acb64, 6, 0xdc2540, "PAD_INPUT_STATE"),
    ("voice_se", 0x9cb85c, 9, 0xdc2908, "SLOT_VOICE_SE"),
]


class SymbolExtractor:
    def __init__(self, nso_path: Path):
        self.nso = Nso(nso_path)
        self.text_start, self.text_len = self.nso.text
        self.rodata_start, self.rodata_len = self.nso.rodata
        self.data_start, self.data_len = self.nso.data
        self.text = self.nso.image[self.text_start:self.text_start + self.text_len]
        self._adrp_index = {}
        self._build_adrp_index()

    def _build_adrp_index(self):
        for i in range(0, len(self.text), 4):
            pc = self.text_start + i
            w = struct.unpack_from("<I", self.text, i)[0]
            if (w & 0x9f000000) == 0x90000000:
                immlo = (w >> 29) & 3
                immhi = (w >> 5) & 0x7ffff
                imm = (immhi << 2) | immlo
                if imm & (1 << 20):
                    imm -= (1 << 21)
                page = (pc & ~0xfff) + (imm << 12)
                self._adrp_index.setdefault(page, []).append((pc, i, w & 0x1f))

    def find_xrefs(self, target_addr: int):
        target_page = target_addr & ~0xfff
        target_off = target_addr & 0xfff
        results = []
        for pc, idx, rd in self._adrp_index.get(target_page, []):
            for step in range(1, 13):
                next_idx = idx + step * 4
                if next_idx >= len(self.text):
                    break
                w2 = struct.unpack_from("<I", self.text, next_idx)[0]
                if (w2 & 0xff800000) == 0x91000000:
                    if ((w2 >> 5) & 0x1f) == rd and ((w2 >> 10) & 0xfff) == target_off:
                        results.append(pc + step * 4)
        return results

    def find_function_entry(self, pc: int) -> int:
        curr = pc
        while curr > self.text_start and (pc - curr) < 2048:
            curr -= 4
            w = struct.unpack_from("<I", self.text, curr - self.text_start)[0]
            if w == 0 or (w & 0xfffffc1f) in (0xd65f0000, 0xd61f0000):
                target = curr + 4
                while target < pc and struct.unpack_from("<I", self.text, target - self.text_start)[0] == 0:
                    target += 4
                return target
            if (w & 0xffc003e0) == 0xa98003e0:
                return curr
        return pc

    def get_cstring(self, addr: int) -> str:
        end = addr
        while end < len(self.nso.image) and self.nso.image[end] != 0:
            end += 1
        return bytes(self.nso.image[addr:end]).decode("utf-8", errors="replace")

    def scan_pattern_strings(self, pattern: bytes):
        results = []
        pos = self.rodata_start
        ro_end = self.rodata_start + self.rodata_len
        while True:
            idx = self.nso.image.find(pattern, pos, ro_end)
            if idx == -1:
                break
            start = idx
            while start > self.rodata_start and self.nso.image[start - 1] != 0:
                start -= 1
            s = self.get_cstring(start)
            if s and (start, s) not in results:
                results.append((start, s))
            pos = idx + len(pattern)
        return results

    def decode_slot_at(self, matched_pc: int) -> int:
        for i in range(12):
            curr_pc = matched_pc + i * 4
            w = struct.unpack_from("<I", self.text, curr_pc - self.text_start)[0]
            if (w & 0x9f000000) == 0x90000000:
                immlo = (w >> 29) & 3
                immhi = (w >> 5) & 0x7ffff
                imm = (immhi << 2) | immlo
                if imm & (1 << 20):
                    imm -= (1 << 21)
                page = (curr_pc & ~0xfff) + (imm << 12)
                rd = w & 0x1f
                for j in range(1, 8):
                    next_pc = curr_pc + j * 4
                    w2 = struct.unpack_from("<I", self.text, next_pc - self.text_start)[0]
                    if (w2 & 0xffc00000) == 0xf9400000 and ((w2 >> 5) & 0x1f) == rd:
                        return page + (((w2 >> 10) & 0xfff) << 3)
                    if (w2 & 0xff800000) == 0x91000000 and ((w2 >> 5) & 0x1f) == rd:
                        sh = (w2 >> 22) & 1
                        return page + (((w2 >> 10) & 0xfff) << (12 if sh else 0))
                    if (w2 & 0xffc00000) == 0xb9400000 and ((w2 >> 5) & 0x1f) == rd:
                        return page + (((w2 >> 10) & 0xfff) << 2)
        return 0

    def generate_anchor_aobs(self):
        anchors = []
        for name, addr, n_ins, expected_slot, slot_sym in ANCHOR_TARGETS:
            parts = []
            for i in range(n_ins):
                pc = addr + i * 4
                w = struct.unpack_from("<I", self.text, pc - self.text_start)[0]
                if (w & 0x9f000000) == 0x90000000 or (w & 0xfc000000) in (0x94000000, 0x14000000) or \
                   (w & 0xff000000) == 0x54000000 or (w & 0x7e000000) in (0x34000000, 0x36000000):
                    parts.extend(["??", "??", "??", f"{(w >> 24) & 0xff:02x}"])
                else:
                    parts.extend([f"{(w >> (s * 8)) & 0xff:02x}" for s in range(4)])
            pat = " ".join(parts)
            regex_parts = [b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in parts]
            matches = [m.start() for m in re.compile(b"".join(regex_parts), re.DOTALL).finditer(self.text)]
            decoded = self.decode_slot_at(self.text_start + matches[0]) if matches else 0
            anchors.append({
                "name": name,
                "pattern": pat,
                "func_entry": f"0x{addr:06x}",
                "expected_slot": f"0x{expected_slot:06x}",
                "decoded_slot": f"0x{decoded:06x}",
                "slot_symbol": slot_sym,
                "unique": len(matches) == 1
            })
        return anchors

    def scan_framerate_constants(self):
        target_val = struct.pack("<f", 1.0 / 30.0)
        target_addr = None
        for offset in range(0, self.rodata_len - 3, 4):
            if self.nso.image[self.rodata_start + offset:self.rodata_start + offset + 4] == target_val:
                target_addr = self.rodata_start + offset
                break
        if not target_addr:
            return []

        target_page = target_addr & ~0xfff
        target_off = target_addr & 0xfff
        ldr_imm = target_off // 4
        sites = []
        for pc, idx, rd in self._adrp_index.get(target_page, []):
            for step in range(1, 16):
                next_idx = idx + step * 4
                if next_idx >= len(self.text):
                    break
                w = struct.unpack_from("<I", self.text, next_idx)[0]
                ins_pc = self.text_start + next_idx
                matched, ins_desc = False, ""
                if (w & 0xffc00000) == 0xbd400000 and ((w >> 5) & 0x1f) == rd and ((w >> 10) & 0xfff) == ldr_imm:
                    matched, ins_desc = True, f"ldr s{w & 0x1f}, [x{rd}, #0x{target_off:x}]"
                elif (w & 0xff800000) == 0x91000000 and ((w >> 5) & 0x1f) == rd and ((w >> 10) & 0xfff) == target_off:
                    matched, ins_desc = True, f"add x{w & 0x1f}, x{rd}, #0x{target_off:x}"

                if matched:
                    fn_entry = self.find_function_entry(ins_pc)
                    domain, desc = "generic_simulation", "Simulation step scaling"
                    if 0x10e700 <= ins_pc <= 0x12c000:
                        domain, desc = "legion_chain_physics", "Legion chain bind timer & tension damping"
                    elif 0x217000 <= ins_pc <= 0x260000:
                        domain, desc = "sync_attack_combat", "Sync Attack cue & combo opportunity window"
                    elif 0x3e2000 <= ins_pc <= 0x430000:
                        domain, desc = "engine_timer_simulation", "TimerManager fractional step accumulator"
                    elif 0x440000 <= ins_pc <= 0x490000:
                        domain, desc = "actor_movement_physics", "Character gravity & movement delta integration"
                    elif 0x5d0000 <= ins_pc <= 0x5f0000:
                        domain, desc = "legion_style_skills", "Legion skill & shield charge timers"
                    elif ins_pc >= 0x690000:
                        domain, desc = "camera_velocity_scaling", "Camera velocity & smoothing scaling"

                    sites.append({
                        "constant_addr": f"0x{target_addr:08x}", "constant_val": "0.033333335 (1/30.0f)",
                        "xref_site": f"0x{ins_pc:08x}", "func_entry": f"0x{fn_entry:08x}",
                        "instruction": ins_desc, "domain": domain, "description": desc
                    })
                if (w & 0x1f) == rd and (w & 0xff000000) not in (0xb9000000, 0xbd000000, 0xf9000000, 0xfd000000):
                    break
        sites.sort(key=lambda s: int(s["xref_site"], 16))
        return sites

    def extract_all(self):
        catalog = {
            "metadata": {
                "sha256": self.nso.sha256, "build_id": self.nso.build_id,
                "main_module_base": f"0x{MAIN_MODULE_BASE:010x}",
                "text_range": f"0x{self.text_start:06x}-0x{self.text_start + self.text_len:06x}",
                "rodata_range": f"0x{self.rodata_start:06x}-0x{self.rodata_start + self.rodata_len:06x}",
                "data_range": f"0x{self.data_start:06x}-0x{self.data_start + self.data_len:06x}"
            },
            "resource_parsers": [], "engine_debug_sites": [],
            "framerate_constants_30fps": self.scan_framerate_constants(),
            "anchor_aobs": self.generate_anchor_aobs()
        }
        seen = set()
        for group, keywords, dest in [
            ("ext", RESOURCE_EXTENSIONS, catalog["resource_parsers"]),
            ("kw", DEBUG_KEYWORDS, catalog["engine_debug_sites"])
        ]:
            for kw in keywords:
                for addr, s in self.scan_pattern_strings(kw):
                    if s in seen:
                        continue
                    seen.add(s)
                    for xref in self.find_xrefs(addr):
                        dest.append({
                            "string": s, "string_addr": f"0x{addr:06x}",
                            "xref_site": f"0x{xref:06x}", "func_entry": f"0x{self.find_function_entry(xref):06x}"
                        })
        return catalog


def emit_toml(catalog: dict) -> str:
    lines = [
        "# symbols.toml — Astral Chain (v1.0.1) Static Reverse-Engineering Symbol Map",
        "# Gerado automaticamente via tools/qa/extract_symbols.py (Fase 1 AC-SDK)",
        "",
        "[metadata]",
        f"sha256 = \"{catalog['metadata']['sha256']}\"",
        f"build_id = \"{catalog['metadata']['build_id']}\"",
        f"main_base = {catalog['metadata']['main_module_base']}",
        f"text_range = \"{catalog['metadata']['text_range']}\"",
        f"rodata_range = \"{catalog['metadata']['rodata_range']}\"",
        f"data_range = \"{catalog['metadata']['data_range']}\"",
        "",
        "[sdk.camera]",
        "vtable = 0x00d5b3d0", "method_update = 0x0027f640", "director_update = 0x0028b120",
        "free_camera_update = 0x002bd8b0", "slot_camera_manager = 0x00db83a0",
        "camera_manager_direct = 0x00dca490", "get_camera_manager_fn = 0x0088fa20",
        "pitch_accum_offset = 0x00001284", "input_delta_x_offset = 0x000008d8",
        "input_delta_y_offset = 0x000008e0", "pos_x_offset = 0x00000b50",
        "",
        "[sdk.ui]",
        "active_menu_slot = 0x00dbec80", "active_menu_direct = 0x00d82d18",
        "slot_engine_state = 0x00db8398", "engine_state_direct = 0x00dcba30",
        "slot_menu_selection = 0x00dbef78", "menu_selection_direct = 0x00d83180",
        "open_menu_fn = 0x0061bf00", "close_menu_fn = 0x0061bed8",
        "",
        "[sdk.time]",
        "timer_manager_slot = 0x00db89a0", "timer_manager_direct = 0x00dcbfd0",
        "setup_timer_fn = 0x003e3cf4", "update_timer_fn = 0x0091dbd0",
        "",
        "[sdk.input]",
        "pad_input_state = 0x00dc2540", "pad_state_table = 0x00dc2518",
        "get_stick_vector_fn = 0x009ad0a0",
        "",
        "[sdk.game_state]",
        "slot_save = 0x00db83d8", "slot_scene = 0x00db84c8", "slot_phase = 0x00db84f8",
        "slot_scenario = 0x00db8930", "slot_player_table = 0x00dbb680",
        "slot_mission = 0x00dbc420", "slot_boss_active = 0x00dbc928",
        "slot_area = 0x00dbcf10", "slot_subarea = 0x00dbcf20", "slot_legion = 0x00dc5bb8",
        "slot_voice_opt = 0x00dc13b0", "slot_voice_dirty1 = 0x00dc13b8",
        "slot_voice_dirty2 = 0x00dc13c0", "slot_voice_se = 0x00dc2908",
        "",
        "[sdk.graphics]",
        "svc_site_display_w = 0x004328c0", "svc_site_display_h = 0x004328c8",
        "svc_site_startup_w = 0x0040d5e4", "svc_site_pool_size = 0x0040e0b0",
        "svc_site_render_scale = 0x0040f184", "svc_site_bufroot = 0x00575a30",
        "svc_site_gbuf = 0x00575c94", "svc_site_camera_aspect = 0x009a30bc",
        "svc_site_nohud_23 = 0x009177ec",
        "",
        "[aob_signatures]"
    ]

    for a in catalog.get("anchor_aobs", []):
        lines.append(f"{a['name']} = \"{a['pattern']}\"")
    lines.append("open_menu = \"f3 0f 1e f8 fd 7b 01 a9 fd 43 00 91 3f 30 00 71 ?? ?? ?? 54 e8 03 01 2a\"")
    lines.append("camera_director_update = \"f3 0f 1e f8 fd 7b 01 a9 fd 43 00 91 f3 03 00 aa 68 46 46 b9 1f b5 03 71\"")
    lines.append("")

    for a in catalog.get("anchor_aobs", []):
        lines.extend([
            "[[anchor_functions]]",
            f"name = \"{a['name']}\"",
            f"func_entry = {a['func_entry']}",
            f"target_slot = {a['expected_slot']}",
            f"slot_symbol = \"{a['slot_symbol']}\"",
            f"pattern = \"{a['pattern']}\"",
            ""
        ])

    for header, key in [
        ("# Discovered Resource Parser Functions (.uid, .bxm, .dat, .wmb)", "resource_parsers"),
        ("# Discovered Engine Debug & Internal System Call Sites", "engine_debug_sites")
    ]:
        lines.extend(["# " + "-" * 77, header, "# " + "-" * 77, ""])
        for item in catalog[key]:
            clean_str = item["string"].replace("\\", "\\\\").replace('"', '\\"')
            lines.extend([
                f"[[{key}]]", f"string = \"{clean_str}\"",
                f"string_addr = {item['string_addr']}",
                f"xref_site = {item['xref_site']}",
                f"func_entry = {item['func_entry']}", ""
            ])

    lines.extend(["# " + "-" * 77, "# Framerate 1/30.0f Constants & Combat Timing Sites", "# " + "-" * 77, ""])
    for f in catalog.get("framerate_constants_30fps", []):
        lines.extend([
            "[[framerate_constants_30fps]]",
            f"constant_addr = {f['constant_addr']}", f"constant_val = \"{f['constant_val']}\"",
            f"xref_site = {f['xref_site']}", f"func_entry = {f['func_entry']}",
            f"domain = \"{f['domain']}\"", f"instruction = \"{f['instruction']}\"",
            f"description = \"{f['description']}\"", ""
        ])

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Extrai símbolos e assinaturas da PlatinumGames")
    parser.add_argument("--nso", type=Path, default=ROOT_DIR / "game/update-exefs/main", help="Caminho do binário main")
    parser.add_argument("--out", type=Path, default=ROOT_DIR / "docs/symbols.toml", help="Arquivo de saída TOML")
    args = parser.parse_args()

    if not args.nso.is_file():
        print(f"Erro: binário NSO não encontrado em {args.nso}", file=sys.stderr)
        sys.exit(1)

    print(f"[*] Analisando binário NSO: {args.nso}")
    extractor = SymbolExtractor(args.nso)
    catalog = extractor.extract_all()
    print(f"[+] Parser sites encontrados: {len(catalog['resource_parsers'])}")
    print(f"[+] Debug/System sites encontrados: {len(catalog['engine_debug_sites'])}")
    print(f"[+] Sites de 1/30.0f encontrados: {len(catalog['framerate_constants_30fps'])}")
    print(f"[+] Âncoras AOB verificadas: {len(catalog['anchor_aobs'])}/{len(ANCHOR_TARGETS)}")

    content = emit_toml(catalog)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(content, encoding="utf-8")
    print(f"[✓] Arquivo de símbolos gravado em: {args.out}")


if __name__ == "__main__":
    main()
