"""Reverse-engineering helper for the menu cursor and screen-space hit-testing.

The repository has no `main`: it comes from the player's own dump and is
extracted by astral_chain/build/setup_game.py. This tool reads it through
astral_chain.shared.nso and answers the two questions that decide whether
absolute (pointer-position) menu aiming is possible at all:

  1. Where does the menu cursor object live, and which offsets are its
     position / kind / index fields?
  2. Does any code compare a screen-space coordinate against a range? That
     comparison is a hit-test. docs/plan-mouse-menu.md 4.4 measured that no
     menu reacts to a touch contact; this scan says whether that is because
     no hit-test exists in the image, or because the one that exists is not
     reached from the touch path.

Everything here is read-only: only the requested report is written.

Usage (from the project root, with the dump already extracted):

  python -m astral_chain.tools.re_cursor disasm 0x625e60 60
  python -m astral_chain.tools.re_cursor fields 0x625e60 400
  python -m astral_chain.tools.re_cursor table 0xbe9190 8
  python -m astral_chain.tools.re_cursor xrefs 0xbe9190
  python -m astral_chain.tools.re_cursor hit-test
  python -m astral_chain.tools.re_cursor report --out docs/re-cursor-report.md

`--exefs` defaults to game/update-exefs; `--base` to the load base that the
documents use for `main+0x...` addresses (0x7000010000).
"""

from pathlib import Path
import argparse
import json
import numbers
import struct
import sys

from astral_chain.shared.containers import FormatError
from astral_chain.shared.nso import Nso
from astral_chain.tools import arm64_shapes as S

# Screen sizes the UI could use for hit-testing: the panel (handheld), the
# docked render size and the port's own 1600x900 client space.
SCREEN_FLOATS = (1280.0, 720.0, 1600.0, 900.0, 1920.0, 1080.0, 2560.0, 1440.0)
# The same bounds as touch-panel integers (a contact is in panel pixels).
SCREEN_INTS = (1280, 720, 1600, 900, 1920, 1080)
# The touched menu-cursor handler and its dispatch table
# (docs/reverse-engineering-ui-perf.md 2.2).
CURSOR_HANDLER = 0x625E60
CURSOR_TABLE = 0xBE9190
CURSOR_CASES = 8


class Image:
    """The `main` module, viewed as a flat image with parsed segments."""

    def __init__(self, exefs, base):
        self.nso = Nso(Path(exefs) / "main")
        self.base = base
        self.text_start, self.text_size = self.nso.text
        self.text_end = self.text_start + self.text_size
        self.ro_start, self.ro_size = self.nso.rodata
        self.ro_end = self.ro_start + self.ro_size

    def u32(self, offset):
        return self.nso.u32(offset)

    def words(self, start, end):
        return S.words_of(self.nso.image, start, min(end, self.text_end))

    def floats(self, value):
        """Module offsets of `value` as a float32 in rodata/data (4-byte aligned)."""
        want = struct.pack("<f", value)
        hits = []
        for start, size in ((self.ro_start, self.ro_size),
                            (self.nso.data[0], self.nso.data[1])):
            blob = bytes(self.nso.image[start:start + size])
            at = blob.find(want)
            while at != -1:
                if at % 4 == 0:
                    hits.append(start + at)
                at = blob.find(want, at + 1)
        return hits

    def module_offset(self, address):
        """Accept both `main+0x...` module offsets and load-base addresses."""
        return S.module_offset(address, self.base)

    def check(self, offset, count=1, region="text"):
        low, high = ((self.text_start, self.text_end) if region == "text"
                     else (self.ro_start, self.ro_end))
        if offset < low or offset + count > high:
            raise FormatError(f"{offset:#x} is outside {region}")


# ---- hit-test scan --------------------------------------------------------

def _window(image, value, constant, site):
    window = S.disasm(image, max(image.text_start, site - 4), 32)
    compares = [i for i in window
                if i.mnemonic in ("fcmp", "fcmpe", "cmp", "cmn")
                or i.mnemonic.startswith("b.") or i.mnemonic in ("cbz", "cbnz", "tbz", "tbnz")]
    return {"value": value, "constant": constant, "site": site,
            "compares": [f"{i.address:#x} {i.mnemonic} {i.op_str}" for i in compares],
            "window": [f"{i.address:#x} {i.mnemonic} {i.op_str}" for i in window]}


def hit_test_scan(image):
    """Report code that materialises a screen size, and whether it compares.

    A hit-test needs both halves: the constant that bounds the screen, and a
    compare plus a conditional branch that decides inside/outside. A site with
    the constant but no compare is layout or scaling, not hit-testing.
    """
    refs = S.build_refs(image)
    report = []
    for value in SCREEN_FLOATS:
        for addr in image.floats(value):
            for site in refs.get(addr, []):
                report.append(_window(image, value, addr, site))
    for value, sites in S.int_constant_sites(image, SCREEN_INTS).items():
        for site in sites:
            report.append(_window(image, value, site, site))
    return report


# ---- commands -------------------------------------------------------------

def cmd_disasm(image, args):
    for insn in S.disasm(image, args.offset, args.count):
        print(f"{insn.address:#08x}  {insn.mnemonic:<8} {insn.op_str}")
    return 0


def cmd_fields(image, args):
    hits = S.field_map(image, args.offset, args.count)
    summary = {}
    for hit in hits:
        summary.setdefault(hit["field"], []).append(hit)
    print(f"{args.offset:#x}: {len(hits)} field accesses, {len(summary)} distinct offsets")
    for field in sorted(summary):
        accesses = summary[field]
        print(f"  this+{field:#05x}  x{len(accesses):<3} {accesses[0]['text']}")
    return 0


def cmd_table(image, args):
    """Entries are absolute text addresses, or offsets from the table base."""
    raw = [image.u32(args.offset + i * 4) for i in range(args.count)]
    for i, value in enumerate(raw):
        target = value if image.text_start <= value < image.text_end else args.offset + value
        tag = "code" if image.text_start <= target < image.text_end else "not code"
        print(f"  [{i}] raw={value:#010x} -> {target:#x} ({tag})")
    return 0


def cmd_xrefs(image, args):
    refs = S.build_refs(image)
    sites = refs.get(args.offset, [])
    print(f"{len(sites)} reference(s) to {args.offset:#x}")
    for site in sites:
        for insn in S.disasm(image, max(image.text_start, site - 8), 6):
            print(f"  {insn.address:#08x}  {insn.mnemonic:<8} {insn.op_str}")
    return 0


def cmd_hit_test(image, args):
    report = hit_test_scan(image)
    suspects = [r for r in report if r["compares"]]
    print(f"{len(report)} site(s) materialise a screen-size bound; {len(suspects)} also compare")
    for entry in suspects:
        print(f"\nsite {entry['site']:#x}  value={entry['value']:g}  constant={entry['constant']:#x}")
        for line in entry["window"]:
            print(f"    {line}")
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


def cmd_report(image, args):
    lines = ["# Menu cursor and hit-testing: measured report", "",
             f"main sha256 `{image.nso.sha256}`, build `{image.nso.build_id}`", "",
             "Generated by `python -m astral_chain.tools.re_cursor report`. Read-only scan.", ""]

    lines += ["## 1. Cursor handler layout", "",
              f"`{args.cursor_handler:#x}`, first {args.count} instructions, "
              "fields relative to `this` (x0).", "",
              "| field | accesses | first |", "|---|---|---|"]
    try:
        hits = S.field_map(image, args.cursor_handler, args.count)
    except FormatError as exc:
        hits = []
        lines.append(f"| (unreadable: {exc}) | | |")
    summary = {}
    for hit in hits:
        summary.setdefault(hit["field"], []).append(hit)
    for field in sorted(summary):
        accesses = summary[field]
        lines.append(f"| `+{field:#x}` | {len(accesses)} | `{accesses[0]['text']}` |")

    lines += ["", "## 2. Dispatch table", "", f"`{args.cursor_table:#x}`:", ""]
    try:
        raw = [image.u32(args.cursor_table + i * 4) for i in range(args.cases)]
    except FormatError as exc:
        raw = []
        lines.append(f"(unreadable: {exc})")
    for i, value in enumerate(raw):
        target = value if image.text_start <= value < image.text_end else args.cursor_table + value
        lines.append(f"- case {i}: `{target:#x}`")

    lines += ["", "## 3. Screen-size constants and hit-test candidates", ""]
    report = hit_test_scan(image)
    suspects = [r for r in report if r["compares"]]
    lines.append(f"{len(report)} sites materialise a screen-size bound; {len(suspects)} also compare.")
    lines.append("")
    for entry in suspects:
        value = (f"{entry['value']:g}" if isinstance(entry["value"], numbers.Real)
                 else str(entry["value"]))
        lines += [f"### site `{entry['site']:#x}` (value {value})", "", "```"]
        lines += [f"{line}" for line in entry["window"]]
        lines += ["```", ""]

    text = "\n".join(lines) + "\n"
    Path(args.out).write_text(text, encoding="utf-8")
    print(f"wrote {args.out} ({len(lines)} lines)")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exefs", type=Path, default=Path("game/update-exefs"))
    parser.add_argument("--base", type=lambda v: int(v, 0), default=0x7000010000)
    sub = parser.add_subparsers(dest="command", required=True)

    dis = sub.add_parser("disasm", help="disassemble a range")
    dis.add_argument("offset", type=lambda v: int(v, 0))
    dis.add_argument("count", type=int, nargs="?", default=40)
    dis.set_defaults(func=cmd_disasm)

    fields = sub.add_parser("fields", help="map the fields of a function's `this`")
    fields.add_argument("offset", type=lambda v: int(v, 0))
    fields.add_argument("count", type=int, nargs="?", default=400)
    fields.set_defaults(func=cmd_fields)

    table = sub.add_parser("table", help="read a jump table")
    table.add_argument("offset", type=lambda v: int(v, 0))
    table.add_argument("count", type=int, nargs="?", default=CURSOR_CASES)
    table.set_defaults(func=cmd_table)

    xrefs = sub.add_parser("xrefs", help="find the code that references an address")
    xrefs.add_argument("offset", type=lambda v: int(v, 0))
    xrefs.set_defaults(func=cmd_xrefs)

    hit = sub.add_parser("hit-test", help="find code that bounds a screen coordinate")
    hit.add_argument("--json", type=Path)
    hit.set_defaults(func=cmd_hit_test)

    report = sub.add_parser("report", help="write the full markdown report")
    report.add_argument("--out", type=Path, default=Path("docs/re-cursor-report.md"))
    report.add_argument("--count", type=int, default=400)
    report.add_argument("--cursor-handler", type=lambda v: int(v, 0), default=CURSOR_HANDLER)
    report.add_argument("--cursor-table", type=lambda v: int(v, 0), default=CURSOR_TABLE)
    report.add_argument("--cases", type=int, default=CURSOR_CASES)
    report.set_defaults(func=cmd_report)

    args = parser.parse_args(argv)
    try:
        image = Image(args.exefs, args.base)
    except (OSError, FormatError) as exc:
        print(f"cannot read {args.exefs / 'main'}: {exc}", file=sys.stderr)
        print("run astral_chain/build/setup_game.py first (it extracts the ExeFS)", file=sys.stderr)
        return 2
    for name in ("offset", "cursor_handler", "cursor_table"):
        if hasattr(args, name):
            setattr(args, name, image.module_offset(getattr(args, name)))
    try:
        return args.func(image, args)
    except FormatError as exc:
        print(f"{args.command}: {exc}", file=sys.stderr)
        print("an address must be a module offset (main+0x...) or a load-base address",
              file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
