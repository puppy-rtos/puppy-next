toolchain("arm-none-eabi")
    set_kind("standalone")
    set_sdkdir("D:/Progrem/arm-gnu-toolchain-12.2.rel1")
    set_bindir("bin")
target_end()

set_project("puppy")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")

local DTM_FLASH_BASE = "0x08000000"
local DTM_RAM_BASE = "0x20000000"
local DTM_FLASH_SIZE = "128k"

local function add_mode_compile_flags()
    if is_mode("debug") then
        add_defines("PUP_DEBUG")
        add_cxflags("-O0", "-gdwarf-2", "-g")
        add_asflags("-Wa,-g", "-gdwarf-2", "-g")
    elseif is_mode("release") then
        add_defines("NDEBUG")
        add_cxflags("-O2", "-gdwarf-2", "-g")
        add_asflags("-Wa,-g", "-gdwarf-2", "-g")
        set_strip("none")
    end
end

target("puppy")
    set_kind("binary")
    set_extension(".elf")
    set_toolchains("arm-none-eabi")

    set_arch("cortex-m4")
    set_plat("cross")
    add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -DPUP_ARCH_CORTEX_M -Wall -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -Wa,-mimplicit-it=thumb -ffunction-sections -fdata-sections -fno-builtin-printf')
    add_mode_compile_flags()
    add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections,-Map=build/puppy.map,-cref,-u,Reset_Handler',{force = true})
    add_ldflags(' -T ' .. os.scriptdir() .. '/core/dtm/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=' .. DTM_FLASH_BASE .. ' -Wl,--defsym,DTM_RAM_BASE=' .. DTM_RAM_BASE .. ' -Wl,--defsym,DTM_FLASH_SIZE=' .. DTM_FLASH_SIZE)

    add_includedirs(
        "core",
        "core/fs",
        "core/dev",
        "core/shared",
        "core/app",
        "core/kernel",
        "core/arch",
        "core/ipc",
        "core/dtm",
        "core/dtm/ips",
        "core/dtm/common",
        "core/dtm/arch/arm/cortex-m4",
        "core/libcm"
    )

    add_files(
        "core/*.c",
        "core/**/*.c",
        "core/arch/*.S",
        "core/dtm/ips/*.c",
        "core/libcm/libcm.c",
        "core/dtm/chips/stm32f407/start.s"
    )
    
    add_defines("PUP_MPU_ENABLED")

    add_includedirs(
        "core/dtm/chips/stm32f407"
    )

    after_build(function (target)
        os.mkdir("output/kernel")
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/kernel/puppy.bin", target:targetfile())
    end)

function define_user_app_target(target_name, config)
    local sources = config.sources or {}
    local includes = config.includes or {}
    local output_bin_name = config.output
    local stack_size = config.stack_size
    local defines = config.defines or {}
    
    target(target_name)
        set_kind("binary")
        set_extension(".elf")
        set_toolchains("arm-none-eabi")

        set_arch("cortex-m4")
        set_plat("cross")
        add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -ffunction-sections -fdata-sections -fno-builtin-printf -fno-builtin-execve -fno-omit-frame-pointer')
        add_cxflags('-fpie -fPIC -msingle-pic-base -mno-pic-data-is-text-relative')
        add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -ffunction-sections -fdata-sections')
        add_mode_compile_flags()
        add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections -n -pie -Wl,--no-dynamic-linker',{force = true})
        add_ldflags(' -T ' .. os.scriptdir() .. '/apps/lib/linker/app.lds')
        if stack_size ~= nil then
            add_ldflags(' -Wl,--defsym,__user_app_stack_size=' .. stack_size)
        end

        add_includedirs("apps/lib/include", "core")
        if #includes > 0 then
            add_includedirs(includes)
        end
        
        if #defines > 0 then
            add_defines(defines)
        end

        local all_sources = {}
        for _, src in ipairs(sources) do
            table.insert(all_sources, src)
        end
        table.insert(all_sources, "apps/lib/runtime/app_bootstrap.c")
        table.insert(all_sources, "apps/lib/runtime/libc.c")
        table.insert(all_sources, "apps/lib/runtime/syscall.S")
        add_files(all_sources)

        after_build(function (target)
            os.mkdir("output/apps")
            os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/apps/" .. output_bin_name, target:targetfile())
        end)
end

define_user_app_target("hello", {
    sources = {"apps/apps/hello/main.c"},
    output = "hello.bin",
    stack_size = "0x800"
})
define_user_app_target("shell", {
    sources = {"apps/apps/shell/main.c"},
    output = "shell.bin",
    stack_size = "0x800"
})
define_user_app_target("test", {
    sources = {"apps/apps/test/main.c"},
    output = "test.bin",
    stack_size = "0x800"
})
define_user_app_target("mpu_test", {
    sources = {"apps/apps/mpu_test/main.c"},
    output = "mpu_test.bin",
    stack_size = "0x800"
})
define_user_app_target("driver", {
    sources = {
        "apps/apps/driver/main.c",
        "apps/apps/driver/driver_gpio.c",
        "apps/apps/driver/driver_spi.c",
        "apps/apps/driver/driver_flash.c",
        "apps/apps/driver/dtm/ips/gpio_arm_m4_st_gd.c",
        "apps/apps/driver/dtm/ips/clk_arm_m4_st_gd.c",
        "apps/apps/driver/dtm/ips/spi_arm_m4_st_gd.c",
        "apps/apps/driver/sfud/src/*.c"
    },
    includes = {
        "apps/apps/driver",
        "apps/apps/driver/dtm",
        "apps/apps/driver/dtm/common",
        "apps/apps/driver/dtm/ips",
        "apps/apps/driver/dtm/arch/arm/cortex-m4",
        "apps/apps/driver/dtm/chips/stm32f407",
        "apps/apps/driver/sfud/inc",
        "core/shared"
    },
    output = "driver.bin",
    stack_size = "0x800"
})

target("firmware")
    set_kind("phony")
    set_toolchains("arm-none-eabi")
    set_arch("cortex-m4")
    set_plat("cross")
    add_deps("puppy", "hello", "shell", "test", "mpu_test", "driver")
    
    after_build(function (target)
        local mode = is_mode("debug") and "debug" or "release"
        print("Packing ROMFS and creating unified firmware...")
        os.mkdir("output/firmware")
        os.run("python tools/merge_firmware.py --apps-dir output/apps")
        os.exec("python tools/build_resource_report.py --mode %s", mode)
    end)
