#pragma once

#ifndef _WIN32
#include "extra/platform/platform_posix.h"
#include "extra/platform/crash_trampoline.h"
#include "extra/platform/crypto_trampoline.h"
#include "extra/input/vk_codes.h"
#include "extra/platform/sync_trampoline.h"
#include "extra/platform/memory_trampoline.h"
#include "extra/platform/shared_memory_trampoline.h"
#include "extra/input/mouse_mode.h"
#include "extra/input/mouse_menu.h"
#include "extra/sdl/window_trampoline.h"
#include "extra/input/menu_detect.h"
#include "extra/gpu/vulkan_trampoline.h"
#include "extra/sdl/audio_trampoline.h"
#include "extra/storage/storage_trampoline.h"
#include "extra/storage/romfs_overlay.h"
#include "extra/storage/find_trampoline.h"
#include "extra/platform/time_trampoline.h"
#include "extra/ui/font_trampoline.h"
#include "extra/gpu/write_tracking_extra.h"
#include "extra/gpu/vtx_debug_extra.h"
#endif
