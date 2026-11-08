#!/usr/bin/env python3
"""Extract PNG icons, area artwork, legions, ranks, and files from RomFS.

Reads game/romfs.bin, deswizzles Tegra X1 BC3/BC1 textures via libtegra,
applies UVD bounding boxes, and exports organized PNG files to:
/home/shadowy/Imagens/Icons AstralChain/

Usage:
    .venv/bin/python3 extra/tools/extract_icons.py
"""
from pathlib import Path
import ctypes
import io
import os
import struct
import subprocess
import sys

from PIL import Image

PROJECT = Path(__file__).resolve().parents[2]
ROMFS = PROJECT / "game" / "romfs.bin"
OUT_ROOT = Path("/home/shadowy/Imagens/Icons AstralChain")
LIBTEGRA_PATH = Path("/tmp/libtegra.so")

NONE = 0xFFFFFFFF


# ---- Tegra Deswizzle Library --------------------------------------------
def ensure_libtegra():
    if not LIBTEGRA_PATH.exists():
        tegra_c = PROJECT / "astral_chain" / "runtime" / "las" / "nvn" / "tegra.c"
        formats_c = PROJECT / "astral_chain" / "runtime" / "las" / "nvn" / "formats.c"
        cmd = ["cc", "-O2", "-shared", "-fPIC", str(tegra_c), str(formats_c), "-o", str(LIBTEGRA_PATH)]
        subprocess.run(cmd, check=True)
    lib = ctypes.CDLL(str(LIBTEGRA_PATH))
    lib.tegra_deswizzle.argtypes = [
        ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
    ]
    return lib


LIBTEGRA = ensure_libtegra()


# ---- RomFS & PKZ --------------------------------------------------------
def romfs_lookup(tree, wanted):
    with tree.open("rb") as f:
        fields = struct.unpack("<10Q", f.read(80))
        dir_meta, file_meta, data_off = fields[3:6], fields[7:9], fields[9]
        f.seek(dir_meta[0]); D = f.read(dir_meta[1])
        f.seek(file_meta[0]); F = f.read(file_meta[1])

    def dent(rel): return struct.unpack_from("<6I", D, rel)
    def fent(rel): return struct.unpack_from("<IIQQII", F, rel)
    def path_of(nbytes, prefix): return prefix + "/" + nbytes.decode("utf-8", "replace")

    def walk(drel, prefix):
        files = dent(drel)[3]
        entry = files
        while entry not in (NONE, 0):
            p, sib, doff, size, _h, nlen = fent(entry)
            if path_of(F[entry + 32 : entry + 32 + nlen], prefix) == wanted:
                return data_off + doff, size
            entry = sib
        child = dent(drel)[2]
        entry = child
        while entry not in (NONE, 0):
            p, sib, _c, fil, _h, nlen = dent(entry)
            name = D[entry + 24 : entry + 24 + nlen]
            found = walk(entry, path_of(name, prefix))
            if found: return found
            entry = sib
        return None

    res = walk(0, "")
    if not res:
        raise FileNotFoundError(f"{wanted} not found in RomFS")
    return res


def zstd_decompress(blob):
    try:
        import zstandard
        return zstandard.ZstdDecompressor().decompress(blob, max_output_size=64 << 20)
    except ImportError:
        return subprocess.run(["zstd", "-d", "-q", "-c"], input=blob,
                              stdout=subprocess.PIPE, check=True).stdout


def read_pkz(romfs, offset, size):
    with romfs.open("rb") as f:
        f.seek(offset)
        magic, version, declared, count, table_off, names_size = struct.unpack("<4sIQIIQ", f.read(32))
        f.seek(offset + table_off)
        table = f.read(count * 32)
        names = f.read(names_size)
        for i in range(count):
            name_off, _expanded, data_off, packed = struct.unpack_from("<QQQQ", table, i * 32)
            end = names.find(b"\0", name_off)
            name = names[name_off:end].decode("utf-8")
            f.seek(offset + data_off)
            yield name, f.read(packed)


def parse_dat(data):
    magic, count, pos_off, ext_off, name_off, sz_off = struct.unpack("<4sIIIII", data[:24])
    pos_table = struct.unpack_from(f"<{count}I", data, pos_off)
    sz_table = struct.unpack_from(f"<{count}I", data, sz_off)
    name_len = struct.unpack_from("<I", data, name_off)[0]
    names = []
    curr = name_off + 4
    for _ in range(count):
        n = data[curr : curr + name_len].split(b"\0")[0].decode("ascii", errors="replace")
        names.append(n)
        curr += name_len
    files = {}
    for i in range(count):
        files[names[i]] = data[pos_table[i] : pos_table[i] + sz_table[i]]
    return files


# ---- Tegra Decoding -----------------------------------------------------
def make_dds_header(w, h, fourcc=b"DXT5"):
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<I", header, 4, 124)
    struct.pack_into("<I", header, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000)
    struct.pack_into("<I", header, 12, h)
    struct.pack_into("<I", header, 16, w)
    struct.pack_into("<I", header, 20, max(1, ((w + 3) // 4) * 16))
    struct.pack_into("<I", header, 28, 1)
    struct.pack_into("<I", header, 76, 32)
    struct.pack_into("<I", header, 80, 0x4)
    header[84:88] = fourcc
    struct.pack_into("<I", header, 108, 0x1000)
    return bytes(header)


def level_block_height(bh_log2, hb):
    while bh_log2 > 0 and (8 << (bh_log2 - 1)) >= hb:
        bh_log2 -= 1
    return bh_log2


def decode_texture(wta_data, wtp_data):
    info_off = struct.unpack_from("<I", wta_data, 0x1C)[0]
    xt1 = wta_data[info_off:]
    fmt = struct.unpack_from("<I", xt1, 0x1C)[0]
    w = struct.unpack_from("<I", xt1, 0x20)[0]
    h = struct.unpack_from("<I", xt1, 0x24)[0]
    pkg = struct.unpack_from("<I", xt1, 0x34)[0]
    bh_log2 = pkg & 0xFF

    if fmt == 0x44:
        wb, hb, bpb, fourcc = (w + 3) // 4, (h + 3) // 4, 16, b"DXT5"
    elif fmt == 0x42:
        wb, hb, bpb, fourcc = (w + 3) // 4, (h + 3) // 4, 8, b"DXT1"
    else:
        raise ValueError(f"Unsupported texture format: 0x{fmt:x}")

    actual_bh = level_block_height(bh_log2, hb)
    unswizzled_sz = wb * hb * bpb
    src_buf = ctypes.create_string_buffer(wtp_data, len(wtp_data))
    dst_buf = ctypes.create_string_buffer(unswizzled_sz)

    LIBTEGRA.tegra_deswizzle(
        ctypes.byref(src_buf), ctypes.byref(dst_buf),
        wb, hb, 1, bpb, actual_bh, 0,
    )
    dds = make_dds_header(w, h, fourcc) + dst_buf.raw
    return Image.open(io.BytesIO(dds))


def extract_pair(dat_blob, dtt_blob, target_png):
    dat_files = parse_dat(zstd_decompress(dat_blob))
    dtt_files = parse_dat(zstd_decompress(dtt_blob))
    wta_name = [k for k in dat_files.keys() if k.endswith(".wta")][0]
    wtp_name = [k for k in dtt_files.keys() if k.endswith(".wtp")][0]

    im = decode_texture(dat_files[wta_name], dtt_files[wtp_name])
    uvd_keys = [k for k in dat_files.keys() if k.endswith(".uvd")]
    if uvd_keys:
        uvd = dat_files[uvd_keys[0]]
        cw = int(struct.unpack_from("<f", uvd, 0x84)[0])
        ch = int(struct.unpack_from("<f", uvd, 0x88)[0])
        if 0 < cw <= im.width and 0 < ch <= im.height:
            im = im.crop((0, 0, cw, ch))

    target_png.parent.mkdir(parents=True, exist_ok=True)
    im.save(target_png)


# ---- Extraction Catalog -------------------------------------------------
AREA_NAMES = {
    "archive_5000": "01_Hypermegafloat_Ark", "archive_5010": "02_Astral_Plane",
    "archive_5100": "03_Restricted_District", "archive_5110": "04_Aegis_Research_Institute",
    "archive_5120": "05_Zone_09", "archive_5130": "06_Sector_V",
    "archive_5140": "07_Zone_10", "archive_5150": "08_Ark_National_Police_Headquarters",
    "archive_5160": "09_Special_Ops_Bureau_HQ", "archive_5170": "10_Zone_30",
    "archive_5180": "11_Maison_Forest", "archive_5190": "12_Zone_32",
    "archive_51a0": "13_Ark_Mall", "archive_51b0": "14_Zone_33",
    "archive_51c0": "15_Harmony_Square", "archive_51d0": "16_Harmony_Square_Station",
    "archive_51e0": "17_Zone_36", "archive_51f0": "18_Central_City",
    "archive_5200": "19_Eastside_Highway", "archive_5210": "20_Rayleigh_Plaza",
    "archive_5220": "21_Grand_Avenue", "archive_5230": "22_Ark_Sewer_Management",
}

LANDMARK_NAMES = {
    "archive_6010": "Five-Story_Pagoda", "archive_6011": "Bar_District",
    "archive_6020": "View_from_the_Rooftops", "archive_6021": "Walls",
    "archive_6030": "Harmony_Square_Media_Center", "archive_6031": "Harmony_Square_West",
    "archive_6032": "Harmony_Square_East", "archive_6040": "Subway_Platform",
    "archive_6041": "Trains", "archive_6050": "Containment_Wall_V",
    "archive_6051": "Sector_V_Camp", "archive_6052": "Hermit_Base",
    "archive_6060": "Arboreal_Atrium", "archive_6061": "Food_Court",
    "archive_6070": "Pipeline", "archive_6071": "Drainage_Pit",
    "archive_6080": "ARI_Entrance", "archive_6081": "Yoseph_Calverts_Laboratory",
    "archive_6090": "View_from_Hals_Hideout", "archive_6091": "Dump",
    "archive_6092": "Sewer_System", "archive_60a0": "Arks_Trash_Problem",
    "archive_60a1": "Collective_Housing", "archive_60b0": "Underground_Parking_Garage",
    "archive_60b1": "Heliport", "archive_60b2": "Restrooms",
    "archive_60c0": "Highway_Junction", "archive_60d0": "Red_Moon",
}

FILE_NAMES = {
    "file_thumb_01": "File_01_Startup", "file_thumb_02": "File_02_Awake",
    "file_thumb_03": "File_03_Link", "file_thumb_04": "File_04_Siege",
    "file_thumb_05": "File_05_Accord", "file_thumb_06": "File_06_Complicity",
    "file_thumb_07": "File_07_Wild", "file_thumb_08": "File_08_Rest",
    "file_thumb_09": "File_09_Salvation", "file_thumb_10": "File_10_Madness",
    "file_thumb_11": "File_11_Reckoning", "file_thumb_12": "File_12_Rebirth",
}

LEGION_NAMES = {
    "legion_style_thumb_00a0": "Legion_Sword",
    "legion_style_thumb_00c0": "Legion_Arrow",
    "legion_style_thumb_02a0": "Legion_Arm",
    "legion_style_thumb_03a0": "Legion_Beast",
    "legion_style_thumb_05a0": "Legion_Axe",
}

CHARA_NAMES = {
    "chara_thumb_01": "Protagonist_Male",
    "chara_thumb_02": "Protagonist_Female",
}

EVAL_RANKS = {
    "all_rank_01": "Rank_Evaluation_01_D",
    "all_rank_02": "Rank_Evaluation_02_C",
    "all_rank_03": "Rank_Evaluation_03_B",
    "all_rank_04": "Rank_Evaluation_04_A",
    "all_rank_05": "Rank_Evaluation_05_S",
    "all_rank_06": "Rank_Evaluation_06_S_Plus",
    "all_rank_99": "Rank_Evaluation_99_None",
}

WEAPONS = {
    "status_weapon_01": "Weapon_XBaton",
    "status_weapon_02": "Weapon_Blaster",
    "status_weapon_03": "Weapon_Gladius",
}


def extract_package(pkz_path, name_map, out_dir):
    off, sz = romfs_lookup(ROMFS, pkz_path)
    members = dict(read_pkz(ROMFS, off, sz))
    count = 0
    for dat_name, dat_blob in members.items():
        if not dat_name.endswith(".dat"): continue
        base = dat_name[:-4]
        dtt_name = base + ".dtt"
        if dtt_name not in members: continue
        out_name = name_map.get(base, base) + ".png"
        extract_pair(dat_blob, members[dtt_name], out_dir / out_name)
        count += 1
    return count


def main():
    if not ROMFS.exists():
        sys.exit(f"ROMFS image not found: {ROMFS}")
    print(f"Astral Chain Icon Extractor -> {OUT_ROOT}")

    # 1. Areas & Landmarks
    off, sz = romfs_lookup(ROMFS, "/miscTex/archives/archives.pkz")
    arch = dict(read_pkz(ROMFS, off, sz))
    for k, name in AREA_NAMES.items():
        extract_pair(arch[f"{k}.dat"], arch[f"{k}.dtt"], OUT_ROOT / "Areas" / f"{name}.png")
    for k, name in LANDMARK_NAMES.items():
        extract_pair(arch[f"{k}.dat"], arch[f"{k}.dtt"], OUT_ROOT / "Landmarks" / f"{name}.png")
    print(f"Extracted {len(AREA_NAMES)} Areas and {len(LANDMARK_NAMES)} Landmarks.")

    # 2. Main Area Map Overview
    off, sz = romfs_lookup(ROMFS, "/miscTex/data_area_thumb/data_area_thumb.pkz")
    m = dict(read_pkz(ROMFS, off, sz))
    extract_pair(m["data_area_thumb_5000.dat"], m["data_area_thumb_5000.dtt"],
                 OUT_ROOT / "Areas" / "Central_City_Harmony_Square_Overview.png")

    # 3. Heliports (Fast Travel Zones)
    n = extract_package("/miscTex/heliport_thumb/heliport_thumb.pkz", {}, OUT_ROOT / "Heliports")
    print(f"Extracted {n} Heliport zone thumbnails.")

    # 4. Files
    n = extract_package("/miscTex/file_thumb/file_thumb.pkz", FILE_NAMES, OUT_ROOT / "Files")
    print(f"Extracted {n} Story File banners.")

    # 5. Cases
    n = extract_package("/miscTex/file_case_thumb/file_case_thumb.pkz", {}, OUT_ROOT / "Cases")
    print(f"Extracted {n} Chapter/Case thumbnails.")

    # 6. Legions
    n = extract_package("/miscTex/legion_style_thumb/legion_style_thumb.pkz", LEGION_NAMES, OUT_ROOT / "Legions")
    print(f"Extracted {n} Legion style icons.")

    # 7. Characters
    n = extract_package("/miscTex/chara_thumb/chara_thumb.pkz", CHARA_NAMES, OUT_ROOT / "Characters")
    print(f"Extracted {n} Protagonist portraits.")

    # 8. Ranks (Police + Evaluation)
    n1 = extract_package("/miscTex/rank_thumb/rank_thumb.pkz", {}, OUT_ROOT / "Ranks" / "Police_Ranks")
    n2 = extract_package("/miscTex/all_rank/all_rank.pkz", EVAL_RANKS, OUT_ROOT / "Ranks" / "Evaluation_Badges")
    print(f"Extracted {n1} Police Ranks and {n2} Evaluation Badges.")

    # 9. Weapons
    n = extract_package("/miscTex/status_thumb/status_thumb.pkz", WEAPONS, OUT_ROOT / "Weapons")
    print(f"Extracted {n} Weapon icons.")

    # 10. Ability Slots & Shortcuts
    n1 = extract_package("/miscTex/abilityslot_thumb/abilityslot_thumb.pkz", {}, OUT_ROOT / "Abilities")
    n2 = extract_package("/miscTex/shortcut_thumb/shortcut_thumb.pkz", {}, OUT_ROOT / "Shortcuts")
    print(f"Extracted {n1} Ability Slots and {n2} Shortcut icons.")

    # 11. Items
    n = extract_package("/miscTex/item_thumb/item_thumb.pkz", {}, OUT_ROOT / "Items")
    print(f"Extracted {n} Item icons.")

    print("\nAll assets successfully extracted to:", OUT_ROOT)


if __name__ == "__main__":
    main()
