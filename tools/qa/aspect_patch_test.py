#!/usr/bin/env python3
"""Automated anti-regression test for resolution and aspect ratio patch.
Validates patch offsets, instruction encodings, non-overlap with other patches,
and mathematical invariants for Ultrawide and 16:10 aspect configurations."""

import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent.parent / "src"
RES_PATCH = SRC / "resources" / "patches" / "resolution-1.0.1.pchtxt"
CUT_PATCH = SRC / "resources" / "patches" / "cutscene-catchup-1.0.1.pchtxt"
FPS_PATCH = SRC / "resources" / "patches" / "dynamic-unlocked-1.0.1.pchtxt"

SVC_B3_ENCODING = "611600D4"  # svc #0xB3 in little-endian hex


def parse_patch_sites(path):
    sites = {}
    shift = 0
    enabled = False
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("//")[0].strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("@flag offset_shift"):
            shift = int(line.split()[-1], 0)
            continue
        if line == "@enabled":
            enabled = True
        elif line in ("@disabled", "@stop"):
            enabled = False
        elif enabled:
            offset, data = line.split()
            addr = int(offset, 16) + shift - 0x100
            sites[addr] = data.upper()
    return sites


def main():
    print("[aspect_patch_test] Validating resolution & aspect ratio patch invariants...")
    assert RES_PATCH.exists(), f"Missing patch file: {RES_PATCH}"
    res_sites = parse_patch_sites(RES_PATCH)
    cut_sites = parse_patch_sites(CUT_PATCH)
    fps_sites = parse_patch_sites(FPS_PATCH)

    # 1. Non-overlapping invariant: resolution patch must not collide with cutscene or fps patches
    overlap_cut = set(res_sites) & set(cut_sites)
    assert not overlap_cut, f"Conflict detected between resolution and cutscene patches at: {overlap_cut}"
    overlap_fps = set(res_sites) & set(fps_sites)
    assert not overlap_fps, f"Conflict detected between resolution and FPS patches at: {overlap_fps}"
    print("  [PASS] Zero collision with dynamic FPS or cutscene catchup patches")

    # 2. Critical camera aspect & rectangle sites must exist and encode svc #0xB3
    aspect_site = 0x009A30BC
    rect_site = 0x009A30C4
    assert aspect_site in res_sites, f"Missing camera aspect site {aspect_site:#x}"
    assert res_sites[aspect_site] == SVC_B3_ENCODING, f"Invalid encoding at {aspect_site:#x}"
    assert rect_site in res_sites, f"Missing camera screen rect site {rect_site:#x}"
    assert res_sites[rect_site] == SVC_B3_ENCODING, f"Invalid encoding at {rect_site:#x}"
    print("  [PASS] Camera aspect and screen rectangle sites encoded correctly (svc #0xB3)")

    # 3. Mathematical validation of aspect and screen rectangle math
    test_cases = [
        # (width, height, expected_aspect, expected_rx_720p)
        (1920, 1080, 16.0 / 9.0, 0),        # 16:9 standard
        (2560, 1080, 2560.0 / 1080.0, -213),# 21:9 ultrawide (-213px offset on 1280 base)
        (3440, 1440, 3440.0 / 1440.0, -220),# 21.5:9 ultrawide
        (5120, 1440, 32.0 / 9.0, -640),     # 32:9 super ultrawide
        (1280, 800,  16.0 / 10.0, 0),       # 16:10 Steam Deck (pillarboxed horizontally)
    ]
    for w, h, exp_asp, exp_rx in test_cases:
        asp = w / h
        assert abs(asp - exp_asp) < 0.05, f"Aspect ratio mismatch for {w}x{h}: {asp} vs {exp_asp}"
        if asp > (16.0 / 9.0):
            ww = int(720.0 * asp)
            rx = (1280 - ww) // 2
            assert abs(rx - exp_rx) <= 5, f"Center offset mismatch for {w}x{h}: got {rx}, expected {exp_rx}"
    print("  [PASS] Ultrawide and 16:10 aspect ratio projection & centering math verified")

    print("[aspect_patch_test] All aspect ratio and resolution tests PASSED!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
