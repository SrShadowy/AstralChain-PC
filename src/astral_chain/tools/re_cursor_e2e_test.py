"""End-to-end test for astral_chain/tools/re_cursor.py without a game image.

Builds a synthetic `main` with one real hit-test shape (a screen-size float
loaded from rodata, compared, then branched on) and runs the scan and the
report against it. Guards the parts of the RE helper that a decoder unit test
cannot reach: `hit_test_scan` wiring, the MOVZ path and the report writer.

  python -m astral_chain.tools.re_cursor_e2e_test
"""

import array
import struct
import sys
import tempfile
from pathlib import Path

TEXT = 0x1000
TEXT_SIZE = 0x20000
RO = 0x30000
RO_SIZE = 0x1000


def _adrp(rd, pc, page):
    delta = ((page & ~0xFFF) - (pc & ~0xFFF)) >> 12
    imm = delta & 0x1FFFFF
    return 0x90000000 | ((imm & 3) << 29) | (((imm >> 2) & 0x7FFFF) << 5) | rd


def _ldr_s(rt, rn, off):
    return 0xBD400000 | ((off // 4) << 10) | (rn << 5) | rt


FCMP = 0x1E212000
B_GT = 0x54000000 | ((0x10 // 4) << 5) | 0b1100


class Fake:
    text_start, text_size = TEXT, TEXT_SIZE
    text_end = text_start + text_size
    ro_start, ro_size = RO, RO_SIZE
    ro_end = ro_start + ro_size
    base = 0x7000010000

    def __init__(self, blob):
        self.nso = type("N", (), {})()
        self.nso.image = blob
        self.nso.sha256 = "deadbeef"
        self.nso.build_id = "0011"

    def words(self, start, end):
        data = array.array("I")
        data.frombytes(bytes(self.nso.image[start:end]))
        return data

    def u32(self, offset):
        from astral_chain.shared.containers import FormatError
        if offset + 4 > len(self.nso.image):
            raise FormatError("outside image")
        return struct.unpack_from("<I", self.nso.image, offset)[0]

    def floats(self, value):
        want = struct.pack("<f", value)
        blob = bytes(self.nso.image)
        found, at = [], blob.find(want, self.ro_start, self.ro_end)
        while at != -1:
            if at % 4 == 0:
                found.append(at)
            at = blob.find(want, at + 1, self.ro_end)
        return found

    def check(self, offset, count=1, region="text"):
        from astral_chain.shared.containers import FormatError
        low, high = ((self.text_start, self.text_end) if region == "text"
                     else (self.ro_start, self.ro_end))
        if not (low <= offset and offset + count <= high):
            raise FormatError(f"{offset:#x} outside {region}")

    def disasm(self, offset, count):
        from astral_chain.tools import arm64_shapes as S
        return S.disasm(self, offset, count)

    def module_offset(self, address):
        from astral_chain.tools import arm64_shapes as S
        return S.module_offset(address, self.base)


def build_image():
    blob = bytearray(RO + RO_SIZE)
    struct.pack_into("<f", blob, RO + 0x10, 1280.0)
    # Two sites that bound the screen at 1280: the loaded register differs from
    # the address register in one of them, which the RE helper must still see.
    for site, address_reg in ((TEXT, 1), (TEXT + 0x1000, 8)):
        blob[site:site + 16] = struct.pack(
            "<IIII",
            _adrp(address_reg, site, RO),
            _ldr_s(0, address_reg, 0x10),
            FCMP,
            B_GT,
        )
    return Fake(blob)


def main():
    from astral_chain.tools import re_cursor as R

    image = build_image()
    report = R.hit_test_scan(image)
    suspects = [entry for entry in report if entry["compares"]]
    sites = {entry["site"] for entry in suspects}
    assert sites == {TEXT, TEXT + 0x1000}, f"hit-test sites: {sorted(sites)}"
    assert all(entry["value"] == 1280.0 for entry in suspects), "wrong bound value"

    # A hit-test site within one instruction of the segment end must report a
    # truncated window, not raise.
    tail = Fake(build_image().nso.image)
    last = Fake.text_end - 4
    tail.nso.image[last:last + 4] = struct.pack("<I", _ldr_s(0, 31, 0x10))
    assert R._window(tail, 1280.0, RO + 0x10, last)["window"], "tail window empty"

    # Both `main+0x...` offsets and load-base addresses resolve to the same
    # site, so the documented addresses can be pasted in directly.
    assert image.module_offset(TEXT) == TEXT
    assert image.module_offset(image.base + TEXT) == TEXT

    out = Path(tempfile.gettempdir()) / "re_cursor_e2e_report.md"
    args = type("Args", (), {})()
    args.out, args.count, args.cases = out, 24, 8
    args.cursor_handler, args.cursor_table = TEXT, RO + 0x10
    R.cmd_report(image, args)
    text = out.read_text(encoding="utf-8")
    out.unlink()
    for needed in ("## 1. Cursor handler layout", "## 2. Dispatch table",
                   "## 3. Screen-size constants", "deadbeef", "value 1280"):
        assert needed in text, f"report is missing {needed!r}"

    print("re_cursor e2e: hit-test scan and report pass")
    return 0

if __name__ == "__main__":
    sys.exit(main())
