"""Cached objects for the vendored LASRecomp NVN layer (runtime/las).

Sources are MIT-licensed LASRecomp files at a recorded commit, plus local shims;
see runtime/las/README.md. Objects are keyed by compiler, flags and every input
file so a stale object is never linked.

Backends:
- null: diagnostic; executes and draws nothing.
- vulkan: LASRecomp's Vulkan backend, presenting into the runtime window. Needs
  the pinned Vulkan-Headers and VulkanMemoryAllocator checkouts under
  third_party/, glslang under tools/ (setup.cmd downloads it), and the system vulkan-1.dll.
"""
import hashlib
import json
from pathlib import Path
import subprocess

from astral_chain.build.native_build import FLAGS, digest_file, run_compiler, runtime_include_flags
from .shader_build import prepare_translator

SRC = Path(__file__).resolve().parent.parent.parent
LAS = SRC/'runtime/las'
PROJECT = SRC.parent
VULKAN_HEADERS = PROJECT/'third_party/Vulkan-Headers/include'
VMA = PROJECT/'third_party/VulkanMemoryAllocator/include'
IMGUI = PROJECT/'third_party/imgui'  # [extra] Dear ImGui 1.92.9b (MIT, see LICENSE.txt there)
GLSLANG = PROJECT/'tools/glslang/bin/glslang'
OBJECT_LAYER = ('nvn/objects.c', 'nvn/cmdbuf.c', 'nvn/formats.c', 'nvn/tegra.c', 'nvn/astc.c')
VULKAN_LAYER = ('nvn/vulkan/device.c',
                'nvn/vulkan/device/instance.c',
                'nvn/vulkan/device/physical_device.c',
                'nvn/vulkan/device/logical_device.c',
                'nvn/vulkan/device/surface.c',
                'nvn/vulkan/device/swapchain.c',
                'nvn/vulkan/device/present.c',
                'nvn/vulkan/device/postprocess.c',
                'nvn/vulkan/exec.c',
                'nvn/vulkan/exec/cache/vcopy.c',
                'nvn/vulkan/exec/cache/gcopy.c',
                'nvn/vulkan/exec/cache/copy_elision.c',
                'nvn/vulkan/exec/cache/vcache.c',
                'nvn/vulkan/exec/memory/transient.c',
                'nvn/vulkan/exec/memory/heap_mirror.c',
                'nvn/vulkan/exec/memory/gfast.c',
                'nvn/vulkan/exec/pipelines/pipeline_workers.c',
                'nvn/vulkan/exec/pipelines/vertex_format.c',
                'nvn/vulkan/exec/pipelines/shader_library.c',
                'nvn/vulkan/exec/pipelines/pipeline.c',
                'nvn/vulkan/exec/pipelines/program.c',
                'nvn/vulkan/exec/pipelines/gpu_warm.c',
                'nvn/vulkan/exec/commands/clear.c',
                'nvn/vulkan/exec/commands/render_pass.c',
                'nvn/vulkan/exec/commands/compute.c',
                'nvn/vulkan/exec/commands/copy.c',
                'nvn/vulkan/exec/commands/present.c',
                'nvn/vulkan/exec/commands/draw.c',
                'nvn/vulkan/exec/descriptors/descriptors.c',
                'nvn/vulkan/exec/submission/submission.c',
                'nvn/vulkan/exec/submission/frame_stats.c',
                'nvn/vulkan/textures.c', 'nvn/vulkan/shaders.c',
                'nvn/vulkan/spirv_cache.c', 'nvn/vulkan/astc_gpu.c', 'nvn/vulkan/bc7_gpu.c',
                'shaderc_glslang.c', 'game_hooks_neutral.c', 'button_prompts.c', 'nvn/smaa_data.c', 'vma.cpp')
BACKENDS = {'null': ('null_backend.c',), 'vulkan': VULKAN_LAYER}


def nvn_sources(backend):
    if backend not in BACKENDS:
        raise ValueError(f'unknown NVN backend {backend!r}')
    sources = [LAS/name for name in OBJECT_LAYER+BACKENDS[backend]]
    # [extra] Runtime toggles overlay (src/additions/toggles/ac_toggles.c) rides in every
    overlay = SRC/'additions'/'overlay'
    rpc = SRC/'additions'/'rpc'
    sources.append(SRC/'additions'/'toggles'/'ac_toggles.c')
    if backend == 'vulkan':
        sources += sorted(IMGUI.glob('*.cpp'))
        sources += [IMGUI/'backends'/'imgui_impl_sdl3.cpp', IMGUI/'backends'/'imgui_impl_vulkan.cpp',
                    overlay/'ac_overlay.cpp', overlay/'ac_vk_glue.cpp', overlay/'ac_hud.cpp', overlay/'ac_menu.cpp',
                    overlay/'ac_reinfo.cpp', overlay/'ac_reinfo_probe.c',
                    rpc/'ac_game_data.cpp', rpc/'ac_discord_rpc.cpp', overlay/'ac_overlay_rpc.cpp']
    return sources


def build_nvn_objects(compiler, reference, cache, backend='null'):
    """Return (objects, link_arguments) for the chosen backend."""
    compiler=Path(compiler).resolve();reference=Path(reference).resolve()
    inputs=sorted(p for p in LAS.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.cpp','.inc'))
    extra_files=sorted(p for d in (SRC/'hooks', SRC/'additions') if d.is_dir() for p in d.rglob('*') if p.is_file() and p.suffix in ('.h','.c','.cpp'))
    provenance={'compiler_sha256':digest_file(compiler),'flags':FLAGS,'backend':backend,
                'inputs':{p.relative_to(LAS).as_posix():digest_file(p) for p in inputs},
                'extra':{p.relative_to(SRC).as_posix():digest_file(p) for p in extra_files},
                'cpu_h':digest_file(reference/'runtime/cpu/cpu.h')}
    if backend=='vulkan':
        provenance['vma_sha256']=digest_file(VMA/'vk_mem_alloc.h')
        provenance['vulkan_core_sha256']=digest_file(VULKAN_HEADERS/'vulkan/vulkan_core.h')
        provenance['imgui']={p.name:digest_file(p) for p in sorted(IMGUI.rglob('*.h'))}  # [extra]
        provenance['translator_sha256']=digest_file(reference/'runtime/nvn/maxwell/translate.c')
        provenance['translator_patches_sha256']=digest_file(Path(__file__).parent/'shader_build.py')
    key=hashlib.sha256(json.dumps(provenance,sort_keys=True).encode()).hexdigest()
    folder=Path(cache).resolve()/f'nvn-{key}';manifest=folder/'complete.json'
    sources=nvn_sources(backend)
    if backend=='vulkan':
        sources=sources+[folder/'translator/translate-reviewed.c']
    def object_of(source):
        if source.is_relative_to(folder):
            name = source.name
        elif source.is_relative_to(LAS):
            name = source.relative_to(LAS).as_posix().replace('/','_')
        else:  # [extra] hooks/additions and third_party/imgui sources: unique flat name with origin tag
            tag = 'imgui' if source.is_relative_to(IMGUI) else 'extra'
            name = f'{tag}_{source.name}'.replace('/','_')
        return folder/(name+'.o')
    objects=[object_of(s) for s in sources]
    link=['-lvulkan','-lc++'] if backend=='vulkan' else []
    if manifest.is_file():
        saved=json.loads(manifest.read_text(encoding='utf-8'))
        if saved.get('key')!=key or not all(o.is_file() and saved['objects'].get(o.name)==digest_file(o) for o in objects):
            raise ValueError('Cached NVN object integrity mismatch')
        return objects,link
    folder.mkdir(parents=True,exist_ok=True)
    common=['-I',str(SRC),'-I',str(LAS),*runtime_include_flags(None, reference/'runtime'),'-I',str(IMGUI),'-I',str(IMGUI/'backends')]  # [extra] imgui
    if backend=='vulkan':
        prepare_translator(reference,folder/'translator')
        common+=['-I',str(LAS/'shim'),'-I',str(VULKAN_HEADERS),'-I',str(VMA),
                 '-I',str(reference/'runtime/nvn/maxwell')]
    log=[]
    for source,obj in zip(sources,objects):
        backend_file=source.is_relative_to(folder) or 'vulkan' in source.parts or source.name in VULKAN_LAYER
        if source.suffix=='.cpp':
            inc = common
            if source.name in ('imgui_impl_sdl3.cpp','ac_overlay.cpp'):
                # [extra] these need the real system SDL3; las/shim (-I) would win over -isystem, so demote it
                shim = str(LAS/'shim')
                i = inc.index(shim)
                inc = inc[:i-1] + inc[i+1:]
                inc = ['-isystem','/usr/include','-isystem',shim] + inc
            command=[str(compiler),'c++',*FLAGS,'-std=c++17',*inc,'-c',str(source),'-o',str(obj)]
        else:
            extra=['-include',str(LAS/'las_compat.h')] if backend_file else []
            command=[str(compiler),'cc',*FLAGS,*extra,*common,'-c',str(source),'-o',str(obj)]
        result=run_compiler(command)
        log.append(' '.join(command)+'\n'+result.stdout+result.stderr)
        if result.returncode:
            (folder/'compiler.log').write_text('\n'.join(log),encoding='utf-8')
            result.check_returncode()
    (folder/'compiler.log').write_text('\n'.join(log),encoding='utf-8')
    manifest.write_text(json.dumps({'key':key,'objects':{o.name:digest_file(o) for o in objects},
                                    'provenance':provenance},indent=2)+'\n',encoding='utf-8')
    return objects,link
