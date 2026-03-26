# Tasks

- [x] Task 1: 优化`define_user_app_target`函数支持多源文件和多包含目录
  - [x] SubTask 1.1: 修改函数签名，支持table参数格式
  - [x] SubTask 1.2: 保持向后兼容，支持原有字符串参数格式
  - [x] SubTask 1.3: 添加sources、includes、output、stack_size字段支持

- [x] Task 2: 复制DTM到driver app目录
  - [x] SubTask 2.1: 创建`apps/driver/dtm/`目录结构
  - [x] SubTask 2.2: 复制dtm/common、dtm/ips、dtm/arch等子目录
  - [x] SubTask 2.3: 复制dtm/chips/stm32f407相关文件

- [x] Task 3: 更新driver app构建配置
  - [x] SubTask 3.1: 使用新的接口格式配置driver target
  - [x] SubTask 3.2: 添加DTM源文件和包含目录

- [x] Task 4: Debug模式回归验证
  - [x] SubTask 4.1: 执行debug模式构建
  - [x] SubTask 4.2: 运行MPU测试验证
  - [x] SubTask 4.3: 运行driver chain测试验证

- [x] Task 5: Release模式回归验证
  - [x] SubTask 5.1: 执行release模式构建
  - [x] SubTask 5.2: 运行MPU测试验证
  - [x] SubTask 5.3: 运行driver chain测试验证

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on [Task 3]
- [Task 5] depends on [Task 4]
