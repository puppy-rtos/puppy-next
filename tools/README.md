# Tools Directory

This directory contains repository maintenance and runtime helper scripts.

## Packaging

- `merge_firmware.py`: build ROMFS from app binaries in `output/*.bin` (excluding `puppy.bin`, `romfs.bin`, `firmware.bin`) and merge with kernel image into `output/firmware.bin`.

## QEMU Launchers

- `qemu-run.bat`: run firmware with serial console.
- `qemu-test.bat`: run short smoke test and stop automatically.
- `qemu-dbg.bat`: launch QEMU with `-S -s` for debugger attach.

## Build Hygiene

- `prebuild-clean.bat`: remove stale build cache and artifacts (`.xmake`, `build`, `output`) before compiling.

Recommended usage from repository root:

```powershell
./prebuild-clean.bat
xmake f -p cross -a cortex-m4 -m debug
xmake build firmware
```

## Compatibility Entrypoints

Root-level script `qemu-run.bat` is kept as a compatibility entrypoint.
Root-level script `prebuild-clean.bat` is kept as a compatibility entrypoint.
