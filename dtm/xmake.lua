-- 定义目标平台和工具链
toolchain("arm-none-eabi")
    set_kind("standalone")
    set_sdkdir("D:/Progrem/arm-gnu-toolchain-12.2.rel1")  -- 修改为你的工具链路径
    set_bindir("bin")
target_end()

-- 包含路径
add_includedirs(
    ".",
    "./ips",
    "./common"
)

-- 源文件
add_files(
    "main.c",
    "ips/*.c"
)

-- 项目配置
target("stm32f407-dtm")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")

    -- 设置目标平台和CPU架构
    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -O0  -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=dtm.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=0x08000000 -Wl,--defsym,DTM_RAM_BASE=0x20000000')

    -- 包含路径
    add_includedirs(
        "arch/arm/cortex-m4",
        "./chips/stm32f407"
    )

    -- 源文件
    add_files(
        "chips/stm32f407/start.s"
    )

    -- 构建后生成bin文件
    after_build(function (target)
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/stm32f407-dtm.bin", target:targetfile())
    end)

target_end()

target("stm32f412-dtm")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")

    -- 设置目标平台和CPU架构
    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -O0  -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=dtm.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=0x08000000 -Wl,--defsym,DTM_RAM_BASE=0x20000000')

    -- 包含路径
    add_includedirs(
        "arch/arm/cortex-m4",
        "./chips/stm32f412"
    )

    -- 源文件
    add_files(
        "chips/stm32f412/start.s"
    )
    
    -- 构建后生成bin文件
    after_build(function (target)
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/stm32f412-dtm.bin", target:targetfile())
    end)

target_end()
-- 项目配置
target("stm32h750-dtm")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")

    -- 设置目标平台和CPU架构
    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -O0  -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=dtm.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=0x08000000 -Wl,--defsym,DTM_RAM_BASE=0x20000000')

    -- 包含路径
    add_includedirs(
        "arch/arm/cortex-m4",
        "./chips/stm32h750"
    )

    -- 源文件
    add_files(
        "chips/stm32h750/start.s"
    )

    -- 构建后生成bin文件
    after_build(function (target)
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/stm32h750-dtm.bin", target:targetfile())
    end)

target_end()

-- 项目配置
target("gd32h759-dtm")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")
    
    -- 设置目标平台和CPU架构
    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -O0  -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=dtm.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=0x08000000 -Wl,--defsym,DTM_RAM_BASE=0x24000000')
    
    -- 包含路径
    add_includedirs(
        "arch/arm/cortex-m4",
        "./chips/gd32h759"
    )

    -- 源文件
    add_files(
        "chips/gd32h759/start.s"
    )
    
    -- 构建后生成bin文件
    after_build(function (target)
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/gd32h759-dtm.bin", target:targetfile())
    end)
    
target_end()
-- 项目配置
target("gd32f427-dtm")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")

    -- 设置目标平台和CPU架构
    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -O0  -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -gdwarf-2 -g -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=dtm.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=0x08000000 -Wl,--defsym,DTM_RAM_BASE=0x20000000')

    -- 包含路径
    add_includedirs(
        "arch/arm/cortex-m4",
        "./chips/gd32f427"
    )

    -- 源文件
    add_files(
        "chips/gd32f427/start.s"
    )

    -- 构建后生成bin文件
    after_build(function (target)
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/gd32f427-dtm.bin", target:targetfile())
    end)

target_end()