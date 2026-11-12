"""Optional cached object build for generated code pages and host runtime."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess

from astral_chain.shared.containers import FormatError

PORTABLE_ZIG_TARGET = 'x86_64-linux-gnu.2.35'
PORTABLE_CPU_TARGET = 'x86_64'


def is_portable_build():
    return os.environ.get('AC_PORTABLE_BUILD', '').strip().lower() in ('1', 'true', 'on', 'yes')


def data_root(app_root):
    override = os.environ.get('AC_DATA_DIR', '').strip()
    return Path(override).resolve() if override else Path(app_root).resolve()


def target_flags(optimize=False):
    opt = '-O2' if optimize else '-O1'
    if is_portable_build():
        target = os.environ.get('AC_ZIG_TARGET', PORTABLE_ZIG_TARGET)
        cpu = os.environ.get('AC_CPU_TARGET', PORTABLE_CPU_TARGET)
        return ['-target', target, opt, f'-mcpu={cpu}', '-mcx16']
    if optimize:
        return ['-target', 'native', '-O2', '-mcpu=x86_64_v3', '-mcx16']
    return ['-target', 'native', opt, '-mcx16']


FLAGS = target_flags(optimize=False)
SUPPORT='''#include "cpu.h"
extern __thread uint64_t native_executed_steps;  /* per host thread (parallel mode: a shared counter bounced between cores) */
extern const uint64_t native_instruction_limit;
__attribute__((noreturn)) void native_boundary(const char*,Cpu*,uint64_t);
extern unsigned thread_reschedule;
int native_checkpoint(Cpu*,uint64_t);
void native_trace_call(uint64_t,uint64_t);
void native_clear_pair(Cpu*);
void native_load_pair(Cpu*,uint64_t,unsigned,unsigned,unsigned);
uint32_t native_store_pair(Cpu*,uint64_t,unsigned,uint64_t,uint64_t);
#define executed_steps native_executed_steps
#define boundary native_boundary
#define thread_checkpoint native_checkpoint
#define trace_call native_trace_call
#define rt_clear_pair_monitor native_clear_pair
#define rt_load_pair native_load_pair
#define rt_store_pair native_store_pair
'''
BRIDGE='''__thread uint64_t native_executed_steps;
#define executed_steps native_executed_steps
__attribute__((noreturn)) void native_boundary(const char*s,Cpu*c,uint64_t pc) {boundary(s,c,pc);}
int native_checkpoint(Cpu*c,uint64_t step) {return thread_checkpoint(c,step);}
void native_trace_call(uint64_t from,uint64_t to) {trace_call(from,to);}
void native_clear_pair(Cpu*c) {rt_clear_pair_monitor(c);}
void native_load_pair(Cpu*c,uint64_t address,unsigned width,unsigned first,unsigned second) {rt_load_pair(c,address,width,first,second);}
uint32_t native_store_pair(Cpu*c,uint64_t address,unsigned width,uint64_t low,uint64_t high) {return rt_store_pair(c,address,width,low,high);}
'''


def split_dispatch(pages,limit,stop_at=None,chain=False):
    from astral_chain.translator.native_trace import emit_page,emit_execute
    groups={}
    for page,cases in sorted(pages.items()):
        groups.setdefault(page>>5,[]).append(emit_page(page,cases,limit,stop_at,external=True,chain=chain))
    units={group:SUPPORT+'\n'.join(parts) for group,parts in groups.items()}
    dispatch=BRIDGE+f'\nconst uint64_t native_instruction_limit={limit}ull;\n'+'\n'.join(f'void page_{page:x}(Cpu*);' for page in sorted(pages))
    return units,dispatch+'\n'+emit_execute(pages,stop_at)


def run_compiler(command,timeout=300):
    with subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) as process:
        try:
            stdout,stderr=process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill();process.communicate(timeout=15)
            raise
        return subprocess.CompletedProcess(command,process.returncode,stdout,stderr)


SHA256_CHUNK_BYTES = 1 << 20


def sha256_stream(stream):
    if hasattr(hashlib, 'file_digest'):
        return hashlib.file_digest(stream, 'sha256').hexdigest()
    digest = hashlib.sha256()
    for chunk in iter(lambda: stream.read(SHA256_CHUNK_BYTES), b''):
        digest.update(chunk)
    return digest.hexdigest()


def digest_file(path):
    with path.open('rb') as stream:
        return sha256_stream(stream)


# Module subfolders of each runtime tree (see docs/architecture.md section 7).
ASTRAL_RUNTIME_SUBDIRS=('gpu','audio','input','storage','services','kernel','tools')
LASRECOMP_SUBDIRS=('cpu','kernel','build','input','hle')

def runtime_include_flags(astral=None,reference=None):
    """Order matters - astral headers (e.g. ipc.h) shadow the vendored LASRecomp ones."""
    flags=[]
    for root,subdirs in ((astral,ASTRAL_RUNTIME_SUBDIRS),(reference,LASRECOMP_SUBDIRS)):
        if root is None:
            continue
        root=Path(root)
        flags+=['-I',str(root)]
        for sub in subdirs:
            flags+=['-I',str(root/sub)]
    return flags


def sdl3_include_flags(app_root):
    env_inc = os.environ.get('AC_SDL3_INCLUDE_DIR', '').strip()
    candidates = [Path(env_inc)] if env_inc else []
    candidates.append(Path(app_root) / 'usr' / 'include')
    for inc in candidates:
        if (inc / 'SDL3' / 'SDL.h').is_file():
            return ['-isystem', str(inc)]
    return []


def host_link_flags(app_root):
    flags = []
    link_lib = Path(app_root) / 'usr' / 'link-lib'
    if (link_lib / 'libvulkan.so').exists():
        flags += ['-L', str(link_lib), '-lvulkan']
    else:
        lib_search = ('/usr/lib/x86_64-linux-gnu', '/usr/lib64', '/usr/lib')
        vk_dir = next((d for d in lib_search if (Path(d) / 'libvulkan.so').exists()), None)
        if vk_dir:
            flags += ['-L', vk_dir, '-lvulkan']
        else:
            vk1_dir = next((d for d in lib_search if (Path(d) / 'libvulkan.so.1').exists()), None)
            if vk1_dir:
                flags += ['-L', vk1_dir, '-l:libvulkan.so.1']
            else:
                flags.append('-lvulkan')
        if is_portable_build():
            flags.append('-Wl,--allow-shlib-undefined')

    env_lib = os.environ.get('AC_SDL3_LIB_DIR', '').strip()
    candidates = [Path(env_lib)] if env_lib else []
    candidates.append(Path(app_root) / 'usr' / 'lib')
    sdl_added = False
    for lib_dir in candidates:
        if (lib_dir / 'libSDL3.so').exists():
            flags += ['-L', str(lib_dir), '-lSDL3', f'-Wl,-rpath,{lib_dir}']
            sdl_added = True
            break
        if (lib_dir / 'libSDL3.so.0').exists():
            flags += ['-L', str(lib_dir), '-l:libSDL3.so.0', f'-Wl,-rpath,{lib_dir}']
            sdl_added = True
            break
    if not sdl_added:
        flags.append('-lSDL3')

    flags += ['-lpthread', '-lm', '-ldl']
    return flags


def sdl3_runtime_env(app_root, base_env=None):
    env = dict(os.environ if base_env is None else base_env)
    runtime_lib = Path(app_root) / 'usr' / 'runtime-lib'
    if runtime_lib.is_dir():
        cur = env.get('LD_LIBRARY_PATH', '').strip()
        env['LD_LIBRARY_PATH'] = f'{runtime_lib}:{cur}' if cur else str(runtime_lib)
    return env


def build(compiler,reference,units,source,folder,cache,workers=4,extra_objects=()):
    """Cache only complete verified objects. Host runtime is rebuilt every round.

    Callers must not run simultaneous builds against the same cache directory.
    Worker threads own distinct content keys within this build.
    """
    if not 1<=workers<=8:
        raise ValueError('Native build worker count must be 1..8')
    compiler=Path(compiler).resolve();reference=Path(reference).resolve()
    folder=Path(folder).resolve();cache=Path(cache).resolve();cache.mkdir(parents=True,exist_ok=True)
    provenance={'compiler_sha256':digest_file(compiler),'flags':FLAGS,
                'headers':{p.name:digest_file(p) for p in sorted((reference/'runtime').rglob('*.h'))}}
    identity=json.dumps(provenance,sort_keys=True).encode()
    def compile_unit(item):
        group,text=item
        key=hashlib.sha256(identity+text.encode()).hexdigest()
        path=cache/key;path.mkdir(exist_ok=True)
        obj=path/'unit.obj';manifest=path/'complete.json'
        if manifest.is_file():
            saved=json.loads(manifest.read_text(encoding='utf-8'))
            if saved.get('key')!=key or not obj.is_file() or saved.get('sha256')!=digest_file(obj):
                raise FormatError('Cached native object integrity mismatch')
            return group,obj,True
        src=path/'unit.c';src.write_text(text,encoding='utf-8')
        result=run_compiler([str(compiler),'cc',*FLAGS,*runtime_include_flags(None, reference/'runtime'),'-c',str(src),'-o',str(obj)])
        (path/'compiler.log').write_text(result.stdout+result.stderr,encoding='utf-8')
        result.check_returncode()
        manifest.write_text(json.dumps({'key':key,'sha256':digest_file(obj),'provenance':provenance},indent=2)+'\n',encoding='utf-8')
        return group,obj,False
    # Distinct groups have distinct emitted page names, hence distinct keys.
    with ThreadPoolExecutor(max_workers=workers) as executor:
        compiled=list(executor.map(compile_unit,sorted(units.items())))
    src=folder/'trace.c';src.write_text(source,encoding='utf-8')
    src_root=Path(__file__).resolve().parent.parent.parent
    root=src_root.parent
    vk_headers=root/'third_party/Vulkan-Headers/include'
    command=[str(compiler),'cc',*FLAGS,*sdl3_include_flags(root),'-I',str(src_root),'-I',str(vk_headers),*runtime_include_flags(src_root/'runtime', reference/'runtime'),str(src),
             *(str(obj) for _,obj,_ in compiled),*(str(obj) for obj in extra_objects),
             *host_link_flags(root),
             '-Wl,-z,stack-size=67108864',  # 64 MB main-thread stack: direct calls nest page functions along guest calls
             '-o',str(folder/'trace')]
    result=run_compiler(command)
    (folder/'compiler.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    traces_dir=data_root(root)/'debug'/'traces';traces_dir.mkdir(parents=True,exist_ok=True)
    (traces_dir/'trace.c').write_text(source,encoding='utf-8')
    (traces_dir/'compiler.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    stats={'units':len(compiled),'reused':sum(hit for _,_,hit in compiled),'compiled':sum(not hit for _,_,hit in compiled)}
    (folder/'build.json').write_text(json.dumps(stats,indent=2)+'\n',encoding='utf-8')
    (folder/'unit_objects.json').write_text(json.dumps([str(obj.resolve()) for _,obj,_ in compiled],indent=2)+'\n',encoding='utf-8')
    result.check_returncode()
    return stats

