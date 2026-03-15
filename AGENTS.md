# AGENTS.md

本文件用于帮助智能体在最短时间内理解 `puppy-next` 项目并安全地进行修改。

## 1. 项目速览

- 项目名称: puppy-next
- 目标: 面向资源受限平台的实时微内核实验项目，当前包含内核、设备抽象、简化文件系统、内核 shell、用户态应用加载与运行能力。
- 主要语言: C / 汇编 / Lua(xmake)
- 主要构建系统: xmake
- 默认平台配置: cross + cortex-m4

## 2. 关键目录

- `core/`: 核心实现。
- `core/kernel/`: 调度、syscall、MPU 等核心内核逻辑。
- `core/arch/`: 架构相关代码（Cortex-M、RISC-V、syscall/user thread 汇编入口）。
- `core/dev/`: 设备层（GPIO/UART/console）。
- `core/fs/`: 文件系统与 devfs/romfs 相关实现。
- `core/app/`: 用户应用加载与管理。
- `dtm/`: 板级与芯片支持包、链接脚本、启动文件。
- `apps/`: 用户态 app、运行时、链接脚本。
- `tools/`: QEMU 启动脚本与固件打包脚本。
- `output/`: 构建产物目录（如 `puppy.bin`、`firmware.bin`、`*_app.bin`）。

## 3. 构建与运行

### 3.1 常用命令

在项目根目录执行:

```powershell
xmake f -p cross -a cortex-m4 -m debug
xmake
xmake build firmware
./qemu-run.bat
```

说明:

- 根目录 `qemu-run.bat` 会转发到 `tools/qemu-run.bat`。
- `firmware` 目标会调用 `tools/merge_firmware.py` 将内核与 ROMFS 应用打包为统一镜像。

### 3.2 典型运行验证

- shell 提示符是否出现（如 `puppy> `）。
- 基本命令是否可用: `help`、`ps`、`apps`、`ls`、`test`。

## 4. 智能体首选阅读顺序

当需要快速定位问题时，优先阅读:

1. `README.md`
2. `xmake.lua`
3. `apps/shell.c`
4. `core/kernel/puppy_core.c`
5. `core/syscall*` 与 `core/arch/*`
6. `core/fs/*`（若问题与文件系统/设备节点相关）
7. `core/app/*` 与 `apps/*`（若问题与用户态应用相关）

## 5. 修改原则（面向智能体）

- 只做最小必要改动，避免无关重构和大面积格式化。
- 优先保持已有接口与宏命名风格，除非任务明确要求重命名。
- 涉及架构相关代码时，检查是否同时影响 `core/arch/` 与 `dtm/chips/*`。
- 新增 shell 命令时，需要同步:
  - 命令声明
  - 命令表 `shell_cmds`
  - 参数与错误提示
- 涉及文件系统/设备节点改动时，至少验证 `ls`/`cat`/`echo` 路径。
- 涉及用户态 app 装载链路时，检查打包脚本与 `output/*_app.bin` 命名是否一致。

## 6. 提交前最小自检

1. 能编译通过（至少 `xmake` 成功）。
2. 能启动（至少 `./qemu-run.bat` 不报启动错误）。
3. 受影响功能有对应手工验证步骤与结果说明。
4. 若修改公共头文件，确认未引入明显的跨模块编译回归。

## 7. 常见入口文件

- 构建入口: `xmake.lua`
- 内核 shell: `apps/shell.c`
- 固件合并: `tools/merge_firmware.py`
- QEMU 启动入口: `qemu-run.bat`、`tools/qemu-run.bat`

---

如果任务目标不明确，智能体应先说明假设，再从 `apps/shell.c` 与 `core/kernel/` 开始最小范围定位。