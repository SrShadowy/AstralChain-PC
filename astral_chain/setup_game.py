"""One-time setup: build the native Windows game from your own Astral Chain dump.

  python -m astral_chain.setup_game [--base FILE] [--update FILE] [--keys prod.keys]

Stages (each is skipped when its result already exists, so a failed run can be fixed and repeated):

  1. tools    download Zig, glslang and NSTool at pinned versions (SHA-256 checked) into tools/
  2. base     extract the base game's Program and Control NCAs (XCI or NSP) with your keys
  3. update   extract the v1.0.1 update's ExeFS, and its RomFS merged over the base
  4. romfs    pack the merged RomFS into game/romfs.bin, then delete the extracted copies
  5. build    translate the game code to C and compile build/game/round-00/trace.exe
  6. shaders  extract the game's shaders and compile the known ones into build/game/cache (also at each launch)

Nothing here leaves your PC. prod.keys is read in place by NSTool and never copied. NSPs carry their own tickets;
the title keys in them are written to a temporary title.keys under game/work for NSTool and deleted after use. Files picked through the dialogs
are remembered in userdata/setup.json (paths only) for the next run.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import threading
import time
import urllib.request
import zipfile

PROJECT = Path(__file__).resolve().parent.parent
if str(PROJECT) not in sys.path:
    sys.path.insert(0, str(PROJECT))
TOOLS = PROJECT/'tools'
GAME = PROJECT/'game'
WORK = GAME/'work'
BUILD = PROJECT/'build'
USERDATA = PROJECT/'userdata'
SETTINGS = USERDATA/'setup.json'

DOWNLOADS = {
    'zig': ('https://ziglang.org/download/0.17.0/zig-x86_64-linux-0.17.0.tar.xz',
            '1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026', 'zig-x86_64-linux-0.17.0/'),
    'glslang': ('https://github.com/KhronosGroup/glslang/releases/download/16.6.0/glslang-16.6.0-linux-x86_64-release.zip',
                'a3fc4f083b1793eb53e55fa3577ac9649ffbe0340715e30d116290fb5382393f', ''),
    'nstool': ('https://github.com/jakcron/nstool/releases/download/v1.9.2/nstool-v1.9.2-ubuntu_x86_64.zip',
               '2ab1d484d79199d1bb428cb920a49cece0ae4d7129c80120eaf48b7361785351', ''),
}
ZIG = TOOLS/'zig'/'zig'
NSTOOL = TOOLS/'nstool'/'nstool'
GLSLANG = TOOLS/'glslang'/'bin'/'glslang'
ROMFS_TOOL = TOOLS/'romfs-build'

LASRECOMP = PROJECT/'third_party'/'LASRecomp'
PATCHES = (PROJECT/'third_party'/'AstralChain-Dynamic-FPS'/'1.0.1.pchtxt',
           PROJECT/'astral_chain'/'patches'/'cutscene-catchup-1.0.1.pchtxt')
MAIN_SHA_101 = 'dc47cae835866379c597e1c507bf52ee0f865ec4412930dfdf1bbc20b6bb9c3b'
PROGRAM_ID = '01007300020fa000'
FREE_SPACE_GB = 35  # extracted RomFS (~10 GB) + its image (~10 GB) + NCAs + build, before the cleanup

UPDATE_EXEFS = GAME/'update-exefs'
CONTROL = GAME/'control.nacp'
ROMFS_IMAGE = GAME/'romfs.bin'
GAME_BUILD = BUILD/'game'
FINGERPRINT = GAME_BUILD/'build-fingerprint.txt'
GUEST_FINGERPRINT = GAME_BUILD/'guest-fingerprint.txt'


class SetupError(Exception):
    pass


def step(text):
    print(f'\n== {text}', flush=True)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


# ---------------------------------------------------------------------------------------------------- 1. tools
def fetch_tools():
    for name, (url, digest, prefix) in DOWNLOADS.items():
        target = TOOLS/name
        if target.is_dir():
            continue
        step(f'Downloading {name}')
        TOOLS.mkdir(exist_ok=True)
        archive_ext = '.tar.xz' if url.endswith('.tar.xz') else '.zip'
        archive = TOOLS/f'{name}{archive_ext}'
        with urllib.request.urlopen(url, timeout=120) as response, open(archive, 'wb') as out:
            shutil.copyfileobj(response, out)
        if sha256(archive) != digest:
            archive.unlink()
            raise SetupError(f'{name}: the download does not match its pinned SHA-256 ({url})')
        partial = TOOLS/f'{name}.partial'
        shutil.rmtree(partial, ignore_errors=True)
        if archive_ext == '.tar.xz':
            with tarfile.open(archive, 'r:xz') as tar:
                for member in tar.getmembers():
                    if not member.name.startswith(prefix) or member.isdir():
                        continue
                    relative = member.name[len(prefix):]
                    destination = (partial/relative).resolve()
                    if partial.resolve() not in destination.parents:
                        raise SetupError(f'{name}: unexpected path in the archive: {member.name}')
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    extracted = tar.extractfile(member)
                    if extracted:
                        with open(destination, 'wb') as out:
                            shutil.copyfileobj(extracted, out)
                        destination.chmod(destination.stat().st_mode | 0o755)
        else:
            with zipfile.ZipFile(archive) as z:
                for member in z.infolist():
                    if not member.filename.startswith(prefix) or member.is_dir():
                        continue
                    relative = member.filename[len(prefix):]
                    destination = (partial/relative).resolve()
                    if partial.resolve() not in destination.parents:
                        raise SetupError(f'{name}: unexpected path in the archive: {member.filename}')
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    with z.open(member) as source, open(destination, 'wb') as out:
                        shutil.copyfileobj(source, out)
                    destination.chmod(destination.stat().st_mode | 0o755)
        partial.rename(target)
        archive.unlink()
    if not ROMFS_TOOL.is_file():
        step('Building the RomFS packer')
        run([ZIG, 'cc', '-O2', '-target', 'x86_64-linux-gnu', '-I', LASRECOMP/'runtime',
             PROJECT/'astral_chain'/'runtime'/'romfs_tool.c', LASRECOMP/'runtime'/'romfs_build.c', '-o', ROMFS_TOOL])
        ROMFS_TOOL.chmod(ROMFS_TOOL.stat().st_mode | 0o755)


def run(command, silent=False, monitor_fn=None, **kwargs):
    command = [str(c) for c in command]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', **kwargs)
    output = []
    stop_monitor = threading.Event()
    if monitor_fn:
        t = threading.Thread(target=monitor_fn, args=(process, stop_monitor), daemon=True)
        t.start()
    for line in iter(process.stdout.readline, ''):
        output.append(line)
        s = line.strip()
        if s and not silent:
            print(f'  {s}', flush=True)
    if monitor_fn:
        stop_monitor.set()
    process.stdout.close()
    returncode = process.wait()
    if returncode:
        tail = '\n'.join(''.join(output).strip().splitlines()[-15:])
        raise SetupError(f'{Path(command[0]).name} failed ({returncode}):\n{tail}')
    return ''.join(output)


# ---------------------------------------------------------------------------------------------------- inputs
def pick_inputs(args):
    saved = json.loads(SETTINGS.read_text(encoding='utf-8')) if SETTINGS.is_file() else {}
    wanted = [('keys', 'your prod.keys', [('Keys', '*.keys'), ('All files', '*.*')]),
              ('base', 'the Astral Chain base game (XCI or NSP)', [('Switch games', '*.xci *.nsp'), ('All files', '*.*')]),
              ('update', 'the Astral Chain v1.0.1 update (NSP)', [('Switch updates', '*.nsp'), ('All files', '*.*')])]
    values = {}
    for key, label, types in wanted:
        given = getattr(args, key, None) or saved.get(key)
        if given and Path(given).is_file():
            values[key] = Path(given)
            continue
        print(f'Select {label}...', flush=True)
        values[key] = ask_file(label, types)
    if getattr(args, 'install_dir', None):
        values['install_dir'] = Path(args.install_dir)
    elif saved.get('install_dir'):
        values['install_dir'] = Path(saved['install_dir'])
    USERDATA.mkdir(exist_ok=True)
    SETTINGS.write_text(json.dumps({k: str(v) for k, v in values.items() if v}, indent=2) + '\n', encoding='utf-8')
    return values


def ask_file(label, types):
    chosen = input(f'Path to {label}: ').strip().strip('"')
    if not chosen or not Path(chosen).is_file():
        raise SetupError(f'No file selected for {label}.')
    return Path(chosen)


# ---------------------------------------------------------------------------------------------------- 2/3. NCAs
def unpack(container, folder, inputs, step_offset=15, step_span=18):
    """The NCAs (and ticket) of an XCI's secure partition or an NSP, into folder. NCAs already there are reused only
    if they came from this same file (source.txt: its path, size and date), not from a file picked before."""
    stat = Path(container).stat()
    origin = f'{Path(container).resolve()}\n{stat.st_size}\n{stat.st_mtime_ns}\n'
    marker = folder/'source.txt'
    if folder.is_dir() and any(folder.glob('*.nca')) and marker.is_file() \
            and marker.read_text(encoding='utf-8', errors='replace') == origin:
        return
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    with open(container, 'rb') as stream:
        head = stream.read(0x110)
    if head[:4] == b'PFS0':
        source = '/'
    elif head[0x100:0x104] == b'HEAD':
        source = '/secure/'
    else:
        raise SetupError(f'{container.name} is neither an NSP nor an XCI.')

    total_size = stat.st_size
    def monitor(proc, stop_ev):
        while not stop_ev.wait(1.5):
            try:
                curr = sum(f.stat().st_size for f in folder.glob('*.nca'))
                if curr > 0 and total_size > 0:
                    pct = min(0.99, curr / total_size)
                    global_pct = step_offset + int(pct * step_span)
                    curr_gb = curr / (1024**3)
                    tot_gb = total_size / (1024**3)
                    print(f'PROGRESS:{global_pct}:Extracting {folder.name} ({curr_gb:.1f}/{tot_gb:.1f} GB - {int(pct*100)}%)', flush=True)
            except Exception:
                pass

    run([NSTOOL, '-k', inputs['keys'], '-x', source, str(folder) + os.sep, container], monitor_fn=monitor)
    if not any(folder.glob('*.nca')):
        raise SetupError(f'No NCAs found in {container.name}.')
    marker.write_text(origin, encoding='utf-8')


def nca_kind(nca, inputs, tik):
    info = run([NSTOOL, '-k', inputs['keys'], *(['--tik', tik] if tik else []), nca], silent=True)
    kind = program = None
    for line in info.splitlines():
        line = line.strip()
        if line.startswith('Content Type:'):
            kind = line.split(':', 1)[1].strip()
        elif line.startswith('ProgID:'):
            program = line.split(':', 1)[1].strip().lower().removeprefix('0x')
    return kind, program


def find_ncas(folder, inputs):
    tik = next(iter(sorted(folder.glob('*.tik'))), None)
    found = {}
    for nca in sorted(folder.glob('*.nca')):
        if nca.name.endswith('.cnmt.nca'):
            continue
        kind, program = nca_kind(nca, inputs, tik)
        if program and program != PROGRAM_ID:
            shutil.rmtree(folder, ignore_errors=True)  # (a retry with another file starts afresh)
            if SETTINGS.is_file():  # (and setup.cmd asks for the file again instead of reusing the saved path)
                saved = json.loads(SETTINGS.read_text(encoding='utf-8'))
                saved.pop(folder.name, None)
                SETTINGS.write_text(json.dumps(saved, indent=2) + '\n', encoding='utf-8')
            raise SetupError(f'The {folder.name} file you picked is for another game: its {nca.name} belongs to program '
                             f'{program}, not Astral Chain ({PROGRAM_ID}). Pick the Astral Chain '
                             f'{"base game" if folder.name == "base" else "v1.0.1 update"} file.')
        if kind in ('Program', 'Control'):
            found[kind] = nca
    return found, tik


def extract_base(inputs):
    folder = WORK/'base'
    unpack(inputs['base'], folder, inputs)
    ncas, tik = find_ncas(folder, inputs)
    if 'Program' not in ncas or 'Control' not in ncas:
        raise SetupError('The base game file has no Astral Chain Program/Control NCA (is it the update instead?).')
    if not CONTROL.is_file():
        step('Extracting the game metadata')
        control = WORK/'control'
        shutil.rmtree(control, ignore_errors=True)
        run([NSTOOL, '-k', inputs['keys'], *(['--tik', tik] if tik else []), '--part0', str(control), ncas['Control']])
        if not (control/'control.nacp').is_file():
            raise SetupError('control.nacp was not found in the Control NCA.')
        shutil.copyfile(control/'control.nacp', CONTROL)
    return ncas['Program'], tik


def extract_update(inputs, base_program, base_tik):
    folder = WORK/'update'
    unpack(inputs['update'], folder, inputs, step_offset=38, step_span=10)
    ncas, tik = find_ncas(folder, inputs)
    if 'Program' not in ncas:
        raise SetupError('The update file has no Astral Chain Program NCA.')
    if not (UPDATE_EXEFS/'main').is_file():
        step('Extracting the update code (ExeFS)')
        shutil.rmtree(UPDATE_EXEFS, ignore_errors=True)
        run([NSTOOL, '-k', inputs['keys'], *(['--tik', tik] if tik else []), '--part0', str(UPDATE_EXEFS), ncas['Program']])
    if sha256(UPDATE_EXEFS/'main') != MAIN_SHA_101:
        shutil.rmtree(UPDATE_EXEFS, ignore_errors=True)
        raise SetupError('The update is not Astral Chain v1.0.1 (its main module does not match). '
                         'This port needs exactly v1.0.1.')
    return ncas['Program'], tik


def title_keys_env(*tiks):
    """NSTool takes one --tik, but an NSP base and the update each need their ticket's title key. NSTool also reads
    .switch/title.keys in the user's home folder, so both keys go into a temporary one (deleted after use)."""
    home = WORK/'nstool-home'
    lines = []
    for tik in filter(None, tiks):
        data = tik.read_bytes()
        lines.append(f'{data[0x2A0:0x2B0].hex()} = {data[0x180:0x190].hex()}')  # rights ID = encrypted title key
    (home/'.switch').mkdir(parents=True, exist_ok=True)
    (home/'.switch'/'title.keys').write_text(''.join(line + '\n' for line in lines), encoding='ascii')
    return {**os.environ, 'USERPROFILE': str(home), 'HOME': str(home)}, home


def build_romfs(inputs, base_program, base_tik, update_program, update_tik):
    if ROMFS_IMAGE.is_file():
        return
    tree = WORK/'romfs'
    step('Extracting the game data (RomFS, about 10 GB; this takes a while)')
    shutil.rmtree(tree, ignore_errors=True)
    env, home = title_keys_env(base_tik, update_tik)
    try:
        def monitor_romfs(proc, stop_ev):
            expected = 10 * (1024**3)
            while not stop_ev.wait(2.0):
                try:
                    curr = sum(f.stat().st_size for f in tree.rglob('*') if f.is_file())
                    if curr > 0:
                        pct = min(0.99, curr / expected)
                        global_pct = 50 + int(pct * 24)
                        curr_gb = curr / (1024**3)
                        print(f'PROGRESS:{global_pct}:Extracting RomFS ({curr_gb:.1f}/10.0 GB - {int(pct*100)}%)', flush=True)
                except Exception:
                    pass

        run([NSTOOL, '-k', inputs['keys'], *(['--tik', update_tik] if update_tik else []), '--basenca', base_program,
             '--part1', str(tree), update_program], env=env, monitor_fn=monitor_romfs)
    finally:
        shutil.rmtree(home, ignore_errors=True)
    if not tree.is_dir() or not any(tree.iterdir()):
        raise SetupError('NSTool could not read the game data (it extracted nothing). Check that the base game and '
                         'update are complete dumps and that prod.keys comes from the same or a newer firmware.')
    step('Packing the game data into game/romfs.bin')
    print('PROGRESS:75:Packing game data into game/romfs.bin...', flush=True)
    partial = GAME/'romfs.bin.partial'
    run([ROMFS_TOOL, tree, partial])
    partial.rename(ROMFS_IMAGE)


# ---------------------------------------------------------------------------------------------------- 5. build
def fingerprint():
    """Changes to the sources, vendored code or patches make the next launch rebuild."""
    digest = hashlib.sha256()
    for root in (PROJECT/'astral_chain', PROJECT/'third_party', PROJECT/'extra'):
        for path in sorted(root.rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts and 'data' not in path.relative_to(root).parts[:1]:
                digest.update(str(path.relative_to(PROJECT)).encode())
                digest.update(path.read_bytes())
    return digest.hexdigest()


def guest_fingerprint():
    """Hashes guest executable, patches, lifter and translator."""
    digest = hashlib.sha256()
    for root in (UPDATE_EXEFS, PROJECT/'astral_chain/patches'):
        if root.exists():
            for path in sorted(root.rglob('*')):
                if path.is_file():
                    digest.update(str(path.relative_to(PROJECT)).encode())
                    digest.update(path.read_bytes())
    for f in (PROJECT/'astral_chain/lifter.py', PROJECT/'astral_chain/native_trace.py'):
        if f.is_file():
            digest.update(f.read_bytes())
    for patch in PATCHES:
        if Path(patch).is_file():
            digest.update(Path(patch).read_bytes())
    return digest.hexdigest()


def try_fast_relink():
    """Fast incremental relink of the native binary when guest code hasn't changed."""
    round_dir = GAME_BUILD/'round-00'
    trace_c = round_dir/'trace.c'
    unit_json = round_dir/'unit_objects.json'
    if not trace_c.is_file() or not unit_json.is_file():
        return False
    try:
        unit_objs = json.loads(unit_json.read_text(encoding='utf-8'))
        if not unit_objs or any(not Path(p).is_file() for p in unit_objs):
            return False
    except Exception:
        return False

    try:
        from .nvn_build import build_nvn_objects
        from .native_build import FLAGS, run_compiler
    except (ImportError, ValueError):
        from nvn_build import build_nvn_objects
        from native_build import FLAGS, run_compiler
    nvn_objects, nvn_link = build_nvn_objects(ZIG, LASRECOMP, BUILD/'native-objects', 'vulkan')
    root = PROJECT
    vk_headers = root/'third_party/Vulkan-Headers/include'
    command = [str(ZIG.resolve()), 'cc', *FLAGS,
               '-I', str(root), '-I', str(vk_headers),
               '-I', str(PROJECT/'astral_chain/runtime'),
               '-I', str(LASRECOMP/'runtime'),
               str(trace_c),
               *(str(obj) for obj in unit_objs),
               *(str(obj) for obj in nvn_objects),
               *nvn_link,
               '-lvulkan', '-lSDL3', '-lpthread', '-lm', '-ldl',
               '-Wl,-z,stack-size=67108864',
               '-o', str(round_dir/'trace')]
    res = run_compiler(command)
    (round_dir/'compiler.log').write_text(res.stdout + res.stderr, encoding='utf-8')
    if res.returncode == 0:
        FINGERPRINT.write_text(fingerprint() + '\n', encoding='ascii')
        GUEST_FINGERPRINT.write_text(guest_fingerprint() + '\n', encoding='ascii')
        print(f'Fast relink succeeded: {round_dir / "trace"} updated.', flush=True)
        return True
    return False


def build_is_current():
    return (GAME_BUILD/'round-00'/'trace').is_file() and FINGERPRINT.is_file() and \
        FINGERPRINT.read_text(encoding='ascii').strip() == fingerprint()


def build_game():
    """native_trace wants a new folder: an older build moves to build/game-old, its shader cache is carried over."""
    if build_is_current():
        return
    if GUEST_FINGERPRINT.is_file() and GUEST_FINGERPRINT.read_text(encoding='ascii').strip() == guest_fingerprint():
        if try_fast_relink():
            return
    step('Building the game (translating and compiling; the first build takes several minutes)')
    print('PROGRESS:85:Translating ARM64 code and compiling native binary...', flush=True)
    old = BUILD/'game-old'
    if GAME_BUILD.exists():
        if (old/'cache').is_dir() and not (GAME_BUILD/'cache').exists():
            (old/'cache').rename(GAME_BUILD/'cache')  # (a failed build left the cache in game-old: kept, not deleted)
        shutil.rmtree(old, ignore_errors=True)
        GAME_BUILD.rename(old)
    command = [sys.executable, '-m', 'astral_chain.native_trace', UPDATE_EXEFS,
               '--reference', LASRECOMP, '--compiler', ZIG, '--output', GAME_BUILD,
               '--instructions', '4000000', '--sample-steps', '0', '--run-timeout', '86400', '--unbounded', '--chain',
               '--follow', '0', '--entry', 'rtld:start', '--seed-all-text', '--build', 'cached',
               '--control', CONTROL, '--romfs', ROMFS_IMAGE, '--nvn-hle', '--nvn-backend', 'vulkan',
               '--optimize', '--lean', '--direct-calls', '--jumps']
    for patch in PATCHES:
        command += ['--patch', patch]
    command.append('--build-only')
    result = subprocess.run([str(c) for c in command], cwd=PROJECT)
    if result.returncode or not (GAME_BUILD/'round-00'/'trace').is_file():
        raise SetupError('The game build failed (see the messages above).')
    FINGERPRINT.write_text(fingerprint() + '\n', encoding='ascii')
    GUEST_FINGERPRINT.write_text(guest_fingerprint() + '\n', encoding='ascii')
    if (old/'cache').is_dir() and not (GAME_BUILD/'cache').exists():
        (old/'cache').rename(GAME_BUILD/'cache')


def prepare_shaders():
    """Shaders compiled before the game starts (shader_cache.py); quick when nothing is new."""
    from . import shader_cache
    step('Preparing shaders')
    print('PROGRESS:95:Preparing Vulkan shaders...', flush=True)
    shader_cache.prepare(ROMFS_IMAGE, GAME, GAME_BUILD, ZIG, GLSLANG, BUILD)


def prepare_bc7():
    """Optional (AC_BC7_PRECOMPUTE=1): BC7 versions of the game's ASTC textures, encoded on the CPU (bc7_cache.py).
    Used by the game with LARECOMP_BC7=1; textures not found there are converted on the GPU."""
    if os.environ.get('AC_BC7_PRECOMPUTE') != '1':
        return
    from . import bc7_cache
    out = GAME_BUILD/'cache'/'bc7'
    stamp = out/'complete.txt'
    want = f'{bc7_cache.tool_digest()} q{bc7_cache.DEFAULT_QUALITY} {ROMFS_IMAGE.stat().st_size}'
    if stamp.is_file() and stamp.read_text(encoding='ascii').strip() == want:
        return
    step('Preparing BC7 textures (about 12 minutes and 16 GB of disk space, once)')
    free = shutil.disk_usage(PROJECT).free / 1e9
    if free < 20:
        print(f'  Skipped: about 20 GB free disk space needed ({free:.0f} GB free). The game converts its textures on '
              'the GPU instead.')
        return
    stamp.unlink(missing_ok=True)
    dll = bc7_cache.build_tool(ZIG, BUILD)
    count, size, seconds = bc7_cache.build(ROMFS_IMAGE, out, dll, progress=lambda line: print(line, flush=True))
    stamp.write_text(want + '\n', encoding='ascii')
    print(f'  {count} textures, {size / 2**30:.1f} GB in {seconds:.0f} s')


def game_data_ready():
    return (UPDATE_EXEFS/'main').is_file() and CONTROL.is_file() and ROMFS_IMAGE.is_file()


def print_status():
    """For the launcher: installed (the game data is extracted), build (current, stale or missing) and shaders
    (compiled, needed or never: shader_cache.status)."""
    from . import shader_cache
    built = (GAME_BUILD/'round-00'/'trace').is_file()
    current = built and build_is_current()
    shaders = shader_cache.status(GAME_BUILD) if built else 'never'
    if shaders == 'compiled' and not current:
        shaders = 'needed'
    print(json.dumps({'installed': game_data_ready(), 'build': 'current' if current else 'stale' if built else 'missing',
                      'shaders': shaders}))


def setup_install_dir(target_dir):
    if not target_dir:
        return
    target = Path(target_dir).resolve()
    target.mkdir(parents=True, exist_ok=True)
    game_link = PROJECT / 'game'
    if game_link.resolve() != target:
        if game_link.is_symlink():
            game_link.unlink()
        elif game_link.is_dir() and not any(game_link.iterdir()):
            game_link.rmdir()
        if not game_link.exists():
            try:
                game_link.symlink_to(target)
            except OSError:
                pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--base', type=Path, help='base game XCI or NSP')
    parser.add_argument('--update', type=Path, help='v1.0.1 update NSP')
    parser.add_argument('--keys', type=Path, help='prod.keys')
    parser.add_argument('--install-dir', type=Path, help='custom installation directory for game data and romfs')
    parser.add_argument('--keep-temp', action='store_true', help='keep the extracted NCAs and RomFS folder')
    parser.add_argument('--rebuild-if-stale', action='store_true', help='(launcher) rebuild only if sources changed')
    parser.add_argument('--force-shaders', action='store_true',
                        help='(with --rebuild-if-stale) check every shader and compile for the GPU again')
    parser.add_argument('--status', action='store_true', help='(launcher) print the setup and shader state as JSON')
    args = parser.parse_args()
    if args.status:
        print_status()
        return
    try:
        target_dir = args.install_dir
        if not target_dir and SETTINGS.is_file():
            try:
                target_dir = json.loads(SETTINGS.read_text(encoding='utf-8')).get('install_dir')
            except Exception:
                pass
        if target_dir:
            setup_install_dir(target_dir)

        if args.rebuild_if_stale:
            if not game_data_ready():
                raise SetupError('The game is not set up yet: install it first.')
            if not build_is_current():
                fetch_tools()
                build_game()
            if args.force_shaders:
                from . import shader_cache
                shader_cache.forget(GAME_BUILD)
            prepare_shaders()
            prepare_bc7()
            return
        GAME.mkdir(exist_ok=True)
        if not game_data_ready():
            free_target = GAME.resolve() if GAME.exists() else PROJECT
            free = shutil.disk_usage(free_target).free / 1e9
            if free < FREE_SPACE_GB:
                raise SetupError(f'Setup needs about {FREE_SPACE_GB} GB free on this drive ({free:.0f} GB free).')
        fetch_tools()
        if not game_data_ready():
            inputs = pick_inputs(args)
            step('Reading the base game')
            base_program, base_tik = extract_base(inputs)
            step('Reading the update')
            update_program, update_tik = extract_update(inputs, base_program, base_tik)
            build_romfs(inputs, base_program, base_tik, update_program, update_tik)
        if not args.keep_temp and WORK.exists():
            step('Removing temporary files')
            shutil.rmtree(WORK, ignore_errors=True)
        build_game()
        prepare_shaders()
        prepare_bc7()
        print('\nSetup complete. Start the game with the launcher (or "./play.sh").')
    except SetupError as error:
        print(f'\nSetup stopped: {error}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
