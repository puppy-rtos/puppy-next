# Puppy 仓库重构清单（可执行版）

## 0. 当前状态

- 已完成：`user_apps` 目录重组（include/runtime/linker/apps）。
- 已完成：`core` 子目录重组（fs/dev/app/kernel），`core/arch` 保持原位。
- 已完成：`xmake` 路径迁移与用户应用目标模板函数收敛。
- 已完成：脚本归档到 `tools`（打包与 QEMU 脚本），根目录保留兼容入口。
- 已完成：`xmake -r`、`xmake -b firmware`、`tools/qemu-test.bat` 验证通过。

## 1. 约束与目标

- 保持 `core/arch` 在 `core` 目录内，不外移。
- 保持当前可编译、可运行优先，按“分阶段、小步迁移”执行。
- 文件系统、设备层命名继续遵循 `pup_` 前缀规范。

## 2. 顶层目录建议（不破坏现有构建）

建议先保持现有顶层不动，仅补充文档语义：

- `core`：内核核心、VFS、设备注册、体系结构适配。
- `apps`：内核态示例/入口逻辑。
- `user_apps`：用户态运行库与用户应用。
- `dtm`：芯片和板级抽象。
- `libcm`：轻量公共库。
- `docs`：架构与维护文档。
- `output`、`build`：产物目录。

## 3. core 目录梳理建议（保留 core/arch）

### 3.1 目标形态

- `core/arch`（保留）
- `core/fs`
- `core/dev`
- `core/app`
- `core/kernel`

### 3.2 文件映射建议（第一批）

- `core/pup_fs.c`、`core/pup_fs.h` -> `core/fs/`
- `core/pup_romfs.c`、`core/pup_romfs.h` -> `core/fs/`
- `core/pup_devfs.c`、`core/pup_devfs.h` -> `core/fs/`
- `core/dev_uart.c`、`core/dev_uart.h` -> `core/dev/`
- `core/dev_gpio.c`、`core/dev_gpio.h` -> `core/dev/`
- `core/app_loader.c`、`core/app_loader.h` -> `core/app/`
- `core/app_manager.c`、`core/app_manager.h` -> `core/app/`
- `core/puppy_core.c`、`core/puppy_core.h`、`core/syscall.c`、`core/syscall.h`、`core/mpu.c`、`core/mpu.h` -> `core/kernel/`

## 4. user_apps 目录梳理建议（重点）

### 4.1 现状问题

- 运行库、头文件、链接脚本、应用源码平铺在同一层。
- 每新增一个应用都需要重复维护目标和路径。

### 4.2 目标形态

- `user_apps/include`
- `user_apps/runtime`
- `user_apps/linker`
- `user_apps/apps/hello`
- `user_apps/apps/fs_test`
- `user_apps/apps/shell`

### 4.3 文件映射建议

- 头文件移入 `user_apps/include`：
  - `app_header.h`
  - `user_fs.h`
  - `user_libc.h`
  - `user_syscall.h`
- 运行库移入 `user_apps/runtime`：
  - `user_libc.c`
  - `user_syscall.S`
- 链接脚本移入 `user_apps/linker`：
  - `app.lds`
- 应用源码移入 `user_apps/apps/*`：
  - `hello_app.c` -> `user_apps/apps/hello/main.c`
  - `fs_test_app.c` -> `user_apps/apps/fs_test/main.c`
  - `shell_app.c` -> `user_apps/apps/shell/main.c`

## 5. xmake 调整建议

分两步改，降低风险：

### 5.1 第一步（先兼容）

- 保持目标名不变：`hello_app`、`fs_test_app`、`shell_app`。
- 各目标统一添加：
  - `add_includedirs("user_apps/include")`
- 链接脚本路径改为：
  - `user_apps/linker/app.lds`
- 运行库统一引用：
  - `user_apps/runtime/user_libc.c`
  - `user_apps/runtime/user_syscall.S`

### 5.2 第二步（再收敛）

- 抽一个 Lua 辅助函数来创建用户应用目标，减少重复。
- 新增应用时只提供：应用名 + 源码路径。

状态：已完成（`xmake.lua` 中已使用 `define_user_app_target` 统一用户应用目标定义）。

## 6. 执行顺序（推荐）

1. 先做 `user_apps` 目录重组（收益高、影响边界清晰）。
2. 编译验证：`xmake -r`。
3. 运行验证：`xmake -b firmware` + `tools/qemu-test.bat`。
4. 再做 `core` 子目录重组。
5. 每迁移一个子域就执行一次第 2-3 步。

## 7. 验收标准

- 构建通过：`xmake -r`。
- 固件打包通过：`xmake -b firmware`。
- QEMU 启动通过：`tools/qemu-test.bat` 可见 shell 提示符。
- 无旧路径残留 include（全局搜索确认）。

## 8. 回滚策略

- 每一阶段单独提交。
- 若阶段失败，仅回滚该阶段变更，不影响其他模块。
- 不跨阶段混改（目录迁移与逻辑修改分开）。
