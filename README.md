# Puppy RTOS

Puppy RTOS 是一个面向资源受限平台的实时微内核实验项目。当前版本重点在以下方向：

- 内核线程与调度
- 用户态应用装载与运行
- 用户/内核内存隔离与回收
- 统一的文件与控制台接口
- 一键打包生成统一固件

如果你是第一次接触本项目，建议先按下面的 5 分钟流程跑通一次，再开始看细节。

## 1. 你会得到什么

构建完成后，产物在 output 目录：

- output/puppy.bin：内核镜像
- output/romfs.bin：用户应用文件系统镜像
- output/firmware.bin：最终统一固件（QEMU 默认加载这个）

当前默认地址布局（由 core/default_config.h 统一定义）：

- Kernel Flash：0x08000000，128KB
- ROMFS Flash：紧随 Kernel 分区之后
- Kernel RAM：0x20000000，64KB
- App RAM：0x20010000，64KB

## 2. 依赖准备

### 2.1 必需工具

- xmake
- Python 3（用于固件打包与资源报告）
- arm-none-eabi 工具链
- QEMU（可选，用于运行）

### 2.2 先改这两个本地路径

本仓库默认把工具链和 QEMU 路径写在脚本里，请先按你的本机环境修改：

- xmake.lua：toolchain 的 set_sdkdir 路径
- tools/qemu-run.bat：qemu-system-arm.exe 路径

## 3. 5 分钟快速上手

在仓库根目录执行：

```powershell
xmake f -p cross -a cortex-m4 -m debug
xmake
xmake build firmware
```

成功后你会看到构建后资源报告，例如：

- Firmware 大小
- Kernel ROM 占用
- ROMFS 占用
- Kernel RAM 占用

运行 QEMU：

```powershell
./qemu-run.bat
```

如果串口窗口出现 puppy 提示符，说明系统启动成功。

## 4. 常用开发命令

### 4.1 常用构建

```powershell
# Debug
xmake f -p cross -a cortex-m4 -m debug
xmake -r

# Release
xmake f -p cross -a cortex-m4 -m release
xmake -r
```

### 4.2 打包统一固件

```powershell
xmake build firmware
```

这个目标会自动做三件事：

1. 构建内核与 user apps
2. 生成 ROMFS 并合并成 output/firmware.bin
3. 打印 ROM/RAM 资源占用报告

## 5. 用户态应用快速开发

### 5.1 目录约定

- apps/<app_name>/main.c：用户应用入口
- apps/runtime：用户态运行时与 syscall 桥
- apps/linker/app.lds：用户应用链接脚本
- apps/include：用户态公共头文件

### 5.2 新增一个用户应用的最小步骤

1. 复制一个现有示例目录（如 hello）
2. 修改 main.c 中应用逻辑
3. 在 xmake.lua 增加一个 define_user_app_target
4. 重新执行 xmake build firmware
5. 启动后通过 shell 命令加载/运行，或放在 /rom 下由系统发现

## 6. 核心目录速览

- apps：内核入口、shell、测试入口
- core/kernel：调度、syscall、内存堆、MPU
- core/app：用户应用装载与生命周期管理
- core/fs：VFS、ROMFS、DEVFS
- core/dev：设备抽象与控制台
- dtm：板级支持、启动文件、链接脚本
- tools：打包、QEMU 启动、辅助脚本

## 7. 架构文档

建议按顺序阅读：

1. docs/shell-stdio-architecture.md
2. docs/pup-fs-architecture.md
3. docs/repo-reorg-checklist.md

## 8. 常见问题

### 8.1 xmake 找不到编译器

检查 xmake.lua 里的 set_sdkdir 是否为本机实际路径。

### 8.2 QEMU 无法启动

检查 tools/qemu-run.bat 中 qemu-system-arm.exe 路径。

### 8.3 只改了代码但 firmware 没更新

请使用 xmake build firmware，而不是只执行 xmake。

### 8.4 想调整内核/ROMFS 分区

优先修改 core/default_config.h 中地址与大小宏，再执行 xmake -r 和 xmake build firmware。

## 9. 当前状态说明

- 已支持用户态应用动态内存分配与退出回收
- 已支持内核堆与用户堆使用量/峰值统计
- 已支持统一固件打包与构建后资源报告
- 当前默认目标为 Cortex-M4（stm32f407 + QEMU 板型）
