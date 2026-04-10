# 新增 STM32H743 开发板支持 Spec

## Why

当前 puppy-next 仅支持 stm32f407（Cortex-M4）构建目标。STM32H743（Cortex-M7）与现有 stm32h750 同属 H7 系列，IPS 驱动（clk/uart/pmctrl）已存在，但缺少芯片级绑定文件和构建目标。需要在现有 xmake.lua 基础上并行添加 M7 内核和 driver 目标，共用 app 目标，最终生成两套固件。

## What Changes

- 新增 `core/dtm/chips/stm32h743/` 芯片目录，包含 `dtm_bindings.h` 和 `start.s`
- 新增 `core/dtm/arch/arm/cortex-m7/` 目录，放置 Cortex-M7 的 dtm_irq.h
- 在根 `xmake.lua` 中新增 `puppy-m7` 内核目标（与 `puppy` 并行，使用 cortex-m7 标志 + stm32h743 芯片目录）
- 在根 `xmake.lua` 中新增 `driver-m7` 用户态驱动目标（与 `driver` 并行）
- 在根 `xmake.lua` 中新增 `firmware-m7` 打包目标（与 `firmware` 并行）
- 在根 `xmake.lua` 中新增 `define_user_app_target_m7` 函数，用于构建 M7 版 app
- App 目标（hello/shell/test/mpu_test/lfs）同时生成 M4 和 M7 两个版本
- `core/default_config.h` 内存布局宏已有 `#ifndef` 保护，M7 目标通过 `add_defines` 覆盖

## Impact

- Affected specs: 构建系统多目标支持
- Affected code:
  - `xmake.lua`（新增 M7 系列目标，不修改现有 M4 目标，**非 BREAKING**）
  - 新增 `core/dtm/chips/stm32h743/` 目录
  - 新增 `core/dtm/arch/arm/cortex-m7/` 目录

## ADDED Requirements

### Requirement: Parallel M7 Kernel Target

系统 SHALL 在 xmake.lua 中提供 `puppy-m7` 目标，与现有 `puppy` 目标并行存在。

`puppy-m7` 与 `puppy` 的差异仅限于：
- 编译标志：`-mcpu=cortex-m7` 替代 `-mcpu=cortex-m4`
- Include 路径：`core/dtm/chips/stm32h743` 替代 `core/dtm/chips/stm32f407`，`core/dtm/arch/arm/cortex-m7` 替代 `core/dtm/arch/arm/cortex-m4`
- 启动文件：`core/dtm/chips/stm32h743/start.s` 替代 `core/dtm/chips/stm32f407/start.s`
- 链接器符号：H743 的 Flash/RAM 地址和大小（DTM_FLASH_BASE=0x08000000, DTM_RAM_BASE=0x24000000, DTM_FLASH_SIZE=2M）
- 内存布局 defines：通过 `add_defines` 覆盖 default_config.h 中的默认值

其余源文件列表、通用 include 路径、内核功能 defines（如 PUP_MPU_ENABLED）均与 `puppy` 相同。

#### Scenario: 构建 M7 内核
- **WHEN** 用户执行 `xmake build puppy-m7`
- **THEN** 产物 `output/kernel/puppy-m7.bin` 生成，使用 cortex-m7 指令集编译

### Requirement: Parallel M7 Driver Target

系统 SHALL 在 xmake.lua 中提供 `driver-m7` 目标，与现有 `driver` 目标并行存在。

`driver-m7` 与 `driver` 的差异仅限于：
- 编译标志：`-mcpu=cortex-m7` 替代 `-mcpu=cortex-m4`
- Include 路径中芯片和架构目录指向 H743/M7 对应目录

### Requirement: Parallel M7 App Targets

系统 SHALL 为每个用户态 app 同时生成 M4 和 M7 两个版本。M7 版 app 的 target 名为 `<app>-m7`（如 `hello-m7`, `shell-m7`），产物输出到 `output/apps/<app>-m7.bin`。

M7 app 与 M4 app 的差异仅限于编译/链接标志（`-mcpu=cortex-m7`），源文件和通用 include 路径完全相同。

### Requirement: Parallel M7 Firmware Target

系统 SHALL 在 xmake.lua 中提供 `firmware-m7` 目标，与现有 `firmware` 目标并行存在。

`firmware-m7` 依赖所有 M7 版内核和 app 目标，打包生成 `output/firmware/firmware-m7.bin`。

#### Scenario: 构建 M7 固件
- **WHEN** 用户执行 `xmake build firmware-m7`
- **THEN** 生成 `output/firmware/firmware-m7.bin`，包含 M7 内核和 M7 版 app

#### Scenario: 同时构建两套固件
- **WHEN** 用户执行 `xmake build firmware firmware-m7`
- **THEN** 同时生成 `output/firmware/firmware.bin`（M4）和 `output/firmware/firmware-m7.bin`（M7）

### Requirement: STM32H743 Chip Support

系统 SHALL 提供 `core/dtm/chips/stm32h743/` 目录，包含：
- `dtm_bindings.h`：IP 描述（CLK/GPIO/UART/PMCTRL/Flash/Pinctrl）、IP 配置（HSE 频率、SYSCLK 目标、Debug UART、LED 引脚）
- `start.s`：Cortex-M7 启动文件，包含向量表和 Reset_Handler

#### Scenario: STM32H743 时钟初始化
- **WHEN** 内核在 stm32h743 上启动
- **THEN** 通过 `DTM_BINFDING_CLK_ARM_M7_ST` 宏选中 `clk_arm_m7_st.c` 驱动，PLL 配置为 HSE→PLL→SYSCLK 路径

#### Scenario: STM32H743 UART 调试输出
- **WHEN** 内核在 stm32h743 上初始化调试串口
- **THEN** 通过 `DTM_BINFDING_UART_ARM_M7_ST` 宏选中 `uart_arm_m7_st.c` 驱动

### Requirement: Cortex-M7 Architecture Support Directory

系统 SHALL 提供 `core/dtm/arch/arm/cortex-m7/dtm_irq.h`，包含 Cortex-M7 的 NVIC 定义。Cortex-M7 的 NVIC 寄存器结构与 Cortex-M4 相同，`__NVIC_PRIO_BITS` 为 4（与 M4 一致），可从 M4 版本复制。

### Requirement: M7 Memory Layout Defines

`puppy-m7` 目标 SHALL 通过 `add_defines` 传入 STM32H743 的内存布局，覆盖 `default_config.h` 的默认值：
- `PUP_KERNEL_RAM_BASE=0x24000000`（AXI SRAM）
- `PUP_KERNEL_RAM_SIZE=0x20000`（128KB）
- `PUP_APP_RAM_BASE=0x24020000`
- `PUP_APP_RAM_SIZE=0x20000`（128KB）
- `PUP_SHARED_RAM_BASE=0x30000000`（SRAM4 用于共享内存）
- `PUP_SHARED_RAM_SIZE=0x10000`（64KB）

## MODIFIED Requirements

无修改。现有 M4 目标和构建命令完全保持不变。

## REMOVED Requirements

无移除项。
