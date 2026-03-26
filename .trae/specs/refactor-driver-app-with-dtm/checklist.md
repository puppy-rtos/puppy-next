# Checklist

## 构建系统验证
- [x] `define_user_app_target`函数支持table参数格式
- [x] `define_user_app_target`函数保持向后兼容（字符串参数）
- [x] driver app构建配置正确使用新接口

## DTM集成验证
- [x] `apps/driver/dtm/`目录结构完整
- [x] dtm/common头文件已复制
- [x] dtm/ips源文件已复制
- [x] dtm/arch头文件已复制
- [x] dtm/chips/stm32f407文件已复制

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
