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
│   ├── memory.hpp / .cpp   # Resolução de base em gmem (0x7000010000) e safe reads/writes
│   └── time.hpp / .cpp     # Singleton de tempo, delta real do frame e FPS
├── graphics/               # Motor Gráfico, Resolução & Vulkan
│   ├── patch.hpp / .cpp    # Sites SVC #0xB3, aspect ratio ultrawide/16:10, grid e pools
│   └── pipeline.hpp / .cpp # Hashes de shaders UI/vídeo e atlas de glifos Xbox
└── gameplay/               # Engine da PlatinumGames, Câmera e Estado
    ├── camera.hpp / .cpp   # Vtables da câmera, pitch/yaw e injeção de mouse raw
    ├── input.hpp / .cpp    # HID Switch (g_PadInputState) e botões
    ├── ui.hpp / .cpp       # Detecção atômica de menu/pausa e liberação de cursor
    └── game_state.hpp / .cpp # Cenas, fases, missões, bosses, Legiões e idioma Wwise
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
