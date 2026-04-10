# DTM 架构统一优化 Spec

## Why

当前项目中存在两份完全相同的 DTM 代码：
- `core/dtm` - 内核态使用
- `apps/apps/driver/dtm` - 用户态 driver app 使用

这种重复导致：
1. 维护成本增加（修改需要同步两处）
2. 代码冗余，增加仓库体积
3. 容易出现不一致问题

## What Changes

- 删除 `apps/apps/driver/dtm` 目录
- 修改 xmake.lua，让 driver app 直接使用 `core/dtm` 的代码
- 将 `DTM_NO_SPI_FLASH` 等差异化配置移至 xmake.lua 的 defines 中

## Impact

- Affected specs: 无
- Affected code:
  - `apps/apps/driver/dtm/` (删除)
  - `xmake.lua` (修改 driver 目标的 include 路径和 defines)

## ADDED Requirements

### Requirement: DTM 代码统一

系统 SHALL 只维护一份 DTM 代码，位于 `core/dtm` 目录。

#### Scenario: 内核目标使用 DTM
- **WHEN** 构建内核目标 (puppy, puppy-m7)
- **THEN** 使用 `core/dtm` 目录的代码

#### Scenario: 用户态 app 使用 DTM
- **WHEN** 构建用户态 app (driver, driver-m7)
- **THEN** 使用 `core/dtm` 目录的代码
- **AND** 通过 xmake defines 配置差异化宏

## MODIFIED Requirements

无

## REMOVED Requirements

无

## 架构方案

### 方案概述

保留 `core/dtm` 作为唯一的 DTM 代码源，删除 `apps/apps/driver/dtm`。

### 目录结构（优化后）

```
core/dtm/
├── arch/arm/
│   ├── cortex-m4/
│   │   └── dtm_irq.h
│   └── cortex-m7/
│       └── dtm_irq.h
├── chips/
│   ├── stm32f407/
│   │   ├── dtm_bindings.h
│   │   └── start.s
│   ├── stm32h743/
│   │   ├── dtm_bindings.h
│   │   └── start.s
│   └── ...
├── common/
│   ├── dtm_gpio.h
│   ├── dtm_uart.h
│   └── ...
├── ips/
│   ├── gpio_arm_m4_st_gd.c
│   ├── clk_arm_m7_st.c
│   └── ...
└── svd/
    └── ...
```

### xmake.lua 配置示例

```lua
-- driver 目标使用 core/dtm
define_user_app_target("driver", {
    sources = {
        "apps/apps/driver/main.c",
        "apps/apps/driver/driver_gpio.c",
        "core/dtm/ips/gpio_arm_m4_st_gd.c",
        "core/dtm/ips/clk_arm_m4_st_gd.c",
        "core/dtm/ips/spi_arm_m4_st_gd.c"
    },
    includes = {
        "apps/apps/driver",
        "core/dtm",
        "core/dtm/common",
        "core/dtm/ips",
        "core/dtm/arch/arm/cortex-m4",
        "core/dtm/chips/stm32f407",
        "core/shared"
    },
    defines = {
        -- 差异化配置通过 defines 传递
    },
    output = "driver.bin",
    stack_size = "0x800"
})

-- driver-m7 目标
define_user_app_target_m7("driver-m7", {
    sources = {
        "apps/apps/driver/main.c",
        "apps/apps/driver/driver_gpio.c",
        "core/dtm/ips/gpio_arm_m4_st_gd.c",
        "core/dtm/ips/clk_arm_m7_st.c"
    },
    includes = {
        "apps/apps/driver",
        "core/dtm",
        "core/dtm/common",
        "core/dtm/ips",
        "core/dtm/arch/arm/cortex-m7",
        "core/dtm/chips/stm32h743",
        "core/shared"
    },
    defines = {
        "DTM_NO_SPI_FLASH=1"  -- M7 构建禁用 SPI/Flash
    },
    output = "driver.bin",
    stack_size = "0x800"
})
```

### 差异化配置处理

原 `apps/apps/driver/dtm/chips/stm32h743/dtm_bindings.h` 中的 `DTM_NO_SPI_FLASH` 宏：
- 移至 xmake.lua 的 `defines` 中
- 或在 `core/dtm/chips/stm32h743/dtm_bindings.h` 中通过条件编译判断

推荐方案：在 xmake.lua 中通过 defines 传递，保持 dtm_bindings.h 的纯净。
