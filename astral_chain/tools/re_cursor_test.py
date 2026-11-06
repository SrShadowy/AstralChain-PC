"""Self-test for astral_chain/tools/re_cursor.py.

The RE helper decodes real ARM64 shapes (the prologue of a function that
materialises a global, a jump table entry) without a game image, so its
decoder can be checked here with synthetic words. Run it directly or from
extra/tools/audit.sh; a change that breaks the decoder fails the audit.

  python -m astral_chain.tools.re_cursor_test
"""

import array
import struct
import sys


def _adrp(rd, pc, page):
    delta = ((page & ~0xFFF) - (pc & ~0xFFF)) >> 12
    imm = delta & 0x1FFFFF  # 21-bit two's complement, immlo[1:0] | immhi[20:2]
    return 0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7FFFF) << 5) | rd


def _add(rd, rn, imm, sh=0):
    return 0x91000000 | (sh << 22) | (imm << 10) | (rn << 5) | rd


def _ldr64(rt, rn, off):
    return 0xF9400000 | ((off // 8) << 10) | (rn << 5) | rt


def _ldr32(rt, rn, off):
    return 0xB9400000 | ((off // 4) << 10) | (rn << 5) | rt


class Fake:
    """The slice of Image that build_refs uses."""

    def __init__(self, blob, text=(0x1000, 0x3000), ro=(0x5000, 0x100)):
        self.nso = type("N", (), {})()
        self.nso.image = blob
        self.text_start, self.text_size = text
        self.text_end = self.text_start + self.text_size
        self.ro_start, self.ro_size = ro
        self.ro_end = self.ro_start + self.ro_size

    def words(self, start, end):
        data = array.array("I")
        data.frombytes(bytes(self.nso.image[start:end]))
        return data


def main():
    from astral_chain.tools import arm64_shapes as R

    checks = 0

    def check(name, blob, address, site):
        nonlocal checks
        got = R.build_refs(Fake(blob)).get(address)
        assert got == [site], f"{name}: wanted [{site:#x}], got {got}"
        checks += 1

    def check_named(name, blob, address, sites):
        nonlocal checks
        got = R.build_refs(Fake(blob)).get(address)
        assert got == sites, f"{name}: wanted {sites}, got {got}"
        checks += 1

    # adrp + add: the address of a global or a jump table.
    blob = bytearray(0x6000)
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(1, 0x1000, 0x2040), _add(1, 1, 0x040))
    check("adrp+add", blob, 0x2040, 0x1000)

    # adrp + ldr s0 (float load) through x8: the destination register differs
    # from the address register, and the reference must still be found.
    blob = bytearray(0x6000)
    ldr_s = 0xBD400000 | ((0x10 // 4) << 10) | (8 << 5) | 0
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(8, 0x1000, 0x5000), ldr_s)
    check("adrp x8 + ldr s0", blob, 0x5010, 0x1000)

    # adrp + add with only the page: base of a table.
    blob = bytearray(0x6000)
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(1, 0x1000, 0x5000), _add(1, 1, 0x000))
    check("adrp+add page", blob, 0x5000, 0x1000)

    # adrp + ldr: the first field of a struct, 64- and 32-bit scale.
    blob = bytearray(0x6000)
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(2, 0x1000, 0x5000), _ldr64(2, 2, 0x50))
    check("adrp+ldr x64", blob, 0x5050, 0x1000)
    blob = bytearray(0x6000)
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(2, 0x1000, 0x5000), _ldr32(2, 2, 0x14))
    check("adrp+ldr w32", blob, 0x5014, 0x1000)

    # A page below the code: the immediate is negative.
    blob = bytearray(0x6000)
    blob[0x3000:0x3008] = struct.pack("<II", _adrp(3, 0x3000, 0x1000), _add(3, 3, 0x100))
    check("backwards page", blob, 0x1100, 0x3000)

    # adrp + ldr sN, literal pool: the float-constant reference.
    blob = bytearray(0x6000)
    blob[0x1000:0x1004] = struct.pack("<I", 0x1C000000 | (((0x1010 - 0x1000) // 4) << 5) | 1)
    check("ldr s literal", blob, 0x1010, 0x1000)

    # A clobbered address register is not a reference.
    blob = bytearray(0x6000)
    blob[0x1000:0x1008] = struct.pack("<II", _adrp(4, 0x1000, 0x2040), _add(4, 9, 0x000))
    assert 0x2040 not in R.build_refs(Fake(blob)), "clobbered adrp counted"
    checks += 1

    # An integer screen bound arrives as MOVZ, not as a float pool entry.
    blob = bytearray(0x6000)
    blob[0x1000:0x1004] = struct.pack("<I", 0xD2800000 | (1280 << 5) | 5)
    sites = R.int_constant_sites(Fake(blob), (1280, 720))
    assert sites[1280] == [0x1000], sites
    assert sites[720] == [], sites
    checks += 1

    # MOVZ with a shift is a different number and must not match.
    blob = bytearray(0x6000)
    blob[0x1000:0x1004] = struct.pack("<I", 0xD2800000 | (1 << 21) | (1280 << 5) | 5)
    assert R.int_constant_sites(Fake(blob), (1280,))[1280] == []
    checks += 1

    # Sign extension at both widths, both signs.
    assert R.sx(0x7FF, 12) == 0x7FF
    assert R.sx(0x800, 12) == -0x800
    assert R.sx(0xFFF, 12) == -1
    assert R.sx(0x3FFFF, 19) == 0x3FFFF
    assert R.sx(0x40000, 19) == -0x40000
    checks += 1

    # Field offsets survive add/sub of a small constant.
    assert R.ldr_scale(0xF9400000) == 8 and R.ldr_scale(0xB9400000) == 4
    checks += 1

    print(f"re_cursor: {checks} checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
