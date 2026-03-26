# Driver App DTM集成重构 Spec

## Why
当前driver app直接操作硬件寄存器，代码重复且难以维护。需要重构driver app使用DTM抽象层，同时简化`define_user_app_target`接口，移除向后兼容代码。

## What Changes
- **BREAKING**: 简化`define_user_app_target`函数，只支持table参数格式
- 重构driver app使用DTM接口替代直接寄存器操作
- 更新所有app的构建配置使用新接口格式

## Impact
- Affected specs: driver app构建系统，所有用户态app构建配置
- Affected code: 
  - `xmake.lua` 构建配置
  - `apps/driver/main.c` 驱动实现

## ADDED Requirements

### Requirement: Driver App使用DTM接口
Driver app SHALL使用DTM抽象层接口操作硬件，而非直接操作寄存器。

#### Scenario: GPIO操作使用DTM
- **WHEN** driver app需要配置GPIO时
- **THEN** 使用`dtm_gpio_*`接口而非直接操作GPIO寄存器

#### Scenario: UART操作使用DTM
- **WHEN** driver app需要操作UART时
- **THEN** 使用`dtm_uart_*`接口而非直接操作UART寄存器

### Requirement: 简化的构建接口
`define_user_app_target`函数SHALL只支持table参数格式，移除向后兼容代码。

#### Scenario: 统一接口格式
- **WHEN** 定义用户态app target时
- **THEN** 必须使用table格式：
```lua
define_user_app_target("app_name", {
    sources = {"path/to/source.c"},
    includes = {"path/to/include"},
    output = "output.bin",
    stack_size = "0x1000"
})
```

## MODIFIED Requirements

### Requirement: 所有App构建配置更新
所有用户态app的构建配置SHALL更新为新的table格式。

**原配置**:
```lua
define_user_app_target("hello", "apps/hello/main.c", "hello.bin")
define_user_app_target("shell", "apps/shell/main.c", "shell.bin", "0x1000")
```

**新配置**:
```lua
define_user_app_target("hello", {
    sources = {"apps/hello/main.c"},
    output = "hello.bin"
})
define_user_app_target("shell", {
    sources = {"apps/shell/main.c"},
    output = "shell.bin",
    stack_size = "0x1000"
})
```

## REMOVED Requirements

### Requirement: 向后兼容字符串参数
**Reason**: 简化接口，统一使用table格式
**Migration**: 所有app配置更新为table格式
