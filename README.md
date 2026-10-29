# Astral Chain PC (Linux Port)

> [!IMPORTANT]
> **Original Project Disclosure:** The upstream Astral Chain PC port was originally developed by Puddz with AI models (Anthropic's Claude through Claude Code and Codex), directed, tested and reviewed by the original project owner. This Linux adaptation builds directly upon that foundation, transitioning the runtime from Windows/Win32 to native Linux (Qt6, SDL3, and Vulkan 1.3) through modular hooks and trampolines. It is shared as-is as a fan project for educational and research purposes.

An unofficial, experimental native Linux port of *Astral Chain* (Nintendo Switch, PlatinumGames, 2019), achieved via **static recompilation**. The game's ARM64 machine code is translated to C and compiled into an ordinary native Linux x86_64 binary, and its NVN graphics calls run natively on Vulkan. There is no emulator or JIT involved.

You build it yourself from **your own** legally dumped game and keys. This repository contains no game code, assets, shaders, keys or firmware, and never will. Everything produced during setup originates from your dump: do not distribute the `build/`, `game/` or `userdata/` directories.

---

## Status

| Component | Status |
| --- | --- |
| Platform | Linux x86_64 (tested on modern distributions with Vulkan 1.3 and SDL3) |
| Graphics | NVN layer rendered via Vulkan 1.3; window and surface managed by SDL3 |
| Audio | Output managed via SDL3 Audio Stream (float32 interleaved, replacing legacy waveOut) |
| Memory Management | POSIX `mmap` (`MAP_NORESERVE | MAP_FIXED_NOREPLACE`), `mprotect`, and `SIGSEGV` lazy write-back handler |
| Synchronization | POSIX `pthread_mutex`, `pthread_cond` and monotonic clock timers |
| Launcher | Native C++20 Qt6 application with configuration management, game art cycling, and controls rebinding |
| Frame Rate | 60 fps target, cutscenes included (Dynamic-FPS patch). Set `AC_CUTSCENE_FPS=30` to run cutscenes at original 30 fps |
| Shaders | Precompiled ahead of time for your specific GPU using native glslang and Vulkan pipeline cache |

---

## Requirements

- **Operating System:** 64-bit Linux distribution (Arch Linux, Ubuntu 24.04+, Fedora 40+, etc.).
- **GPU & Driver:** GPU supporting Vulkan 1.3 with up-to-date proprietary or Mesa drivers.
- **System Libraries:**
  - `SDL3` (`libsdl3`)
  - `Vulkan Loader` (`libvulkan`)
  - `Qt6` (Core, Gui, Widgets for the launcher)
  - `CMake` (3.20+) and `Ninja` / `GCC` or `Clang` with C++20 support
  - `Python 3.10+` with virtual environment (`python3-venv`)
- **Disk Space:**
  - Approximately **35 GB** free during setup (about 11 GB retained after temporary file cleanup).
- **Your Own Dump:**
  - The **base game** (title ID `01007300020FA000`), as an XCI or NSP;
  - The **v1.0.1 update** NSP (exactly v1.0.1 is required);
  - Your console's **`prod.keys`**.

---

## Quick Start

### 1. Dependencies Installation

On Arch Linux / CachyOS / Manjaro:
```bash
sudo pacman -S base-devel cmake ninja sdl3 vulkan-tools vulkan-icd-loader qt6-base python
```

On Ubuntu / Debian:
```bash
sudo apt install build-essential cmake ninja-build libsdl3-dev libvulkan-dev qt6-base-dev python3 python3-venv
```

On Fedora:
```bash
sudo dnf install cmake ninja-build sdl3-devel vulkan-loader-devel qt6-qtbase-devel python3
```

### 2. Setup the Game

Run the setup script:
```bash
./setup.sh
```
The script creates a Python virtual environment, installs the required packages, downloads the pinned Linux toolchain (Zig, glslang, NSTool), prompts you for your `prod.keys`, base game, and v1.0.1 update, extracts the data, and compiles the native game and initial shaders.

You can also pass arguments directly:
```bash
./setup.sh --keys /path/to/prod.keys --base /path/to/base.nsp --update /path/to/update.nsp
```

### 3. Launch the Game

#### Option A: Native Qt6 Launcher
```bash
./launcher.sh
```
Or launch directly from `build-launcher/astral_chain_launcher`.
The launcher allows launching the game, running with diagnostics, configuring graphics and audio settings, rebinding controls, and precompiling shaders.

#### Option B: Terminal / Script Runners
- **Standard execution:**
  ```bash
  ./play.sh
  ```
- **Execution with profiling and diagnostics:**
  ```bash
  ./play-diagnostics.sh
  ```

---

## Controls

The port supports both keyboard/mouse and gamepad input:
- **Default bindings:**
  - Left click: Attack / Action (ZR)
  - Right click: Call / Dismiss Legion (ZL)
  - Mouse movement: Camera control
  - `W` / `A` / `S` / `D`: Character movement (Left Stick)
  - `Shift`: Sprint
  - `Space`: Dodge / Evade (Right Stick click)
  - `I` / `J` / `K` / `L`: Face buttons (`X`, `Y`, `B`, `A`)
  - `Tab`: IRIS scan (`Minus`)
  - `Enter`: Pause menu (`Plus`)
- Rebinding can be performed via the **Controls** dialog in `./launcher.sh` or edited directly in `userdata/launcher.ini`.

---

## Architecture & Project Structure

The codebase is organized to maintain modularity and ease upstream merges with the original Windows repository:

| Path | Description |
| --- | --- |
| `extra/core/` | Modular platform hooks and trampolines (`sync`, `memory`, `window`, `vulkan`, `audio`, `storage`, `platform_posix`) |
| `extra/ui/launcher/` | Native Qt6 C++20 launcher (`ac::launcher` namespace, modular classes <= 250 lines) |
| `extra/tools/` | Verification and compliance audit scripts (`audit.sh`) |
| `extra/hooks.h` | Downstream master re-export header for non-intrusive upstream hooks |
| `launcher.sh` | Build and execution helper for the Qt6 launcher |
| `setup.sh` | Linux virtual environment setup and game data unpacker |
| `play.sh`, `play-diagnostics.sh` | Linux game runners with environment guards |
| `astral_chain/runtime/hooks/` | Upstream forwarding headers preserving historical include paths |
| `astral_chain/` | Native lifter driver, build system, and game runtime |
| `astral_chain/patches/` | Pchtxt code patches (Dynamic-FPS, cutscene sync) |
| `third_party/` | Submodules and vendored components (LASRecomp, Vulkan-Headers, VulkanMemoryAllocator) |
| `SHADOW_CHANGES.md` | Upstream Touch Ledger and modular code architecture rules |

---

## Legal Notice

This is an unofficial fan project, not affiliated with, endorsed by, or sponsored by Nintendo or PlatinumGames. *Astral Chain* is a registered trademark of Nintendo. No copyrighted game code, assets, ROMs, shaders, keys, or firmware files are included or distributed in this repository.

The code modifications for Linux compatibility are provided under the same MIT License as the original project (see `LICENSE`). Vendored components retain their respective licenses, detailed in `THIRD_PARTY_NOTICES.md`.

---

## Credits & Acknowledgments

- **[Puddz](https://github.com/Puddz/AstralChain-PC)**: Author and creator of the original Astral Chain PC port and recompilation pipeline.
- **Anthropic Claude & OpenAI Codex**: AI models utilized in authoring, reverse engineering, and documenting the original implementation.
- **[LASRecomp](https://github.com/peiikai/LASRecomp)** (MIT): The ARM64 static lifter, NVN-on-Vulkan translation layer, and Maxwell shader translator upon which this port is built.
- **[AstralChain-Dynamic-FPS](https://github.com/yogesh1239/AstralChain-Dynamic-FPS)** by yogesh1239 (MIT): The 60 fps dynamic timing patch.
- **[astral-extractor](https://github.com/cabalex/astral-extractor)** by cabalex: PlatinumGames PKZ / DAT archive research and format specifications.
- **[AC-Recomp](https://github.com/swaymun/AC-Recomp)**: The macOS recompilation project whose setup pipeline inspired the CLI workflow.
- **[SDL (Simple DirectMedia Layer)](https://github.com/libsdl-org/SDL)**: Cross-platform windowing, event handling, and audio streaming for Linux.
- **[The Qt Project](https://www.qt.io/)**: Modern C++ application framework powering the Linux launcher.
- **Tooling & Libraries:**
  - [Zig](https://ziglang.org/)
  - [glslang](https://github.com/KhronosGroup/glslang)
  - [NSTool](https://github.com/jakcron/nstool)
  - [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)
  - [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
  - [Capstone Engine](https://www.capstone-engine.org/)
  - [lz4](https://github.com/python-lz4/python-lz4)
