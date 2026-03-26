# Tasks

- [x] Task 1: 简化`define_user_app_target`函数
  - [x] SubTask 1.1: 移除向后兼容代码，只保留table参数格式
  - [x] SubTask 1.2: 简化函数实现，移除类型判断逻辑

- [x] Task 2: 更新所有app构建配置
  - [x] SubTask 2.1: 更新hello app配置
  - [x] SubTask 2.2: 更新shell app配置
  - [x] SubTask 2.3: 更新test app配置
  - [x] SubTask 2.4: 更新mpu_test app配置
  - [x] SubTask 2.5: 更新driver app配置

- [x] Task 3: 重构driver app使用DTM接口
  - [x] SubTask 3.1: 使用dtm_gpio接口替代直接GPIO寄存器操作
  - [x] SubTask 3.2: 使用dtm_clk接口替代直接RCC寄存器操作
  - [x] SubTask 3.3: 使用dtm接口配置EXTI中断

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
