# Tasks

- [x] Task 1: 创建 STM32H743 芯片目录和绑定文件
  - [x] SubTask 1.1: 创建 `core/dtm/chips/stm32h743/dtm_bindings.h`，参照 stm32h750 的 dtm_bindings.h，修改为 H743 的 IP 地址和配置（HSE=8MHz, SYSCLK=480MHz, Debug UART=USART1 PA9/PA10）
  - [x] SubTask 1.2: 创建 `core/dtm/chips/stm32h743/start.s`，参照 stm32h750 的 start.s，将 `.cpu cortex-m4` 改为 `.cpu cortex-m7`，更新向量表为 STM32H743 的中断向量

- [x] Task 2: 创建 Cortex-M7 架构支持目录
  - [x] SubTask 2.1: 创建 `core/dtm/arch/arm/cortex-m7/dtm_irq.h`，从 `cortex-m4/dtm_irq.h` 复制

- [x] Task 3: 在根 xmake.lua 中新增 M7 系列构建目标
  - [x] SubTask 3.1: 新增 `define_user_app_target_m7` 函数，与 `define_user_app_target` 类似但使用 cortex-m7 标志，输出 bin 名加 `-m7` 后缀
  - [x] SubTask 3.2: 新增 `puppy-m7` 内核 target，使用 cortex-m7 标志、stm32h743 芯片目录、H743 内存布局 defines
  - [x] SubTask 3.3: 新增 M7 版 app targets（hello-m7, shell-m7, test-m7, mpu_test-m7, lfs-m7）
  - [x] SubTask 3.4: 新增 `driver-m7` target
  - [x] SubTask 3.5: 新增 `firmware-m7` target，依赖所有 M7 目标，打包生成 firmware-m7.bin

- [x] Task 4: 验证构建
  - [x] SubTask 4.1: 执行 `xmake -r`，确认 M4 目标（puppy, firmware）构建正常
  - [x] SubTask 4.2: 执行 `xmake build puppy-m7`，确认 M7 内核构建正常
  - [x] SubTask 4.3: 执行 `xmake build firmware-m7`，确认 M7 固件打包正常

# Task Dependencies

- Task 1 和 Task 2 可并行执行
- Task 3 依赖 Task 1 和 Task 2（需要芯片目录和架构目录存在）
- Task 4 依赖 Task 3 全部完成
