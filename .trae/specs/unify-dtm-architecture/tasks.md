# Tasks

- [ ] Task 1: 修改 xmake.lua，让 driver 和 driver-m7 目标使用 core/dtm
  - [ ] SubTask 1.1: 修改 driver 目标的 sources 和 includes 路径
  - [ ] SubTask 1.2: 修改 driver-m7 目标的 sources 和 includes 路径
  - [ ] SubTask 1.3: 添加 DTM_NO_SPI_FLASH 到 driver-m7 的 defines

- [ ] Task 2: 验证构建
  - [ ] SubTask 2.1: 构建 driver 目标
  - [ ] SubTask 2.2: 构建 driver-m7 目标
  - [ ] SubTask 2.3: 完整构建所有目标

- [ ] Task 3: 删除冗余的 apps/apps/driver/dtm 目录
  - [ ] SubTask 3.1: 确认构建通过后删除目录

# Task Dependencies

- Task 2 依赖 Task 1
- Task 3 依赖 Task 2
