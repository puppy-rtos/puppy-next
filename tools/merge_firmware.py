#!/usr/bin/env python3
"""
Firmware packager for Puppy RTOS.

This script merges kernel and ROMFS images into a single flashable binary.
Default layout:
  - kernel @ 0x08000000
  - romfs  @ 0x08080000
"""

import argparse
import glob
import os
import re
import struct
import sys

ROMFS_MAGIC = 0x524F4D46
ROMFS_MAX_FILES = 16
ROMFS_MAX_NAME_LEN = 32


def load_default_layout():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    config_path = os.path.join(repo_root, "core", "default_config.h")

    with open(config_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    defines = {}
    for name, value in re.findall(r"^\s*#define\s+([A-Z0-9_]+)\s+([^\r\n]+)", content, re.MULTILINE):
        defines[name] = value.strip()

    cache = {}

    def eval_define(name):
        if name in cache:
            return cache[name]
        if name not in defines:
            raise ValueError(f"missing define: {name}")

        expr = defines[name]
        expr = re.sub(r"\b([0-9A-Fa-fx]+)(UL|U|L)+\b", r"\1", expr)

        def repl(match):
            sym = match.group(1)
            if sym in defines:
                return str(eval_define(sym))
            return sym

        expr = re.sub(r"\b([A-Z][A-Z0-9_]*)\b", repl, expr)
        value = int(eval(expr, {"__builtins__": None}, {}))
        cache[name] = value
        return value

    return {
        "kernel_flash_base": eval_define("PUP_KERNEL_FLASH_BASE"),
        "romfs_flash_base": eval_define("PUP_ROMFS_FLASH_BASE"),
        "kernel_max_size": eval_define("PUP_KERNEL_FLASH_SIZE"),
        "romfs_max_size": eval_define("PUP_ROMFS_FLASH_SIZE"),
    }


LAYOUT = load_default_layout()
KERNEL_FLASH_BASE = LAYOUT["kernel_flash_base"]
ROMFS_FLASH_BASE = LAYOUT["romfs_flash_base"]
KERNEL_MAX_SIZE = LAYOUT["kernel_max_size"]
ROMFS_MAX_SIZE = LAYOUT["romfs_max_size"]


def read_binary(filepath):
    with open(filepath, "rb") as f:
        return f.read()


def write_binary(filepath, data):
    out_dir = os.path.dirname(os.path.abspath(filepath))
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir)
    with open(filepath, "wb") as f:
        f.write(data)

def create_romfs(apps_dir, output_file):
    apps = []
    excluded_bins = {"puppy.bin", "romfs.bin", "firmware.bin"}
    bin_files = [
        p for p in glob.glob(os.path.join(apps_dir, "*.bin"))
        if os.path.basename(p) not in excluded_bins
    ]

    if not bin_files:
        print(f"No application binaries found in {apps_dir}")
        print("Looking for app .bin files (excluding puppy.bin, romfs.bin, firmware.bin)")
        return None

    for bin_file in sorted(bin_files):
        name = os.path.basename(bin_file).replace(".bin", "")
        if len(name) > ROMFS_MAX_NAME_LEN - 1:
            name = name[: ROMFS_MAX_NAME_LEN - 1]

        data = read_binary(bin_file)
        apps.append({"name": name, "data": data, "size": len(data)})
        print(f"  {name}: {len(data)} bytes")

    if len(apps) > ROMFS_MAX_FILES:
        print(f"Warning: Too many apps ({len(apps)}), limiting to {ROMFS_MAX_FILES}")
        apps = apps[:ROMFS_MAX_FILES]

    header_size = 16
    file_header_size = 4 + 4 + ROMFS_MAX_NAME_LEN
    total_header_size = header_size + file_header_size * len(apps)
    data_offset = total_header_size
    total_data_size = sum(app["size"] for app in apps)

    romfs = bytearray()
    romfs += struct.pack("<I", ROMFS_MAGIC)
    romfs += struct.pack("<I", len(apps))
    romfs += struct.pack("<I", total_data_size)
    romfs += struct.pack("<I", data_offset)

    current_data_offset = 0
    for app in apps:
        romfs += struct.pack("<I", app["size"])
        romfs += struct.pack("<I", current_data_offset)

        name_bytes = app["name"].encode("utf-8")
        name_bytes = name_bytes[: ROMFS_MAX_NAME_LEN - 1]
        name_bytes += b"\x00" * (ROMFS_MAX_NAME_LEN - len(name_bytes))
        romfs += name_bytes

        current_data_offset += app["size"]

    for app in apps:
        romfs += app["data"]

    print("\nROMFS created:")
    print(f"  Files: {len(apps)}")
    print(f"  Total size: {len(romfs)} bytes ({len(romfs) / 1024:.2f} KB)")
    print(f"  Header size: {total_header_size} bytes")
    print(f"  Data size: {total_data_size} bytes")
    print(f"  Data offset: {data_offset}")

    write_binary(output_file, romfs)
    print(f"  Output: {output_file}")

    return romfs


def ensure_romfs_image(apps_dir, romfs_bin):
    print(f"Creating ROMFS image from: {apps_dir}")
    romfs = create_romfs(apps_dir, romfs_bin)
    if romfs is None:
        raise RuntimeError("failed to create ROMFS image")


def merge_kernel_and_romfs(kernel_bin, romfs_bin, output_bin):
    print(f"Reading kernel: {kernel_bin}")
    kernel_data = read_binary(kernel_bin)
    print(f"  Size: {len(kernel_data)} bytes ({len(kernel_data) / 1024:.2f} KB)")

    if len(kernel_data) > KERNEL_MAX_SIZE:
        print(f"  WARNING: kernel exceeds max partition ({KERNEL_MAX_SIZE} bytes)")

    print(f"\nReading ROMFS: {romfs_bin}")
    romfs_data = read_binary(romfs_bin)
    print(f"  Size: {len(romfs_data)} bytes ({len(romfs_data) / 1024:.2f} KB)")

    if len(romfs_data) > ROMFS_MAX_SIZE:
        print(f"  WARNING: ROMFS exceeds max partition ({ROMFS_MAX_SIZE} bytes)")

    romfs_offset = ROMFS_FLASH_BASE - KERNEL_FLASH_BASE
    print(f"\nROMFS offset: 0x{romfs_offset:X} ({romfs_offset} bytes)")

    total_size = romfs_offset + len(romfs_data)
    firmware = bytearray([0xFF] * total_size)

    firmware[: len(kernel_data)] = kernel_data
    firmware[romfs_offset : romfs_offset + len(romfs_data)] = romfs_data

    print(f"\nMerged firmware size: {len(firmware)} bytes ({len(firmware) / 1024:.2f} KB)")
    print(f"Writing output: {output_bin}")
    write_binary(output_bin, firmware)

    print("\nMemory map:")
    print(f"  0x{KERNEL_FLASH_BASE:08X} - Kernel start")
    print(f"  0x{KERNEL_FLASH_BASE + len(kernel_data):08X} - Kernel end")
    print(f"  0x{ROMFS_FLASH_BASE:08X} - ROMFS start")
    print(f"  0x{ROMFS_FLASH_BASE + len(romfs_data):08X} - ROMFS end")


def parse_args(argv):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)
    default_output_dir = os.path.join(repo_root, "output")

    parser = argparse.ArgumentParser(
        description="Merge Puppy kernel and ROMFS into one firmware image"
    )
    parser.add_argument(
        "--kernel",
        default=os.path.join(default_output_dir, "puppy.bin"),
        help="kernel binary path (default: output/puppy.bin)",
    )
    parser.add_argument(
        "--romfs",
        default=os.path.join(default_output_dir, "romfs.bin"),
        help="ROMFS binary path (default: output/romfs.bin)",
    )
    parser.add_argument(
        "--output",
        default=os.path.join(default_output_dir, "firmware.bin"),
        help="output firmware path (default: output/firmware.bin)",
    )
    parser.add_argument(
        "--apps-dir",
        default=None,
        help="optional apps bin directory; when set, regenerate ROMFS before merge",
    )
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv if argv is not None else sys.argv[1:])

    print("=" * 56)
    print("Puppy RTOS Kernel + ROMFS Firmware Packager")
    print("=" * 56)

    if args.apps_dir:
        ensure_romfs_image(args.apps_dir, args.romfs)

    if not os.path.exists(args.kernel):
        print(f"Error: kernel binary not found: {args.kernel}")
        return 1

    if not os.path.exists(args.romfs):
        print(f"Error: ROMFS binary not found: {args.romfs}")
        print("Tip: pass --apps-dir output to generate ROMFS first.")
        return 1

    merge_kernel_and_romfs(args.kernel, args.romfs, args.output)
    print("\nDone!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
