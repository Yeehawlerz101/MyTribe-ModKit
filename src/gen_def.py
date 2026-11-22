import os
import struct
import sys

HOOKED_EXPORTS = {
    "SDL_Init",
    "SDL_InitSubSystem",
    "SDL_Quit",
    "SDL_VideoModeOK",
    "SDL_SetVideoMode",
    "SDL_GetVideoSurface",
    "SDL_FreeSurface",
    "SDL_Flip",
    "SDL_UpdateRect",
    "SDL_UpdateRects",
    "SDL_PollEvent",
    "SDL_WaitEvent",
    "SDL_PushEvent",
    "SDL_WarpMouse",
    "SDL_GetMouseState",
    "SDL_WM_GrabInput",
    "SDL_WM_ToggleFullScreen",
}


def parse_pe_exports(dll_path: str) -> list[tuple[int, str]]:
    with open(dll_path, "rb") as f:
        data = f.read()

    if data[:2] != b"MZ":
        raise RuntimeError(f"{dll_path} is not a valid PE file (missing MZ)")

    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off : pe_off + 4] != b"PE\x00\x00":
        raise RuntimeError(f"{dll_path} is not a valid PE file (missing PE)")

    num_sec = struct.unpack_from("<H", data, pe_off + 6)[0]
    opt_off = pe_off + 24
    opt_sz = struct.unpack_from("<H", data, pe_off + 20)[0]
    sec_off = opt_off + opt_sz

    sections = []
    for i in range(num_sec):
        o = sec_off + i * 40
        vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, o + 8)
        sections.append((vaddr, vsize, rptr, rsize))

    def rva_to_off(rva: int) -> int:
        for vaddr, vsize, rptr, rsize in sections:
            if vaddr <= rva < vaddr + max(vsize, rsize):
                return rptr + (rva - vaddr)
        raise RuntimeError(f"RVA 0x{rva:08X} outside sections")

    exp_rva, exp_sz = struct.unpack_from("<II", data, opt_off + 96)
    if not exp_rva:
        raise RuntimeError(f"{dll_path} has no export table")

    eo = rva_to_off(exp_rva)
    _, _, _, _, base, _num_funcs, num_names, addr_funcs, addr_names, addr_ords = (
        struct.unpack_from("<IIIIIIIIII", data, eo)
    )

    fo_tbl = rva_to_off(addr_funcs)
    no = rva_to_off(addr_names)
    oo = rva_to_off(addr_ords)
    exports = []
    for i in range(num_names):
        nrva = struct.unpack_from("<I", data, no + i * 4)[0]
        ord_idx = struct.unpack_from("<H", data, oo + i * 2)[0]
        frva = struct.unpack_from("<I", data, fo_tbl + ord_idx * 4)[0]
        assert not (exp_rva <= frva < exp_rva + exp_sz), (
            f"Export #{base + ord_idx} in {dll_path} is a forwarder — expected vanilla SDL.dll!"
        )
        fo = rva_to_off(nrva)
        end = data.index(b"\x00", fo)
        name = data[fo:end].decode("latin1")
        exports.append((base + ord_idx, name))

    return sorted(exports, key=lambda x: x[0])


def find_vanilla_sdl(script_dir: str) -> str:
    candidates = [
        os.path.normpath(os.path.join(script_dir, "..", "..", "ModDevBackups", "SDL.dll")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "SDL_orig.dll")),
        os.path.normpath(os.path.join(script_dir, "..", "..", "SDL.dll")),
    ]
    for path in candidates:
        if os.path.isfile(path):
            try:
                parse_pe_exports(path)
                return path
            except AssertionError:
                continue
    raise FileNotFoundError(
        "Could not locate a non-proxied vanilla SDL.dll automatically. "
        "Pass its path explicitly: python gen_def.py <path-to-vanilla-SDL.dll>"
    )


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    orig_dll = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else find_vanilla_sdl(script_dir)
    out_def = os.path.join(script_dir, "sdl_proxy.def")

    exports = parse_pe_exports(orig_dll)
    assert len(exports) == 207, f"Expected 207 exports in {orig_dll}, got {len(exports)}"

    export_names = {name for _, name in exports}
    missing = HOOKED_EXPORTS - export_names
    assert not missing, f"Hooked exports missing from {orig_dll}: {sorted(missing)}"

    lines = ["LIBRARY SDL", "EXPORTS"]
    hooked_count = 0
    fwd_count = 0
    for ordinal, name in exports:
        if name in HOOKED_EXPORTS:
            lines.append(f"    {name} @{ordinal}")
            hooked_count += 1
        else:
            lines.append(f"    {name}=SDL_orig.{name} @{ordinal}")
            fwd_count += 1

    with open(out_def, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    print(
        f"Generated {out_def} from {orig_dll}: {len(exports)} total exports "
        f"({hooked_count} hooked, {fwd_count} forwarded to SDL_orig.dll)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
