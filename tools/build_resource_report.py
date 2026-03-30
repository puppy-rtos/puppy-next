#!/usr/bin/env python3
"""
Post-build resource report for Puppy RTOS.

Prints firmware size and ROM/RAM allocation summary based on:
- output binaries (puppy.bin, romfs.bin, firmware.bin)
- linker map (build/puppy.map)
"""

import argparse
import os
import re


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
        "kernel_rom_partition_size": eval_define("PUP_KERNEL_FLASH_SIZE"),
        "romfs_partition_size": eval_define("PUP_ROMFS_FLASH_SIZE"),
        "kernel_ram_size": eval_define("PUP_KERNEL_RAM_SIZE"),
    }


LAYOUT = load_default_layout()
KERNEL_ROM_PARTITION_SIZE = LAYOUT["kernel_rom_partition_size"]
ROMFS_PARTITION_SIZE = LAYOUT["romfs_partition_size"]
KERNEL_RAM_SIZE = LAYOUT["kernel_ram_size"]


def safe_size(path):
    if os.path.isfile(path):
        return os.path.getsize(path)
    return 0


def to_kb(v):
    return f"{v / 1024.0:.2f}"


def pct(used, total):
    if total <= 0:
        return "0.00"
    return f"{used * 100.0 / total:.2f}"


def parse_map_usage(map_path):
    text_size = 0
    exidx_size = 0
    data_size = 0
    bss_size = 0

    if not os.path.isfile(map_path):
        return text_size, exidx_size, data_size, bss_size

    text_re = re.compile(r"^\.text\s+0x08[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)")
    exidx_re = re.compile(r"^\.ARM\.exidx\s+0x08[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)")
    data_re = re.compile(r"^\.data\s+0x20[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)")
    bss_re = re.compile(r"^\.bss\s+0x20[0-9a-fA-F]+\s+0x([0-9a-fA-F]+)")

    in_linker_map = False

    with open(map_path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()

            if not in_linker_map:
                if line.startswith("Linker script and memory map"):
                    in_linker_map = True
                continue

            m = text_re.match(line)
            if m:
                text_size = max(text_size, int(m.group(1), 16))
                continue

            m = exidx_re.match(line)
            if m:
                exidx_size = max(exidx_size, int(m.group(1), 16))
                continue

            m = data_re.match(line)
            if m:
                data_size = max(data_size, int(m.group(1), 16))
                continue

            m = bss_re.match(line)
            if m:
                bss_size = max(bss_size, int(m.group(1), 16))
                continue

    return text_size, exidx_size, data_size, bss_size


def main():
    parser = argparse.ArgumentParser(description="Print build resource allocation report")
    parser.add_argument("--mode", default="release", help="build mode (debug/release)")
    args = parser.parse_args()

    puppy_bin = os.path.join("output", "kernel", "puppy.bin")
    romfs_bin = os.path.join("output", "firmware", "romfs.bin")
    firmware_bin = os.path.join("output", "firmware", "firmware.bin")
    map_file = os.path.join("build", "puppy.map")

    puppy_size = safe_size(puppy_bin)
    romfs_size = safe_size(romfs_bin)
    firmware_size = safe_size(firmware_bin)

    text_size, exidx_size, data_size, bss_size = parse_map_usage(map_file)
    rom_used = text_size + exidx_size + data_size
    ram_used = data_size + bss_size

    print("\n================ Build Resource Report ================")
    print(f"Mode: {args.mode}")
    print(f"Firmware: {firmware_size} bytes ({to_kb(firmware_size)} KB)")

    print("\n[ROM Allocation]")
    print(
        f"Kernel partition: {puppy_size} / {KERNEL_ROM_PARTITION_SIZE} bytes ({pct(puppy_size, KERNEL_ROM_PARTITION_SIZE)}%)"
    )
    print(
        f"ROMFS partition : {romfs_size} / {ROMFS_PARTITION_SIZE} bytes ({pct(romfs_size, ROMFS_PARTITION_SIZE)}%)"
    )
    print(
        f"Kernel ELF ROM  : {rom_used} / {KERNEL_ROM_PARTITION_SIZE} bytes ({pct(rom_used, KERNEL_ROM_PARTITION_SIZE)}%)"
    )
    print(f"  text={text_size}, exidx={exidx_size}, data(load)={data_size}")

    print("\n[RAM Allocation]")
    print(f"Kernel ELF RAM  : {ram_used} / {KERNEL_RAM_SIZE} bytes ({pct(ram_used, KERNEL_RAM_SIZE)}%)")
    print(f"  data={data_size} bytes, bss={bss_size} bytes")
    print("=======================================================\n")


if __name__ == "__main__":
    main()
