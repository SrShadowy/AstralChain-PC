"""Checks the Dynamic-Unlocked FPS patch against the vendored base patch.

The unlocked variant is not a separate mod: upstream (yogesh1239/AstralChain-
Dynamic-FPS) publishes it as the same v1.1.1 with the rate cap removed, and its
full pchtxt differs from the vendored Dynamic-60fps file by two instructions.
Keeping only that delta here means a refresh of the vendored patch is inherited
instead of forked, so this test pins the delta: the file must touch exactly the
two cap sites, with exactly the unlocked immediate.

  python -m astral_chain.tools.fps_patch_test
"""

import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent.parent
PROJECT = SRC.parent
BASE = PROJECT / "third_party" / "AstralChain-Dynamic-FPS" / "1.0.1.pchtxt"
UNLOCKED = SRC / "resources" / "patches" / "dynamic-unlocked-1.0.1.pchtxt"
CUTSCENE = SRC / "resources" / "patches" / "cutscene-catchup-1.0.1.pchtxt"

# The two words that hold the cap: `mov w1,#1` in the base, `mov w1,#0` unlocked.
CAP_SITES = {0x009E8D84, 0x009DB6BC}
CAP_ON, CAP_OFF = "21008052", "01008052"


def words(path):
    """{offset: bytes} for the enabled section of a pchtxt, in main's file offsets."""
    found, shift, enabled = {}, 0, False
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("//")[0].strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("@nsobid-"):
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
            found[int(offset, 16) + shift - 0x100] = data.upper()  # (native_trace apply_pchtxt)
    return found


def build_id(path):
    for raw in path.read_text(encoding="utf-8").splitlines():
        if raw.strip().startswith("@nsobid-"):
            return raw.strip()[8:].lower()
    return None


def main():
    base, unlocked = words(BASE), words(UNLOCKED)
    assert base, f"no patch words in {BASE}"

    # The unlocked file is an overlay applied after the base, so it must be exactly
    # the cap delta: any extra site would be a fork of the mod hiding in a small file.
    assert set(unlocked) == CAP_SITES, f"unlocked words outside the cap: {sorted(set(unlocked) - CAP_SITES)}"
    for site in sorted(CAP_SITES):
        assert unlocked[site] == CAP_OFF, f"unlocked {site:#x} is {unlocked[site]}, expected mov w1,#0"
        assert base[site] == CAP_ON, f"base {site:#x} is {base.get(site)}, expected mov w1,#1"

    # Applying base then unlocked must keep every other base word: the overlay may not
    # silently shadow a site the base also patches.
    merged = dict(base) | unlocked
    assert len(merged) == len(base), "unlocked adds a site the base does not patch"
    assert sum(merged[k] != base[k] for k in base) == len(CAP_SITES), "unlocked changes more than the cap"

    # All three are applied in one build (setup_game.py active_patches), and
    # native_trace --patch rejects a build ID that does not match, so they must agree.
    assert build_id(UNLOCKED) == build_id(BASE) == build_id(CUTSCENE), "build IDs differ"

    print(f"fps patch: unlocked is the base plus {len(CAP_SITES)} words (mov w1,#1 -> #0)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
