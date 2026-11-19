"""Symbolize AC_LPROF sample reports (AC_LPROF=<start>,<secs>[,<out>]) against ELF symbol tables."""
import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
from pathlib import Path
import subprocess

SLEEPY_FRAMES = {
    "clock_nanosleep", "nanosleep", "__nanosleep",
    "pthread_cond_wait", "pthread_cond_timedwait", "pthread_cond_clockwait",
    "epoll_wait", "ppoll", "__poll", "poll", "select", "pause",
    "parallel_block", "sigtimedwait", "hang_watch",
}


def is_idle_chain(syms: list[str]) -> bool:
    for sym in syms:
        if sym in SLEEPY_FRAMES or "libusb" in sym or "cond_wait" in sym:
            return True
    return False


def load_elf_symbols(path: Path) -> tuple[list[int], list[str]]:
    if not path.is_file():
        return [], []
    for flag in ("-n", "-Dn"):
        res = subprocess.run(
            ["nm", flag, "--defined-only", str(path)],
            capture_output=True, text=True, check=False,
        )
        addrs, names = [], []
        for line in res.stdout.splitlines():
            parts = line.split(None, 2)
            if len(parts) < 3 or parts[1] not in ("t", "T", "W", "w", "i"):
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            name = parts[2].split("@", 1)[0]
            if not addrs or addrs[-1] != addr:
                addrs.append(addr)
                names.append(name)
        if addrs:
            return addrs, names
    return [], []


def parse_report(report_path: Path):
    samples = []
    maps = []
    for line in report_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("S "):
            parts = line.split()
            if len(parts) >= 4:
                tid = parts[1]
                comm = parts[2]
                pcs = [int(x, 16) for x in parts[3:]]
                samples.append((tid, comm, pcs))
        elif line.startswith("M "):
            rest = line[2:].split(None, 5)
            if len(rest) < 6:
                continue
            rng, perms, off_hex, _, _, path = rest
            if "x" not in perms or not path.startswith("/"):
                continue
            lo_s, hi_s = rng.split("-")
            maps.append((int(lo_s, 16), int(hi_s, 16), int(off_hex, 16), path.strip()))
    return samples, maps


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("report", type=Path, help="Path to ac_lprof.txt")
    ap.add_argument("--top", type=int, default=20, help="Top functions per thread")
    args = ap.parse_args()

    samples, maps = parse_report(args.report)
    maps.sort()

    elf_cache: dict[str, tuple[list[int], list[str]]] = {}
    base_for_path: dict[str, int] = {}
    for lo, hi, off, p in maps:
        if off == 0 or p not in base_for_path:
            base_for_path[p] = min(base_for_path.get(p, lo - off), lo - off)

    def resolve(pc: int) -> str:
        for lo, hi, off, p in maps:
            if lo <= pc < hi:
                if p not in elf_cache:
                    elf_cache[p] = load_elf_symbols(Path(p))
                addrs, names = elf_cache[p]
                rel = pc - base_for_path[p]
                if not addrs:
                    # Try exact pc if non-PIE executable mapped at fixed virtual address
                    return f"{Path(p).name}+0x{pc - lo + off:x}"
                idx = bisect_right(addrs, pc) - 1
                if idx >= 0 and pc - addrs[idx] < 0x100000:
                    return names[idx]
                idx = bisect_right(addrs, rel) - 1
                if idx >= 0 and rel - addrs[idx] < 0x100000:
                    return names[idx]
                return f"{Path(p).name}+0x{pc - lo + off:x}"
        return f"0x{pc:x}"

    per_thread_active = defaultdict(int)
    per_thread_total = defaultdict(int)
    per_thread_self: dict[str, Counter] = defaultdict(Counter)
    per_thread_incl: dict[str, Counter] = defaultdict(Counter)

    for tid, comm, pcs in samples:
        key = f"{comm} (tid {tid})"
        per_thread_total[key] += 1
        syms = [resolve(pc) for pc in pcs]
        if syms and is_idle_chain(syms):
            continue
        per_thread_active[key] += 1
        per_thread_self[key][syms[0]] += 1
        for sym in set(syms):
            per_thread_incl[key][sym] += 1

    print(f"Loaded {len(samples)} total samples across {len(per_thread_total)} threads.")
    for key, active in sorted(per_thread_active.items(), key=lambda kv: -kv[1]):
        if active < 15:
            continue
        tot = per_thread_total[key]
        print(f"\n=== {key}: {active}/{tot} active samples ({100.0 * active / max(tot, 1):.1f}% busy) ===")
        print("  Self:")
        for sym, cnt in per_thread_self[key].most_common(args.top):
            print(f"    {100.0 * cnt / active:5.1f}%  {cnt:5d}  {sym}")
        print("  Inclusive:")
        for sym, cnt in per_thread_incl[key].most_common(args.top):
            print(f"    {100.0 * cnt / active:5.1f}%  {cnt:5d}  {sym}")


if __name__ == "__main__":
    main()
