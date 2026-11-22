import argparse
import json
import os
import shutil
import struct
import sys
from pathlib import Path

MODS_DIR = Path(__file__).resolve().parent
GAME_ROOT = MODS_DIR.parent
BACKUP_DIR = GAME_ROOT / "ModDevBackups"
MANIFEST_PATH = MODS_DIR / ".installed_manifest.json"

IGNORED_TOP_FILES = {"mod.json", "readme.txt", "readme.md"}
IGNORED_ANY_FILES = {".ds_store", "thumbs.db", ".gitkeep"}
PROTECTED_TOP_DIRS = {"moddevbackups", "mods", "mytribe-modkit"}


def safe_rel_path(rel_str: str) -> str:
    """Normalize and validate a relative target path inside GAME_ROOT."""
    if not rel_str or not isinstance(rel_str, str):
        raise ValueError(f"Empty or invalid relative path: {rel_str!r}")
    norm = rel_str.replace("\\", "/").strip("/")
    p = Path(norm)
    if p.is_absolute() or ":" in norm:
        raise ValueError(f"Absolute or drive-qualified path not allowed: {rel_str!r}")
    parts = [part for part in p.parts if part not in ("", ".")]
    if not parts or any(part == ".." for part in parts):
        raise ValueError(f"Path traversal not allowed: {rel_str!r}")
    if parts[0].lower() in PROTECTED_TOP_DIRS:
        raise ValueError(f"Target path cannot write into protected folder {parts[0]!r}: {rel_str!r}")
    resolved = (GAME_ROOT.joinpath(*parts)).resolve()
    root_resolved = GAME_ROOT.resolve()
    try:
        resolved.relative_to(root_resolved)
    except ValueError as exc:
        raise ValueError(f"Target path escapes game root: {rel_str!r}") from exc
    return "/".join(parts)


def is_proxy_sdl_dll(path: Path) -> bool:
    """Return True if the PE DLL at `path` forwards any export to SDL_orig."""
    try:
        data = path.read_bytes()
        if len(data) < 0x100 or data[:2] != b"MZ":
            return False
        pe_off = struct.unpack_from("<I", data, 0x3C)[0]
        if pe_off + 24 > len(data) or data[pe_off : pe_off + 4] != b"PE\x00\x00":
            return False
        num_sec = struct.unpack_from("<H", data, pe_off + 6)[0]
        opt_off = pe_off + 24
        opt_sz = struct.unpack_from("<H", data, pe_off + 20)[0]
        sec_off = opt_off + opt_sz

        sections = []
        for i in range(num_sec):
            o = sec_off + i * 40
            if o + 40 > len(data):
                return False
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", data, o + 8)
            sections.append((vaddr, vsize, rptr, rsize))

        def rva_to_off(rva: int) -> int | None:
            for vaddr, vsize, rptr, rsize in sections:
                if vaddr <= rva < vaddr + max(vsize, rsize):
                    return rptr + (rva - vaddr)
            return None

        exp_rva, exp_sz = struct.unpack_from("<II", data, opt_off + 96)
        if not exp_rva or not exp_sz:
            return False
        eo = rva_to_off(exp_rva)
        if eo is None or eo + 40 > len(data):
            return False

        _, _, _, _, _base, _num_funcs, num_names, addr_funcs, _addr_names, addr_ords = (
            struct.unpack_from("<IIIIIIIIII", data, eo)
        )
        fo_tbl = rva_to_off(addr_funcs)
        oo = rva_to_off(addr_ords)
        if fo_tbl is None or oo is None:
            return False

        for i in range(num_names):
            ord_idx = struct.unpack_from("<H", data, oo + i * 2)[0]
            frva = struct.unpack_from("<I", data, fo_tbl + ord_idx * 4)[0]
            if exp_rva <= frva < exp_rva + exp_sz:
                fo = rva_to_off(frva)
                if fo is not None:
                    end = data.find(b"\x00", fo)
                    if end != -1 and data[fo:end].lower().startswith(b"sdl_orig."):
                        return True
        return False
    except Exception:
        return False


def load_mod_meta(mod_dir: Path) -> dict:
    meta_path = mod_dir / "mod.json"
    if not meta_path.is_file():
        return {}
    try:
        with meta_path.open("r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception as exc:
        raise RuntimeError(f"Failed to parse {meta_path}: {exc}") from exc


def discover_mods() -> tuple[list[tuple[str, Path, dict]], list[tuple[str, Path, dict]]]:
    enabled: list[tuple[str, Path, dict]] = []
    disabled: list[tuple[str, Path, dict]] = []

    if not MODS_DIR.is_dir():
        return enabled, disabled

    for entry in sorted(MODS_DIR.iterdir(), key=lambda p: p.name.lower()):
        if not entry.is_dir():
            continue
        name = entry.name
        if name == "_disabled":
            for sub in sorted(entry.iterdir(), key=lambda p: p.name.lower()):
                if sub.is_dir():
                    disabled.append((sub.name, sub, load_mod_meta(sub)))
        elif name.startswith("_") or name.startswith("."):
            if name != ".git":
                disabled.append((name, entry, load_mod_meta(entry)))
        else:
            enabled.append((name, entry, load_mod_meta(entry)))

    return enabled, disabled


def iter_payload_files(mod_dir: Path) -> list[str]:
    files: list[str] = []
    for root, dirs, filenames in os.walk(mod_dir):
        dirs.sort(key=str.lower)
        rel_root = Path(root).relative_to(mod_dir)
        is_top = (rel_root == Path("."))
        for fn in sorted(filenames, key=str.lower):
            fn_low = fn.lower()
            if fn_low in IGNORED_ANY_FILES:
                continue
            if is_top and fn_low in IGNORED_TOP_FILES:
                continue
            rel = (rel_root / fn) if not is_top else Path(fn)
            files.append(safe_rel_path(rel.as_posix()))
    return files


def cleanup_empty_parents(rel_path: str) -> None:
    rel = Path(rel_path)
    parent = (GAME_ROOT / rel).parent
    root_resolved = GAME_ROOT.resolve()
    while parent.resolve() != root_resolved:
        if parent.name.lower() in PROTECTED_TOP_DIRS:
            break
        try:
            parent.rmdir()
        except OSError:
            break
        parent = parent.parent


def cmd_uninstall() -> int:
    if not MANIFEST_PATH.is_file():
        print("No mods are currently recorded as installed.")
        return 0

    with MANIFEST_PATH.open("r", encoding="utf-8") as f:
        manifest = json.load(f)

    installed_mods = manifest.get("installed_mods", [])
    replaced_files = manifest.get("replaced_files", [])
    added_files = manifest.get("added_files", [])
    cleanup_files = manifest.get("cleanup_files", [])

    if replaced_files and not BACKUP_DIR.is_dir():
        print(f"ERROR: Backup directory missing: {BACKUP_DIR}", file=sys.stderr)
        return 1

    # Verify all vanilla backups exist before modifying root
    for rel in replaced_files:
        safe_rel = safe_rel_path(rel)
        bak_file = BACKUP_DIR / safe_rel
        if not bak_file.is_file():
            print(f"ERROR: Missing vanilla backup file: {bak_file}", file=sys.stderr)
            return 1

    restored_count = 0
    for rel in replaced_files:
        safe_rel = safe_rel_path(rel)
        src = BACKUP_DIR / safe_rel
        dst = GAME_ROOT / safe_rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        restored_count += 1

    removed_count = 0
    seen_remove = set()
    for rel in list(added_files) + list(cleanup_files):
        safe_rel = safe_rel_path(rel)
        key = safe_rel.lower()
        if key in seen_remove:
            continue
        seen_remove.add(key)
        target = GAME_ROOT / safe_rel
        if target.is_file():
            target.unlink()
            cleanup_empty_parents(safe_rel)
            removed_count += 1

    MANIFEST_PATH.unlink()

    print(
        f"Uninstalled {len(installed_mods)} mod(s) ({', '.join(installed_mods) or 'none'}): "
        f"restored {restored_count} vanilla file(s), removed {removed_count} mod/runtime file(s)."
    )
    return 0


def cmd_install() -> int:
    if MANIFEST_PATH.is_file():
        print("Existing installation detected; restoring vanilla state before reinstalling...")
        rc = cmd_uninstall()
        if rc != 0:
            return rc

    enabled, disabled = discover_mods()
    if not enabled:
        print("No enabled mods found in mods\\. Game remains vanilla.")
        return 0

    BACKUP_DIR.mkdir(exist_ok=True)

    # Pre-flight validation across all enabled mods before writing anything
    plan_steps = []
    files_to_backup: dict[str, str] = {}
    tracked_added_low: set[str] = set()
    tracked_replaced_low: set[str] = set()
    file_owners: dict[str, str] = {}

    def check_vanilla_backup(rel: str, mod_name: str, must_exist: bool) -> bool:
        rel_low = rel.lower()
        bak_file = BACKUP_DIR / rel
        root_file = GAME_ROOT / rel
        if bak_file.is_file():
            if rel_low == "sdl.dll" and is_proxy_sdl_dll(bak_file):
                print(
                    f"ERROR: [{mod_name}] Backup ModDevBackups\\SDL.dll is a proxy DLL, not vanilla. "
                    "Verify game files in Steam to restore original SDL.dll.",
                    file=sys.stderr,
                )
                return False
            return True

        if root_file.is_file():
            if rel_low == "sdl.dll" and is_proxy_sdl_dll(root_file):
                print(
                    f"ERROR: [{mod_name}] Root SDL.dll is already a proxy DLL and no vanilla backup exists in "
                    "ModDevBackups\\SDL.dll — verify game files in Steam to restore original SDL.dll.",
                    file=sys.stderr,
                )
                return False
            files_to_backup[rel_low] = rel
            return True

        if must_exist:
            print(
                f"ERROR: [{mod_name}] Vanilla file to preserve not found in ModDevBackups or game root: {rel}",
                file=sys.stderr,
            )
            return False
        return True

    for mod_name, mod_dir, meta in enabled:
        preserve_map = meta.get("preserve_original_as", {})
        if preserve_map and not isinstance(preserve_map, dict):
            print(f"ERROR: Invalid preserve_original_as in {mod_name}/mod.json", file=sys.stderr)
            return 1

        preserves: list[tuple[str, str]] = []
        for orig_rel_raw, alias_rel_raw in preserve_map.items():
            orig_rel = safe_rel_path(orig_rel_raw)
            alias_rel = safe_rel_path(alias_rel_raw)
            if not check_vanilla_backup(orig_rel, mod_name, must_exist=True):
                return 1

            alias_low = alias_rel.lower()
            if (BACKUP_DIR / alias_rel).is_file() or alias_low in files_to_backup:
                tracked_replaced_low.add(alias_low)
            else:
                if (GAME_ROOT / alias_rel).exists() and alias_low not in tracked_added_low and alias_low not in tracked_replaced_low:
                    print(
                        f"ERROR: [{mod_name}] Refusing to overwrite untracked file not present in ModDevBackups: {alias_rel}",
                        file=sys.stderr,
                    )
                    return 1
                tracked_added_low.add(alias_low)
            preserves.append((orig_rel, alias_rel))

        payload = iter_payload_files(mod_dir)
        for rel in payload:
            rel_low = rel.lower()
            if rel_low not in tracked_added_low and rel_low not in tracked_replaced_low:
                if not check_vanilla_backup(rel, mod_name, must_exist=False):
                    return 1
            if (BACKUP_DIR / rel).is_file() or rel_low in files_to_backup:
                tracked_replaced_low.add(rel_low)
            else:
                tracked_added_low.add(rel_low)

        raw_cleanup = meta.get("cleanup_files", [])
        if raw_cleanup and not isinstance(raw_cleanup, list):
            print(f"ERROR: Invalid cleanup_files in {mod_name}/mod.json", file=sys.stderr)
            return 1
        cleanups = [safe_rel_path(c) for c in raw_cleanup]

        plan_steps.append((mod_name, mod_dir, meta, preserves, payload, cleanups))

    # Automatically back up any vanilla files before overwriting them
    for rel in sorted(files_to_backup.values(), key=str.lower):
        src = GAME_ROOT / rel
        dst = BACKUP_DIR / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"[BACKUP] Saved vanilla {rel} -> ModDevBackups\\{rel}")

    # Execute installation
    replaced_files: dict[str, str] = {}
    added_files: dict[str, str] = {}
    cleanup_files: dict[str, str] = {}
    installed_mods: list[str] = []

    for mod_name, mod_dir, _meta, preserves, payload, cleanups in plan_steps:
        installed_mods.append(mod_name)

        for orig_rel, alias_rel in preserves:
            src = BACKUP_DIR / orig_rel
            dst = GAME_ROOT / alias_rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            alias_low = alias_rel.lower()
            if (BACKUP_DIR / alias_rel).is_file():
                replaced_files[alias_low] = alias_rel
            else:
                added_files[alias_low] = alias_rel

        for rel in payload:
            rel_low = rel.lower()
            if rel_low in file_owners:
                print(f"[WARN] Conflict: {rel} overridden by {mod_name} (was {file_owners[rel_low]})")
            file_owners[rel_low] = mod_name

            src = mod_dir / rel
            dst = GAME_ROOT / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

            if (BACKUP_DIR / rel).is_file():
                replaced_files[rel_low] = rel
            else:
                added_files[rel_low] = rel

        for cl in cleanups:
            cleanup_files[cl.lower()] = cl

    manifest = {
        "installed_mods": installed_mods,
        "replaced_files": sorted(replaced_files.values(), key=str.lower),
        "added_files": sorted(added_files.values(), key=str.lower),
        "cleanup_files": sorted(cleanup_files.values(), key=str.lower),
    }
    with MANIFEST_PATH.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
        f.write("\n")

    print(
        f"Installed {len(installed_mods)} mod(s): {', '.join(installed_mods)} "
        f"(replaced {len(replaced_files)} file(s), added {len(added_files)} file(s))."
    )
    if disabled:
        print(f"Skipped {len(disabled)} disabled mod(s): {', '.join(d[0] for d in disabled)}")
    return 0


def cmd_status() -> int:
    enabled, disabled = discover_mods()

    print("=== My Tribe Mod Status ===")
    if MANIFEST_PATH.is_file():
        with MANIFEST_PATH.open("r", encoding="utf-8") as f:
            m = json.load(f)
        print(f"Active State : MODDED ({len(m.get('installed_mods', []))} installed)")
        print(f"Installed    : {', '.join(m.get('installed_mods', [])) or 'none'}")
        print(f"Replaced     : {', '.join(m.get('replaced_files', [])) or 'none'}")
        print(f"Added        : {', '.join(m.get('added_files', [])) or 'none'}")
    else:
        print("Active State : VANILLA (no mods currently installed)")

    print(f"\nEnabled Mods ({len(enabled)}):")
    if not enabled:
        print("  (none)")
    for name, mod_dir, meta in enabled:
        title = meta.get("name", name)
        ver = meta.get("version", "")
        desc = meta.get("description", "")
        payload = iter_payload_files(mod_dir)
        ver_str = f" v{ver}" if ver else ""
        print(f"  - [{name}] {title}{ver_str} ({len(payload)} file(s))")
        if desc:
            print(f"    {desc}")

    print(f"\nDisabled Mods ({len(disabled)}):")
    if not disabled:
        print("  (none)")
    for name, _mod_dir, meta in disabled:
        title = meta.get("name", name)
        print(f"  - [{name}] {title}")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="My Tribe Mod Manager")
    parser.add_argument("action", choices=["install", "uninstall", "status"])
    args = parser.parse_args()

    try:
        if args.action == "install":
            return cmd_install()
        if args.action == "uninstall":
            return cmd_uninstall()
        return cmd_status()
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
