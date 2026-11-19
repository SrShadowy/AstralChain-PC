#pragma once

/**
 * sdk.hpp — Ponto de Entrada Raiz do Mini-SDK AstralChain-PC
 * 
 * Organização em subpastas por domínio:
 * - core/     : Tipos fundamentais, endereçamento em gmem, timers e áudio
 * - graphics/ : Patches de resolução/aspect ratio e pipeline Vulkan
 * - gameplay/ : Câmera, input/HID, menus/UI e estado do jogo PlatinumGames
 * - storage/  : Gestão de mods LayeredFS, overlays e persistência
 */

#include "core/types.hpp"
#include "core/memory.hpp"
#include "core/time.hpp"
#include "core/sound.hpp"

#include "graphics/patch.hpp"
#include "graphics/pipeline.hpp"

#include "gameplay/camera.hpp"
#include "gameplay/input.hpp"
#include "gameplay/ui.hpp"
#include "gameplay/game_state.hpp"

#include "storage/mods.hpp"
