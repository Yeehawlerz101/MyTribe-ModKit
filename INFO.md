# Modding Guide: My Tribe (Steam v1.02W)

Technical reference for modding the Steam release of **My Tribe** (`v1.02W`, Grubby Games / 2008).

---

## 1. Engine & Executable Architecture

* **Executable:** `MyTribe.exe` (`2,858,504` bytes)
  * **Architecture:** 32-bit x86 (`PE32`, machine `0x014C`)
  * **Compiler:** MinGW GCC 3.x (`C++` with GCC RTTI class names present in `.rdata`)
  * **Game Version String:** `1.02W`
* **Runtime Libraries:**
  * **Graphics / Windowing / Input / Text:** SDL 1.2.11 (`SDL.dll`, `SDL_image.dll`, `SDL_ttf.dll`), `libpng1.dll`, `jpeg.dll`, `libfreetype-6.dll`, `zlib.dll` / `zlib1.dll`
  * **Audio:** FMOD Ex (`fmodex.dll`)
* **Fixed Internal Layout (`800×600`):**
  * The engine renders to a fixed `800×600` software framebuffer across all screen controllers (`CControllerMain`, `CControllerGame`, `CControllerBuild`, `CControllerIslandSelect`, `CControllerPersonDetail`, etc.).
  * Key UI textures are authored directly at `800×600` or `800px`/`600px` spans (e.g., `mainmenu_main.jpg` `800×600`, `settings_bg.jpg` `800×600`, `trophies_bg.jpg` `800×600`, `islandselector_mainE.jpg` `800×93`, `sidebar_mainC.jpg` `156×600`, `sidebar_trim_right.png` `95×600`).
  * There are **no** external `.lua`, `.xml`, `.ini`, or `.json` gameplay scripts — core entities (`CPerson`, `CBuilding`, `CHut`, `CFarm`, `CScienceStation`, `CObservatory`, `CCraftworks`, `CShipyard`, `CWonderA`, `CWonderB`) are compiled directly into `MyTribe.exe`.
* **SteamStub (`.bind` Section) Constraints:**
  * `MyTribe.exe` is wrapped with SteamStub: it has a `.bind` section (`VA 0x002CB000`) and its `.text` code section (`VA 0x00001000`, entropy `8.000`) is encrypted on disk until launched via Steam (while `.rdata` is unencrypted).
  * If you copy `MyTribe.exe` into another folder and launch it directly, the Steam wrapper hands off execution to Steam, which launches the **registered Steam install directory** instead.
  * Because modifying `MyTribe.exe` on disk breaks SteamStub / decryption, code-level and QoL mods should be implemented via **32-bit DLL proxies** (`SDL.dll`, `SDL_image.dll`, `SDL_ttf.dll`, `fmodex.dll`), leaving `MyTribe.exe` untouched.

---

## 2. Resource Archives (`resource.dat`, `resource2.dat`, `resource3.dat`)

All three `resource*.dat` files share a simple, uncompressed custom archive format:

### Binary Layout (Little-Endian)
```text
[Header]
  uint32 entry_count
  uint32 offsets[entry_count]

[At each offsets[i]]
  uint32 data_size
  uint32 name_len
  char   name[name_len]       // ASCII/Latin-1 filename, NO null terminator
  uint8  data[data_size]      // Raw file bytes
```

### Important Constraints
1. **Case-Insensitive Sort Order:** All entries in `resource.dat`, `resource2.dat`, and `resource3.dat` are stored **sorted case-insensitively** (`name.lower()`) — keep entries sorted case-insensitively when repacking so binary-search lookups succeed.
2. **Matching Sprite Frame Sizes:** `CSprite` logs warnings/errors if frames in an animation sequence have mismatched dimensions (`Original w/h ... New w/h ...`), so keep replacement sprite frames the exact same pixel dimensions as the originals.

### Archive Contents Summary

| Archive | Size | Entries | Contents |
| :--- | ---: | ---: | :--- |
| `resource.dat` | 37,253,436 B | 13,832 | `13,435 PNG`, `380 JPG`, `9 OGG` (`female_huh_*.ogg`), `8 TGA` (mouth blanks, cursors, rain splashes) |
| `resource2.dat` | 2,135,108 B | 98 | All game sound effects (`.ogg`) |
| `resource3.dat` | 71,611 B | 4 | Game fonts (`CC.otf`, `Fizzball 2.ttf`, `Zoinks.otf`, plus `.DS_Store`) |

### Reference Python Unpacker / Repacker

```python
import os, struct

def unpack_dat(dat_path: str, out_dir: str) -> None:
    os.makedirs(out_dir, exist_ok=True)
    with open(dat_path, "rb") as f:
        data = f.read()
    count = struct.unpack_from("<I", data, 0)[0]
    offsets = struct.unpack_from(f"<{count}I", data, 4)
    for off in offsets:
        size, namelen = struct.unpack_from("<II", data, off)
        name = data[off + 8 : off + 8 + namelen].decode("latin1")
        payload = data[off + 8 + namelen : off + 8 + namelen + size]
        dst = os.path.join(out_dir, name)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as out:
            out.write(payload)

def pack_dat(src_dir: str, dat_path: str) -> None:
    names = sorted(
        (fn for fn in os.listdir(src_dir) if os.path.isfile(os.path.join(src_dir, fn))),
        key=str.lower,
    )
    header_size = 4 + 4 * len(names)
    cursor = header_size
    offsets, records = [], []
    for name in names:
        with open(os.path.join(src_dir, name), "rb") as f:
            payload = f.read()
        name_b = name.encode("latin1")
        offsets.append(cursor)
        rec = struct.pack("<II", len(payload), len(name_b)) + name_b + payload
        records.append(rec)
        cursor += len(rec)
    with open(dat_path, "wb") as out:
        out.write(struct.pack("<I", len(names)))
        out.write(struct.pack(f"<{len(names)}I", *offsets))
        for rec in records:
            out.write(rec)
```

---

## 3. Localization, Text & Font Styling (`strings.dat`)

`strings.dat` (`429,100` bytes, `3,818` lines) is a plain **UTF-16 Little-Endian** text file (must keep the UTF-16 LE BOM `FF FE` when edited). It contains `3,468` string entries and `183` font directives.

### Syntax
* **`# ...`** — Comment line (ignored).
* **`!FONT|SIZE|R|G|B|LINE_SPACING`** — Font directive that applies to every `@` string below it until the next `!` line (e.g., `!CC.otf|18|251|252|219|3`). Font filenames correspond to entries inside `resource3.dat`.
* **`@ID|TEXT`** — Single-line string definition keyed by unique numeric ID.

### Inline Formatting & Variable Tags
* `%b ... %b` — Toggle bold
* `%i ... %i` — Toggle italics
* `%u ... %u` — Toggle underline
* `%n` — Line break (no raw newlines allowed inside `@ID|TEXT`)
* `%1`–`%6` — Runtime value placeholders filled by game code
* `%7`–`%a` — Cross-references replaced by another string ID from `strings.dat`

### Major ID Ranges
* `1000–2000`: Sidebar
* `2000–3000`: Sidebar detail info
* `3000–4000`: Sidebar detail buttons
* `4000–5000`: Resource / Stardust / Moondust counters
* `6000–7000`: Island selector, suffixes & island names
* `7000–8000`: Villager names (male & female)
* `8000–9000`: Specialties
* `9000–10000`: Villager actions (adults, kids, babies)
* `10000–11000`: Build GUI names & descriptions
* `11000–12000`: Person detail GUI, hair colors, skills, skill rankings
* `12000–13000`: Pop-up dialogs & causes of death
* `13000–16000`: Credits, In-Game Menu, Tooltips
* `16000–20000`: Trophy unlock dialogs, Instructions, Settings, Trophies
* `20000–23000`: Flowers GUI, Science GUI, Loading screen tips
* `23000–29000`: Intro story, Crate/Recipes, Clothes GUI, Mysterious Objects, Science Lab, Map GUI
* `30000–35000`: Villager general & event thoughts
* `40000–55000`: Tutorial dialogs, End story, Profile, Exit, Nag dialogs

> **Caveat:** Editing existing `@ID` strings or `!` font styles works immediately without touching the `.exe`. However, adding *additional* entries to indexed pools (such as island names, male/female names, or loading tips) will not be picked up unless the hardcoded count constants inside the compiled game (`ISLAND_NAME_NUM`, `NAME_NUM_MALE`, `NAME_NUM_FEMALE`, `MAX_LOADING_TIPS`) are also updated.

---

## 4. Loose Assets

These files live directly in the game folder and can be replaced directly (with backups managed via `mods\`):

| Path | Format | Notes |
| :--- | :--- | :--- |
| `music\MENU.dat`, `music\GAME1`–`4.dat` | Standard **Ogg Vorbis** | Background music tracks |
| `voice\Intro1`–`4.dat`, `ending.dat`, `mystery1`–`8.dat`, `tutorial0`–`106.dat` | Standard **Ogg Vorbis** | Voice narration (120 files total) |
| `loadingscreen_loading.dat`, `loadingscreen_main.dat`, `loadingscreen_logo_0`–`40.dat` | Standard **JPEG** | Loading screens (`loadingscreen_main.dat` is `800×600`) |
| `icon.dat` | Standard **BMP** | `32×32` 24-bit window icon |
| `splash2.jpg` | Standard **JPEG** | Splash image |
| `data1.dat` | Plain ASCII | 7-byte text (`BigFish`) |

---

## 5. Save & Profile Files

* **Location:** `%LOCALAPPDATA%\Grubby Games\My Tribe Data\`
* **Files:**
  * `MYTRIBEv8.DAT` (`80` bytes — profile header)
  * `PROFILE1v8.DAT` (slot save data, plus `.BAKL` / `.BAKS` backups)
* **Checksum Format:**
  The first 4 bytes (`uint32_le`) are a **32-bit signed-byte sum** over the rest of the file:
  ```python
  import struct
  checksum = sum(struct.unpack(f"<{len(data) - 4}b", data[4:])) & 0xFFFFFFFF
  ```
* **Structure:** Uncompressed sequential C++ struct serialization containing type markers such as `9game_data`, `9tree_data`, `9rock_data`, `11person_data`, `9task_item`, `13building_data`, `7fi_data`, `11flower_data`, `10craft_data`, `7ti_data`, and `14tombstone_data`.

---

## 6. Writing Runtime DLL Proxy Mods

Because `MyTribe.exe` dynamically loads local DLLs from the game folder, a 32-bit x86 proxy DLL is the cleanest way to add runtime features:

### Relevant Imports Used by the Game
* **`MyTribe.exe` imports 38 `SDL.dll` functions:**
  `SDL_CreateRGBSurface`, `SDL_Delay`, `SDL_DisplayFormat`, `SDL_DisplayFormatAlpha`, `SDL_EnableUNICODE`, `SDL_FillRect`, `SDL_Flip`, `SDL_FreeSurface`, `SDL_GetError`, `SDL_GetRGBA`, `SDL_GetTicks`, `SDL_Init`, `SDL_LoadBMP_RW`, `SDL_LockSurface`, `SDL_MapRGB`, `SDL_MapRGBA`, `SDL_PollEvent`, `SDL_PushEvent`, `SDL_Quit`, `SDL_RWFromFile`, `SDL_RWFromMem`, `SDL_SetAlpha`, `SDL_SetClipRect`, `SDL_SetColorKey`, `SDL_SetModuleHandle`, `SDL_SetVideoMode`, `SDL_ShowCursor`, `SDL_UnlockSurface`, `SDL_UpdateRect`, `SDL_UpdateRects`, `SDL_UpperBlit`, `SDL_VideoModeOK`, `SDL_WM_GrabInput`, `SDL_WM_SetCaption`, `SDL_WM_SetIcon`, `SDL_WarpMouse`, `SDL_strlcat`, `SDL_strlcpy`.
* **`SDL_image.dll` imports 7 `SDL.dll` functions** (and exports `IMG_LoadTyped_RW`).
* **`SDL_ttf.dll` imports 7 `SDL.dll` functions** (`TTF_Init`, `TTF_OpenFontRW`, `TTF_RenderUNICODE_Blended`, etc.).
* **`fmodex.dll`** provides 22 imported audio functions (`FMOD_System_*`, `FMOD_Channel_*`, `FMOD_Sound_*`).

### Key Implementation Rules
1. **Architecture & Calling Convention:** Must be **32-bit x86 (`0x014C`)**. SDL functions use `__cdecl`; FMOD Ex C API functions use `__stdcall`.
2. **PE Export Forwarding with MSVC:** Generate a `.def` file mapping unhooked symbols as `Name=SDL_orig.Name @Ordinal`. With modern MSVC toolchains (`14.50+`), run `lib /DEF:proxy.def /MACHINE:X86` to build the `.exp` object first, then pass that `.exp` to `link /DLL` (passing `/DEF:` directly alongside `.obj` files in `link` can fail to recognize DLL forwarders).
3. **Never Call `LoadLibrary` in `DllMain`:** Store `HMODULE` in `DllMain(DLL_PROCESS_ATTACH)` and lazy-load `SDL_orig.dll` on the first hooked API call using `GetModuleFileNameA`.
4. **Environment Variables:** Call `SDL_orig`'s `SDL_putenv` rather than only Win32 `SetEnvironmentVariableA`, because SDL 1.2 checks the MSVCRT environment table via `getenv`.
5. **Ideas for Future Proxy Mods:**
   * **Game-Speed / Fast-Forward Hotkeys:** Hook `SDL_GetTicks` / `SDL_Delay` to scale simulation time cleanly.
   * **Loose Asset Override Loader:** Proxy `SDL_image.dll` (`IMG_LoadTyped_RW`) or `SDL_ttf.dll` (`TTF_OpenFontRW`) to override textures/fonts from a folder without repacking `resource.dat`.
   * **Custom Audio / Volume Hooks:** Proxy `fmodex.dll` for per-channel audio tweaking.

---

## 7. Packaging Mods for `mods/`

The included `mods\mod_manager.py` loads all enabled folders inside `mods\` in alphabetical order (case-insensitive):

```text
mods/
└── my_custom_mod/
    ├── mod.json            (optional metadata & backup instructions)
    ├── strings.dat         (replaces root strings.dat)
    └── music/
        └── MENU.dat        (replaces music/MENU.dat)
```

### `mod.json` Schema
```json
{
  "name": "Human-Readable Mod Name",
  "version": "1.0",
  "description": "Short summary shown by status_mods.bat.",
  "preserve_original_as": {
    "SDL.dll": "SDL_orig.dll"
  },
  "cleanup_files": [
    "mytribe_display.log"
  ]
}
```
* `preserve_original_as`: Copies the listed vanilla file from `ModDevBackups\` (backing it up automatically from root on first install) to an alternate filename in the game root (used by DLL proxies so the original DLL stays accessible).
* `cleanup_files`: Extra runtime logs/caches generated by the mod that should be deleted on `uninstall_mods.bat`.
* **Load Order & Conflicts:** Mods install alphabetically; if two enabled mods touch the same relative path, the later mod wins and `[WARN] Conflict:` is printed.
* **Disabling a Mod:** Move its folder into `mods\_disabled\` or prefix its folder name with `_` or `.`, then re-run `mods\install_mods.bat`.
