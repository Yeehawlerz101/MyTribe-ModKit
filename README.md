# My Tribe — Display Scaling Mod & Mod Manager (Steam v1.02W)

A drop-in 32-bit `SDL.dll` display-scaling wrapper and lightweight mod manager for the Steam release of **My Tribe** (2008). It lets the classic `800×600` game run in a large, freely resizable window or true borderless native fullscreen without altering `MyTribe.exe`.

> **Disclaimer:** This repository contains only original mod code, build tools, configuration, and documentation. No copyrighted game binaries, original `SDL.dll`, or game assets are included.

---

## Features

* **Resizable & Maximizable Window:** Drag window edges freely or maximize to fill your screen instead of being locked to a tiny `800×600` window.
* **Native Borderless Fullscreen:** Converts fullscreen mode into a borderless window at your monitor's native resolution without jarring desktop resolution switches.
* **Stretched Full-Window Default (Zero Black Bars):** Defaults to `aspect = stretch` to fill 16:9 / ultrawide windows completely, with instant toggling back to exact 4:3 (`fit`) or integer scaling (`integer`).
* **OpenGL Hardware Scaling + GDI Fallback:** Uploads each `800×600` software frame to an OpenGL texture for smooth scaling (and automatically falls back to GDI `StretchDIBits` if OpenGL initialization fails).
* **Accurate Mouse Remapping:** Remaps mouse movement, clicks, `SDL_WarpMouse`, and `SDL_GetMouseState` cleanly between window and game coordinates.
* **Per-Monitor V2 DPI Awareness:** Crisp scaling on Windows 10/11 high-DPI displays.
* **Modular `mods/` Manager:** Automatically backs up vanilla files to `ModDevBackups\` on install, tracks all replaced/added files in a manifest, and cleanly restores 100% vanilla state on uninstall.

---

## In-Game Hotkeys

| Hotkey | Action |
| :--- | :--- |
| **`F11`** or **`Alt+Enter`** | Toggle between borderless native fullscreen and resizable windowed mode |
| **`F9`** | Toggle scaling filter (`linear` smooth ↔ `nearest` crisp pixels) |
| **`F10`** | Cycle aspect mode (`stretch` → `fit` 4:3 → `integer` → `stretch`) |

---

## Installation

### Option 1: Mod Manager (Recommended)

**Prerequisite:** [Python 3](https://www.python.org/downloads/) installed and available as `python` in PATH.

1. Open your **My Tribe** Steam folder (typically `...\SteamLibrary\steamapps\common\My Tribe\`).
2. Copy the `mods\` folder from this repository directly into the **My Tribe** folder so `...\My Tribe\mods\install_mods.bat` exists.
3. Double-click **`mods\install_mods.bat`**.
   * On first run, the mod manager automatically creates `ModDevBackups\` and saves a backup of your original `SDL.dll` (and any other vanilla files modified by enabled mods).
   * It deploys `SDL_orig.dll` (your vanilla SDL), our proxy `SDL.dll`, and `mytribe_display.ini` into the game folder.
4. Launch **My Tribe** normally through Steam (or via `MyTribe.exe`).

### Option 2: Manual Install (No Python Required)

1. Open your **My Tribe** game folder (`...\steamapps\common\My Tribe\`).
2. Rename the original `SDL.dll` to **`SDL_orig.dll`** *(do not skip this step — the proxy forwards calls to `SDL_orig.dll`)*.
3. Copy `mods\display_scaling\SDL.dll` and `mods\display_scaling\mytribe_display.ini` from this repo into your **My Tribe** folder.
4. Launch **My Tribe** normally.

To manually uninstall later: delete `SDL.dll`, `mytribe_display.ini`, and `mytribe_display.log`, then rename `SDL_orig.dll` back to `SDL.dll`.

---

## Configuration (`mytribe_display.ini`)

> **Tip:** If using the Mod Manager, edit **`mods\display_scaling\mytribe_display.ini`** and re-run `mods\install_mods.bat` for changes to persist across reinstalls, or edit `mytribe_display.ini` in the game root directly for quick testing.

| Setting | Values | Default | Description |
| :--- | :--- | :--- | :--- |
| `mode` | `auto`, `windowed`, `borderless` | `auto` | `auto` follows the game's Full Screen setting (borderless native vs. resizable window); `windowed` or `borderless` forces startup mode |
| `window_width` | integer (`0` = auto) | `0` | Initial window width in windowed mode (`0` auto-sizes to ~85% of screen work-area height at 4:3) |
| `window_height` | integer (`0` = auto) | `0` | Initial window height in windowed mode |
| `aspect` | `stretch`, `fit`, `integer` | `stretch` | `stretch` fills the entire window; `fit` keeps exact 4:3 with black bars; `integer` uses the largest integer scale multiple |
| `filter` | `linear`, `nearest` | `linear` | `linear` smooth bilinear filtering; `nearest` crisp nearest-neighbor pixels |
| `vsync` | `1`, `0` | `1` | Enable (`1`) or disable (`0`) vertical sync in OpenGL mode |
| `unlock_mouse_in_windowed` | `1`, `0` | `1` | Allow the mouse cursor to leave the window freely in windowed mode |
| `log` | `1`, `0` | `1` | Write diagnostic output to `mytribe_display.log` in the game root |

---

## Managing, Disabling & Uninstalling Mods

* **Check mod status:** Double-click **`mods\status_mods.bat`**.
* **Disable a mod:** Move its folder (e.g., `mods\display_scaling`) into `mods\_disabled\` (or rename it with a leading underscore like `_display_scaling`), then run **`mods\install_mods.bat`**.
* **Restore pure vanilla:** Double-click **`mods\uninstall_mods.bat`**. This restores every overwritten vanilla file from `ModDevBackups\` and deletes all mod-added files and logs.

---

## Building from Source

**Requirements:**
* Windows with **Visual Studio Build Tools** (Desktop development with C++ / MSVC x86 tools + Windows SDK)
* Python 3 (only needed if regenerating `src\sdl_proxy.def` from a vanilla `SDL.dll`)

To rebuild `mods\display_scaling\SDL.dll`:
1. Open a command prompt in `src\` (or just double-click **`src\build.bat`** — it automatically locates `vswhere.exe` and initializes the 32-bit `x86` MSVC environment).
2. Optionally pass a path to a vanilla `SDL.dll` as the first argument to regenerate `sdl_proxy.def`:
   ```bat
   build.bat "C:\path\to\vanilla\SDL.dll"
   ```
3. `build.bat` compiles `src\sdl_proxy.cpp` (`/O2 /MT`, static CRT), builds the `.exp` forwarder table from `src\sdl_proxy.def` using `lib.exe`, and links `mods\display_scaling\SDL.dll` against `user32.lib`, `gdi32.lib`, and `opengl32.lib`.

---

## Modding Guide

See **[INFO.md](INFO.md)** for full technical documentation on reverse-engineering and modding the Steam version of **My Tribe**, including archive formats (`resource*.dat`), localization (`strings.dat`), save checksums (`PROFILE*v8.DAT`), SteamStub constraints, and packaging custom mods for `mods\`.
