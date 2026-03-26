# Driver App重构与DTM集成 Spec

## Why
当前driver app直接操作硬件寄存器，代码重复且难以维护。将dtm整体复制到driver app目录下，可以让驱动开发基于统一的DTM（Device Tree Manager）抽象层，提高代码复用性和可维护性。同时优化xmake构建接口，支持多源文件和头文件配置。

## What Changes
- 将`dtm/`目录整体复制到`apps/driver/dtm/`下
- 重构driver app使用DTM接口替代直接寄存器操作
- 优化`define_user_app_target`函数，支持多源文件和多包含目录
- 更新xmake.lua中driver app的构建配置

## Impact
- Affected specs: driver app构建系统
- Affected code: 
  - `apps/driver/` 目录结构
  - `xmake.lua` 构建配置
  - `apps/driver/main.c` 驱动实现

## ADDED Requirements

### Requirement: DTM集成到Driver App
Driver app SHALL包含完整的DTM代码副本，使驱动可以基于DTM抽象层开发。

#### Scenario: DTM目录结构
- **WHEN** 构建driver app时
- **THEN** `apps/driver/dtm/`目录包含所有DTM源码和头文件

### Requirement: 多源文件构建支持
`define_user_app_target`函数SHALL支持配置多个源文件和多个包含目录。

#### Scenario: 多源文件配置
- **WHEN** 调用`define_user_app_target`时
- **THEN** 可以传入源文件列表和包含目录列表

### Requirement: 回归验证
重构后SHALL通过debug和release两轮构建验证。

#### Scenario: Debug模式验证
- **WHEN** 执行debug模式构建和测试
- **THEN** 所有MPU测试和driver测试通过

#### Scenario: Release模式验证
- **WHEN** 执行release模式构建和测试
- **THEN** 所有MPU测试和driver测试通过

## MODIFIED Requirements

### Requirement: Driver App构建配置
Driver app构建配置SHALL使用新的多源文件接口，包含DTM源码。

**原配置**:
```lua
define_user_app_target("driver", "apps/driver/main.c", "driver.bin", "0x1000")
```

**新配置**:
```lua
define_user_app_target("driver", {
    sources = {"apps/driver/main.c", "apps/driver/dtm/ips/*.c"},
    includes = {"apps/driver/dtm", "apps/driver/dtm/common", "apps/driver/dtm/ips", ...},
    output = "driver.bin",
    stack_size = "0x1000"
})
```
