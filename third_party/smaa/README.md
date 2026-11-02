# SMAA 1x (Subpixel Morphological Antialiasing)

- **Upstream**: https://github.com/iryoku/smaa (`master`)
- **License**: MIT — see `LICENSE.txt` (Jimenez, Echevarria, Masia, Navarro, Gutierrez, 2013).

`SMAA.hlsl` is the reference shader source, vendored verbatim. The runtime build
generates two artifacts from it into `astral_chain/runtime/las/nvn/`:

- `smaa_hlsl.inc` — `SMAA.hlsl` escaped into a C string (the shader body; the
  `#version`/defines/entry points are prepended/appended by `nvn/vulkan/shaders.c`).
- `AreaTex.h` / `SearchTex.h` — the precomputed lookup textures (160×560 R8G8 and
  64×16 R8) from the upstream `Textures/` directory.

The C symbols (`smaa_hlsl_src`, `smaa_area_tex`, `smaa_search_tex`) are exposed by
`astral_chain/runtime/las/nvn/smaa_data.c` and consumed by the Vulkan post passes.
