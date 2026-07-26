# DTM Device Tree

`core/dtm/ofw` provides a small read-only parser for standard `dtc` generated
DTB files. It is inspired by RT-Thread OFW, but only keeps what Puppy needs
during early MCU boot:

- fixed-size static pools, no dynamic allocation
- node/property traversal
- `compatible`, `status`, aliases and `/chosen/stdout-path`
- `reg` decoding with `#address-cells` / `#size-cells`
- simple phandle lists such as `gpios`, `clocks`, `pinctrl-0` and `cs-gpios`

## Architecture

The kernel owns the embedded DTB blob but does not export a driver-specific
resource snapshot as user ABI. The stable user/kernel interface is deliberately
small:

- `PUP_DTM_QUERY_BLOB_INFO`: returns whether a DTB is present, its size, and
  the read-only flash address that user-mode drivers can parse directly.

Kernel boot sequence:

1. `core/init.c` calls `dtm_ofw_init_default()`.
2. `dtm_board_init()` dispatches a shared `dtm_of_driver_t`
   compatible/probe table over the embedded DTB.
3. Kernel clock, UART and IRQ drivers match their own `compatible` strings and
   parse their own binding properties inside `probe(node)`. Clock drivers read
   RCC/RCU `reg`, HSE/SYSCLK, PLL, bus divider properties and child
   `puppy,clock-gate` nodes. UART drivers read `reg`, `interrupts`,
   `current-speed`, `clocks` and pinctrl from the selected `/chosen/stdout-path`
   node.
4. Kernel DTM/IP code reads those resources through `dtm_board_get_*()`.

User-mode driver sequence:

1. The user-mode `driver_of` layer calls
   `dtm_query(PUP_DTM_QUERY_BLOB_INFO, ...)`.
2. If a readable DTB mapping exists, it passes the returned flash address
   directly to `dtm_ofw_init()`.
3. The shared `dtm_of_probe_all()` walker dispatches available nodes to
   registered `dtm_of_driver_t` compatible/probe entries.
4. Each concrete driver owns its compatible table and reads its own binding
   properties inside `probe(node)`.
5. If no readable DTB mapping exists, the user-mode driver fails instead of
   consulting private board data.

This keeps DTB transport stable while using one OF driver model in both kernel
and user mode. Kernel and user mode still differ in how they obtain the DTB:
the kernel parses the embedded blob directly, while user-mode drivers receive
the same blob's read-only flash address through `SYS_DTM_QUERY`.

## Embedded DTB

The default STM32F407 firmware embeds:

```text
core/dtm/boards/generated/stm32f407-puppy.dtb.c
```

It defines:

- `__dtm_dtb_blob`
- `__dtm_dtb_blob_size`

The M4 `puppy` target includes this file through `core/**/*.c`. The M7 target
explicitly removes `core/dtm/boards/generated/*.c` so it does not accidentally
link an F407 DTB.

Regenerate after editing the DTS:

```powershell
python tools\dtm_build_dtb.py `
  --dtc D:\work\rt-thread\bsp\rockchip\tools\dtc\windows\dtc.exe `
  --dts core\dtm\boards\stm32f407-puppy.dts `
  --dtb output\dtb\stm32f407-puppy.dtb `
  --out-c core\dtm\boards\generated\stm32f407-puppy.dtb.c
```

Run `xmake -r` after regenerating so xmake refreshes its source list.

## Current Resource Types

`core/dtm/common/dtm_resource.h` defines small reusable resource structs used by
DTB parsers:

- console UART base, IRQ, baudrate, clock ID and TX/RX pins
- clock controller base, HSE/SYSCLK, PLL parameters and bus dividers
- key EXTI syscfg/exti base, line, port, IRQ and priority
- user-mode GPIO devices
- SPI buses, pinmux and chip selects
- SPI NOR flash devices

There is intentionally no aggregate "driver config" object. Kernel drivers keep
or publish only the console, clock and key EXTI resources they own. User-mode
GPIO, SPI and flash drivers each keep their own resource tables after their
compatible `probe(node)` parses the DTB in user space.

Important boundary: these structs are not in `core/shared/pup_dtm.h` and are
not syscall ABI. They are local parser output types shared only where a driver
module explicitly chooses to reuse them.

## Clock Bindings

Clock controller nodes use driver-owned Puppy properties:

- `reg`: RCC/RCU register base.
- `hse-frequency`: external oscillator frequency in Hz.
- `clock-frequency`: target SYSCLK frequency in Hz.
- `puppy,pll`: `<m n p q>`.
- `puppy,bus-dividers`: `<hclk pclk1 pclk2>` on M4, and
  `<hclk pclk1 pclk2 pclk3 pclk4>` on M7.

Peripheral gate clocks are child nodes of the controller:

```dts
usart1_clk: usart1-clock {
    compatible = "puppy,clock-gate";
    #clock-cells = <0>;
    puppy,clock-id = <21>;
    puppy,parent-clock = <9>;
    puppy,enable-reg = <0x44>;
    puppy,enable-bit = <4>;
};

usart1: serial@40011000 {
    clocks = <&usart1_clk>;
};
```

`puppy,clock-id` and `puppy,parent-clock` use `enum clk_id` values from
`core/dtm/common/dtm_clk.h`. `puppy,enable-reg` is the enable-register offset
from the clock controller base, and `puppy,enable-bit` is the gate bit index.
Board DTS files should use explicit `puppy,clock-gate` child nodes, so the
clock topology stays in the board description instead of the driver.

## Remaining Work

- Add DTS files for M7 and other chips.
- Add richer clock topology bindings if future chips need more than the current
  PLL and bus divider properties.
- Keep linker memory layout in build config unless a bootloader starts passing
  load-time metadata before linking is reconsidered.
