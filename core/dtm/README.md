# DTM

DTM is Puppy Next's board and chip description layer. The current direction is
to keep IP drivers small and move board-specific resource selection into a
device-tree-backed configuration layer.

## Layout

- `common/`: DTM APIs, internal board-resource types and board configuration helpers.
- `ofw/`: DTB parser and board resource extraction.
- `boards/`: DTS files and generated DTB C blobs for selected boards.
- `chips/`: legacy chip fallback macros and startup files.
- `ips/`: register-level IP implementations such as clock, GPIO, UART, SPI,
  delay, flash and MPU.
- `arch/`: architecture-specific DTM headers.

## Boot Flow

1. `dtm_ofw_init_default()` parses the embedded DTB when
   `__dtm_dtb_blob` / `__dtm_dtb_blob_size` are linked.
2. `dtm_board_init()` dispatches the kernel OF driver table through the shared
   `dtm_of_probe_all()` walker.
3. Kernel clock, UART and IRQ drivers match their own `compatible` strings,
   parse their own bindings in `probe(node)`, and publish only the small
   resources they own through `dtm_board_set_*()`.
4. `SYS_DTM_QUERY` exposes the read-only DTB blob address and size to
   user-mode apps.
5. User-mode drivers parse that flash-resident DTB directly, use the same
   `dtm_of_driver_t` / `dtm_of_probe_all()` model, and parse their own
   bindings inside driver-owned probe functions.
6. If no DTB exists, the DTB-backed driver model is unavailable; there is no
   private fallback board state.

## Current DTB Coverage

The STM32F407 board DTS describes:

- console UART from `/chosen/stdout-path`
- RCC register base, clock frequency, PLL parameters, bus dividers and
  peripheral gate clock child nodes
- GPIO devices exposed by the user-mode driver
- key EXTI routing
- SPI2 pinmux, chip select and SPI NOR flash

Peripheral nodes reference explicit `puppy,clock-gate` child nodes through
their `clocks` phandle. The gate node carries the `enum clk_id` value plus
the controller-relative enable register offset and bit, so board/chip migration
does not require editing the clock driver for every peripheral gate.

Memory layout and linker placement are still build-time settings in
top-level `xmake.lua` and `link.lds`; they cannot be made runtime-only because
the kernel and app binaries need those addresses while linking.

## Migration Rule

New board-specific values should be added to DTS first. Kernel and user-mode
drivers should both declare `dtm_of_driver_t` compatible/probe entries. Kernel
drivers consume only kernel-needed resources through the narrow `dtm_board`
facade; user-mode drivers obtain the DTB flash address through `SYS_DTM_QUERY`
and parse driver-owned bindings in user mode. Direct reads from
`dtm_bindings.h` should be treated as legacy-only board constants, not runtime
resource selection.
