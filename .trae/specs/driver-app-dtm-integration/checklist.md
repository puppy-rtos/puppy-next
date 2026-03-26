# Checklist

## 构建系统验证
- [x] `define_user_app_target`函数只支持table参数格式
- [x] 所有app构建配置已更新为table格式
- [x] hello app构建配置正确
- [x] shell app构建配置正确
- [x] test app构建配置正确
- [x] mpu_test app构建配置正确
- [x] driver app构建配置正确

## DTM集成验证
- [x] driver app使用dtm_gpio接口操作GPIO
- [x] driver app使用dtm_clk接口操作时钟
- [x] driver app使用dtm接口配置中断

## Debug模式回归验证
- [x] debug模式构建成功
- [x] MPU violate_end测试通过
- [x] MPU violate_start测试通过
- [x] driver chain测试通过

## Release模式回归验证
- [x] release模式构建成功
- [x] MPU violate_end测试通过
- [x] MPU violate_start测试通过
- [x] driver chain测试通过
