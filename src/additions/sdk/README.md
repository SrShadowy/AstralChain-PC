# ac_sdk — Mini-SDK Nativo (AstralChain-PC)

O `ac_sdk` é a camada de engenharia reversa de fora para dentro para manipulação direta de subsistemas internos da engine da PlatinumGames em *Astral Chain* (Nintendo Switch ARM64 recompilado para Linux x86_64).

## Estrutura de Diretórios

O SDK é dividido por domínios de especialidade, separando estritamente tipos, motor gráfico e subsistemas de gameplay:

```
sdk/
├── sdk.hpp                 # Header raiz do SDK
├── ac_sdk.hpp              # Alias de compatibilidade
├── README.md               # Documentação técnica
├── core/                   # Tipos fundamentais e infraestrutura de memória
│   ├── types.hpp           # Vetores (Vec3, Vec4, StickVector) e estilos de controle
│   ├── memory.hpp / .cpp   # Resolução em gmem, scans AOB e decodificador ADRP+LDR ARM64
│   ├── time.hpp / .cpp     # Singleton de tempo, 144+ FPS e invariantes de combate
│   └── sound.hpp / .cpp    # Volumes master/BGM e controle de foco de áudio
├── graphics/               # Motor Gráfico, Resolução & Vulkan
│   ├── patch.hpp / .cpp    # Sites SVC #0xB3, aspect ratio ultrawide/16:10, grid e pools
│   └── pipeline.hpp / .cpp # Hashes de shaders UI/vídeo e atlas de glifos Switch/PS/KBM
├── gameplay/               # Engine da PlatinumGames, Câmera e Estado
│   ├── camera.hpp / .cpp   # Vtables da câmera, pitch/yaw e injeção de mouse raw
│   ├── input.hpp / .cpp    # HID Switch (g_PadInputState) e botões
│   ├── ui.hpp / .cpp       # Detecção atômica de menu/pausa e liberação de cursor
│   └── game_state.hpp / .cpp # Cenas, fases, missões, Legiões e localização (Texto/Voz)
└── storage/                # Gestão de Mods e Camada LayeredFS
    └── mods.hpp / .cpp     # Descoberta, listagem e alternância atômica (.disabled) de mods
```

## Exemplo de Uso

```cpp
#include "sdk/sdk.hpp"

void update_frame_mouse(float dx, float dy) {
    if (ac::ui::is_menu_open()) {
        // Libera cursor do sistema quando há menu aberto
        return;
    }

    // Injeção de mouse 1:1 na câmera ativa
    uintptr_t camera_ptr = get_active_camera_ptr();
    ac::camera::inject_mouse_delta(camera_ptr, dx, dy, 0.002f, 0.002f);
}

void check_boss_encounter() {
    if (ac::game_state::is_boss_active()) {
        uint32_t boss_id = ac::game_state::get_boss_id();
        // Lógica de HUD ou telemetria
    }
}
```

## Blindagem contra Offsets Fixos (AOB & Decodificação ADRP+LDR)

Para suportar múltiplas revisões de dump (ex: v1.0.0 vs v1.0.1) ou mods sem quebrar ponteiros globais (`SLOT_*`), o `ac_sdk` integra resolução dinâmica de símbolos:

- `ac::memory::scan_aob(pattern)`: Varredura vetorial com wildcards `??` na seção `.text`.
- `ac::memory::decode_adrp_ldr(code_addr)`: Decodifica instruções ARM64 (`ADRP` + `LDR`/`ADD`) em tempo de execução para extrair o endereço absoluto/relativo do slot de memória de forma imune a deslocamentos de código.
- `ac::memory::resolve_slot_from_aob(pattern, fallback)`: Busca a assinatura da função-âncora, decodifica o slot associado e, caso a memória guest não esteja mapeada (ex: testes unitários offline), utiliza o offset fallback seguro.

Toda a tabela de assinaturas únicas e suas funções-âncora mapeadas é gerada e verificada por `tools/qa/extract_symbols.py` e registrada em `docs/symbols.toml`.
