# Tools Directory

This directory contains repository maintenance and runtime helper scripts.

## Packaging

- `merge_firmware.py`: build ROMFS from `output/*_app.bin` and merge with kernel image into `output/firmware.bin`.

## QEMU Launchers

- `qemu-run.bat`: run firmware with serial console.
- `qemu-test.bat`: run short smoke test and stop automatically.
- `qemu-dbg.bat`: launch QEMU with `-S -s` for debugger attach.

## Compatibility Entrypoints

Root-level script `qemu-run.bat` is kept as a compatibility entrypoint.
