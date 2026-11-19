#!/usr/bin/env python3
"""
tools/extract_symbols.py — Extrator estático de símbolos, vtables e assinaturas (Fase 1 AC-SDK).

Analisa a seção .rodata e .text do binário NSO (ARM64 v1.0.1), mapeia xrefs de strings
de formatos da engine da PlatinumGames (.dat, .bxm, .uid), mensagens de erro internas
e gera um arquivo symbols.toml estruturado.
"""

from pathlib import Path
import argparse
import struct
import sys

# Ensure src/ is on sys.path for astral_chain imports
ROOT_DIR = Path(__file__).resolve().parent.parent.parent
SRC_DIR = ROOT_DIR / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from astral_chain.shared.nso import Nso

MAIN_MODULE_BASE = 0x7000010000

RESOURCE_EXTENSIONS = [
    b".uid",
    b".bxm",
    b".dat",
    b".wmb",
    b".mot",
    b".csv",
    b".bnk"
]

DEBUG_KEYWORDS = [
    b"Graphic::Startup",
    b"--- StartupShaderDataSet",
    b"--- shader.dat",
    b"EffectShader",
    b"BehaviorSurvCamera",
    b"BehaviorSword",
    b"BehaviorArm",
    b"SAVE:/GameData",
    b"SAVE:/SlotData",
    b"EVENT_BOSS_CODE",
    b"MENU_DATA_AREA_NAME",
    b"CORE_MAP_AREA"
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
        """Build an index of all ADRP instructions by page for instant xref lookup."""
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
                rd = w & 0x1f
                self._adrp_index.setdefault(page, []).append((pc, i, rd))

    def find_xrefs(self, target_addr: int):
        """Find ADRP + ADD pairs targeting target_addr."""
        target_page = target_addr & ~0xfff
        target_off = target_addr & 0xfff
        results = []

        for pc, idx, rd in self._adrp_index.get(target_page, []):
            # Check upcoming 12 instructions for ADD with matching register and offset
            for step in range(1, 13):
                next_idx = idx + step * 4
                if next_idx >= len(self.text):
                    break
                w2 = struct.unpack_from("<I", self.text, next_idx)[0]
                # ADD (immediate) 64-bit: 0x91000000
                if (w2 & 0xff800000) == 0x91000000:
                    rn = (w2 >> 5) & 0x1f
                    imm = (w2 >> 10) & 0xfff
                    if rn == rd and imm == target_off:
                        results.append(pc + step * 4)
        return results

    def find_function_entry(self, pc: int) -> int:
        """Scan backwards from pc to find the function entry point."""
        curr = pc
        while curr > self.text_start and (pc - curr) < 2048:
            curr -= 4
            w = struct.unpack_from("<I", self.text, curr - self.text_start)[0]
            # UDF #0 padding or RET (0xd65f0000) or BR (0xd61f0000)
            if w == 0 or (w & 0xfffffc1f) == 0xd65f0000 or (w & 0xfffffc1f) == 0xd61f0000:
                target = curr + 4
                while target < pc:
                    wt = struct.unpack_from("<I", self.text, target - self.text_start)[0]
                    if wt != 0:
                        break
                    target += 4
                return target
            # STP with writeback: stp x..., x..., [sp, -imm]!
            if (w & 0xffc003e0) == 0xa98003e0:
                return curr
        return pc

    def get_cstring(self, addr: int) -> str:
        """Extract null-terminated string at addr."""
        end = addr
        while end < len(self.nso.image) and self.nso.image[end] != 0:
            end += 1
        raw = bytes(self.nso.image[addr:end])
        return raw.decode("utf-8", errors="replace")

    def scan_pattern_strings(self, pattern: bytes):
        """Find strings containing pattern in .rodata."""
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

    def extract_all(self):
        """Run complete extraction and return catalog dict."""
        catalog = {
            "metadata": {
                "sha256": self.nso.sha256,
                "build_id": self.nso.build_id,
                "main_module_base": f"0x{MAIN_MODULE_BASE:010x}",
                "text_range": f"0x{self.text_start:06x}-0x{self.text_start + self.text_len:06x}",
                "rodata_range": f"0x{self.rodata_start:06x}-0x{self.rodata_start + self.rodata_len:06x}",
                "data_range": f"0x{self.data_start:06x}-0x{self.data_start + self.data_len:06x}"
            },
            "resource_parsers": [],
            "engine_debug_sites": []
        }

        # 1. Resource extensions
        seen_strs = set()
        for ext in RESOURCE_EXTENSIONS:
            for addr, s in self.scan_pattern_strings(ext):
                if s in seen_strs:
                    continue
                seen_strs.add(s)
                xrefs = self.find_xrefs(addr)
                if xrefs:
                    for xref in xrefs:
                        fn_entry = self.find_function_entry(xref)
                        catalog["resource_parsers"].append({
                            "string": s,
                            "string_addr": f"0x{addr:06x}",
                            "xref_site": f"0x{xref:06x}",
                            "func_entry": f"0x{fn_entry:06x}"
                        })

        # 2. Debug & error keywords
        for kw in DEBUG_KEYWORDS:
            for addr, s in self.scan_pattern_strings(kw):
                if s in seen_strs:
                    continue
                seen_strs.add(s)
                xrefs = self.find_xrefs(addr)
                if xrefs:
                    for xref in xrefs:
                        fn_entry = self.find_function_entry(xref)
                        catalog["engine_debug_sites"].append({
                            "string": s,
                            "string_addr": f"0x{addr:06x}",
                            "xref_site": f"0x{xref:06x}",
                            "func_entry": f"0x{fn_entry:06x}"
                        })

        return catalog


def emit_toml(catalog: dict) -> str:
    """Format extracted symbols as clean TOML."""
    lines = [
        "# symbols.toml — Astral Chain (v1.0.1) Static Reverse-Engineering Symbol Map",
        "# Gerado automaticamente via tools/extract_symbols.py (Fase 1 AC-SDK)",
        "",
        "[metadata]",
        f"sha256 = \"{catalog['metadata']['sha256']}\"",
        f"build_id = \"{catalog['metadata']['build_id']}\"",
        f"main_base = {catalog['metadata']['main_module_base']}",
        f"text_range = \"{catalog['metadata']['text_range']}\"",
        f"rodata_range = \"{catalog['metadata']['rodata_range']}\"",
        f"data_range = \"{catalog['metadata']['data_range']}\"",
        "",
        "# -----------------------------------------------------------------------------",
        "# SDK Subsystems & Core Slots",
        "# -----------------------------------------------------------------------------",
        "",
        "[sdk.camera]",
        "vtable = 0x00d5b3d0",
        "method_update = 0x0027f640",
        "director_update = 0x0028b120",
        "free_camera_update = 0x002bd8b0",
        "slot_camera_manager = 0x00db83a0",
        "camera_manager_direct = 0x00dca490",
        "get_camera_manager_fn = 0x0088fa20",
        "pitch_accum_offset = 0x00001284",
        "input_delta_x_offset = 0x000008d8",
        "input_delta_y_offset = 0x000008e0",
        "pos_x_offset = 0x00000b50",
        "",
        "[sdk.ui]",
        "active_menu_slot = 0x00dbec80",
        "active_menu_direct = 0x00d82d18",
        "slot_engine_state = 0x00db8398",
        "engine_state_direct = 0x00dcba30",
        "slot_menu_selection = 0x00dbef78",
        "menu_selection_direct = 0x00d83180",
        "open_menu_fn = 0x0061bf00",
        "close_menu_fn = 0x0061bed8",
        "",
        "[sdk.time]",
        "timer_manager_slot = 0x00db89a0",
        "timer_manager_direct = 0x00dcbfd0",
        "setup_timer_fn = 0x003e3cf4",
        "update_timer_fn = 0x0091dbd0",
        "",
        "[sdk.input]",
        "pad_input_state = 0x00dc2540",
        "pad_state_table = 0x00dc2518",
        "get_stick_vector_fn = 0x009ad0a0",
        "",
        "[sdk.game_state]",
        "slot_save = 0x00db83d8",
        "slot_scene = 0x00db84c8",
        "slot_phase = 0x00db84f8",
        "slot_scenario = 0x00db8930",
        "slot_player_table = 0x00dbb680",
        "slot_mission = 0x00dbc420",
        "slot_boss_active = 0x00dbc928",
        "slot_area = 0x00dbcf10",
        "slot_subarea = 0x00dbcf20",
        "slot_legion = 0x00dc5bb8",
        "slot_voice_opt = 0x00dc13b0",
        "slot_voice_dirty1 = 0x00dc13b8",
        "slot_voice_dirty2 = 0x00dc13c0",
        "slot_voice_se = 0x00dc2908",
        "",
        "[sdk.graphics]",
        "svc_site_display_w = 0x004328c0",
        "svc_site_display_h = 0x004328c8",
        "svc_site_startup_w = 0x0040d5e4",
        "svc_site_pool_size = 0x0040e0b0",
        "svc_site_render_scale = 0x0040f184",
        "svc_site_bufroot = 0x00575a30",
        "svc_site_gbuf = 0x00575c94",
        "svc_site_camera_aspect = 0x009a30bc",
        "svc_site_nohud_23 = 0x009177ec",
        "",
        "# -----------------------------------------------------------------------------",
        "# Discovered Resource Parser Functions (.uid, .bxm, .dat, .wmb)",
        "# -----------------------------------------------------------------------------",
        ""
    ]

    for p in catalog["resource_parsers"]:
        clean_str = p["string"].replace("\\", "\\\\").replace('"', '\\"')
        lines.append("[[resource_parsers]]")
        lines.append(f"string = \"{clean_str}\"")
        lines.append(f"string_addr = {p['string_addr']}")
        lines.append(f"xref_site = {p['xref_site']}")
        lines.append(f"func_entry = {p['func_entry']}")
        lines.append("")

    lines.append("# -----------------------------------------------------------------------------")
    lines.append("# Discovered Engine Debug & Internal System Call Sites")
    lines.append("# -----------------------------------------------------------------------------")
    lines.append("")

    for d in catalog["engine_debug_sites"]:
        clean_str = d["string"].replace("\\", "\\\\").replace('"', '\\"')
        lines.append("[[engine_debug_sites]]")
        lines.append(f"string = \"{clean_str}\"")
        lines.append(f"string_addr = {d['string_addr']}")
        lines.append(f"xref_site = {d['xref_site']}")
        lines.append(f"func_entry = {d['func_entry']}")
        lines.append("")

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
    print(f"[*] Seções mapeadas: .text={extractor.text_len:#x}, .rodata={extractor.rodata_len:#x}, .data={extractor.data_len:#x}")
    print(f"[*] Índice ADRP construído: {len(extractor._adrp_index)} páginas rastreadas")

    catalog = extractor.extract_all()
    print(f"[+] Parser sites encontrados: {len(catalog['resource_parsers'])}")
    print(f"[+] Debug/System sites encontrados: {len(catalog['engine_debug_sites'])}")

    content = emit_toml(catalog)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(content, encoding="utf-8")
    print(f"[✓] Arquivo de símbolos gravado em: {args.out}")


if __name__ == "__main__":
    main()
