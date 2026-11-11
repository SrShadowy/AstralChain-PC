"""ARM64 shape decoding for the read-only RE helpers.

Small, dependency-light pieces of the decoder that `re_cursor.py` needs to
find references to an address and to read a struct's fields. Kept apart so
each can be tested and reasoned about on its own; nothing here touches a game
image directly, it all works on words.

A real disassembler is only used where a full instruction is needed
(`disasm`, `field_map`), and it is imported lazily so importing this module
does not require capstone.
"""

import array
import struct


# ---- immediates -----------------------------------------------------------

def sx(value, bits):
    """Two's-complement sign extension of `value` in `bits` bits."""
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def adrp_page(addr, word):
    """The 4 KB page an ADRP addresses, from the PC of the ADRP itself."""
    imm = (((word >> 5) & 0x7FFFF) << 2) | ((word >> 29) & 3)
    return (addr & ~0xFFF) + (sx(imm, 21) << 12)


def movz_value(word):
    """(shift, immediate) of a MOVZ, for screening integer constants."""
    return (word >> 21) & 3, (word >> 5) & 0xFFFF


def module_offset(address, base):
    """Resolve an address written as `main+0x...` or as a load-base address.

    The documents use module offsets; a debugger prints load-base addresses.
    Both are accepted so an address can be pasted from either source.
    """
    return address - base if address >= base else address


# ---- instruction class tests ---------------------------------------------

def is_adrp(w):
    return (w & 0x9F000000) == 0x90000000


def is_add_imm(w):
    # ADD (immediate), 64- and 32-bit: bits 31:24 are sf|0|0|10001 and bit 23
    # (sh) is part of the immediate, so it must stay out of the mask.
    return (w & 0xFF000000) in (0x91000000, 0x11000000)


def is_ldr(w):
    return (w & 0xFFC00000) in (0xF9400000, 0xB9400000, 0xB9800000, 0xBD400000, 0xFD400000)


def ldr_scale(w):
    top = w & 0xFFC00000
    return {0xF9400000: 8, 0xB9400000: 4, 0xB9800000: 4,
            0xBD400000: 4, 0xFD400000: 8}[top]


def is_ldr_literal_s(w):
    return (w & 0xFF000000) == 0x1C000000


def is_movz(w):
    return (w & 0x7F800000) in (0x52800000, 0xD2800000)


# ---- reference scanning ---------------------------------------------------

def build_refs(image):
    """Map a module address to the code offsets that materialise it.

    Catches the two shapes that matter: `adrp xN, page` + `add xN, xN, #off`
    (address of a global/table) and `adrp` + `ldr` (first field of a struct).
    Also catches `ldr sN, <literal pool>` for float constants.

    `image` only needs `text_start`, `text_end` and `words(start, end)`.
    """
    refs = {}
    words = image.words(image.text_start, image.text_end)
    total = len(words)
    for i in range(total):
        w = words[i]
        addr = image.text_start + i * 4
        if is_ldr_literal_s(w):
            target = addr + sx((w >> 5) & 0x7FFFF, 19) * 4
            refs.setdefault(target, []).append(addr)
            continue
        if not is_adrp(w):
            continue
        rd = w & 31
        page = adrp_page(addr, w)
        for j in range(1, 4):
            if i + j >= total:
                break
            nxt = words[i + j]
            # Both shapes address through rd: `add xN, xN, #off` and
            # `ldr xN/sN, [xN, #off]`. The loaded register may differ from rd
            # (adrp x8 page; ldr s0, [x8, #off]) so only rn is compared.
            if ((nxt >> 5) & 31) != rd:
                continue
            if is_add_imm(nxt):
                if (nxt & 31) != rd:
                    continue
                refs.setdefault(page + (((nxt >> 10) & 0xFFF) << (12 if (nxt >> 22) & 1 else 0)),
                                []).append(addr)
            elif is_ldr(nxt):
                refs.setdefault(page + ((nxt >> 10) & 0xFFF) * ldr_scale(nxt), []).append(addr)
    return refs


def int_constant_sites(image, values):
    """Code offsets whose MOVZ materialises one of `values`.

    A screen bound can arrive as an integer (a touch coordinate) rather than a
    float, so a float-only scan would miss half the hit-tests.
    """
    wanted = {v: [] for v in values}
    words = image.words(image.text_start, image.text_end)
    for i, w in enumerate(words):
        if not is_movz(w):
            continue
        shift, imm = movz_value(w)
        if shift:
            continue
        if imm in wanted:
            wanted[imm].append(image.text_start + i * 4)
    return wanted


# ---- disassembly-backed helpers ------------------------------------------

def disasm(image, offset, count):
    """Disassemble up to `count` instructions, stopping at the text segment end.

    Callers pass a window around a site they found by scanning, so a site near
    the end of `.text` would otherwise raise instead of reporting a truncated
    window.
    """
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
    count = min(count, (image.text_end - offset) // 4)
    if count <= 0:
        return []
    image.check(offset, count * 4)
    raw = bytes(image.nso.image[offset:offset + count * 4])
    return list(Cs(CS_ARCH_ARM64, CS_MODE_ARM).disasm(raw, offset))


def field_map(image, offset, count):
    """Follow `this` (x0) through register copies and list the field accesses.

    A field access is any ldr/str whose base register still traces back to
    x0, with the accumulated displacement. That is what turns "the cursor
    object is at +0x8C" into a layout.
    """
    import re
    aliases = {0: 0}  # register -> displacement from this
    found = []
    for insn in disasm(image, offset, count):
        ops = insn.op_str
        m = re.search(r"\bx(\d+), x(\d+)(?:, #(0x[0-9a-f]+|\d+))?$", ops)
        if insn.mnemonic in ("mov", "add", "sub") and m:
            dst, src = int(m.group(1)), int(m.group(2))
            if src in aliases:
                delta = int(m.group(3), 0) if m.group(3) else 0
                if insn.mnemonic == "sub":
                    delta = -delta
                aliases[dst] = aliases[src] + delta
                continue
        m = re.search(r"x(\d+), \[x(\d+)(?:, #(0x[0-9a-f]+|\d+))?\]", ops)
        if m and insn.mnemonic.split(".")[0] in ("ldr", "ldrb", "ldrh", "ldrsw", "ldrsb",
                                                 "ldrsh", "str", "strb", "strh"):
            base = int(m.group(2))
            if base in aliases:
                disp = aliases[base] + (int(m.group(3), 0) if m.group(3) else 0)
                found.append({"offset": insn.address, "access": insn.mnemonic,
                              "field": disp, "text": f"{insn.mnemonic} {ops}"})
    return found


def words_of(blob, start, end):
    """A little-endian word array over a slice, for tests and callers."""
    data = array.array("I")
    data.frombytes(bytes(blob[start:end]))
    return data


def float32(value):
    return struct.pack("<f", value)
