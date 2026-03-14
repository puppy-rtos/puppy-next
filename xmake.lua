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
        add_cxflags("-O2")
        add_asflags("-Wa,-g")
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
    add_ldflags(' -T ' .. os.scriptdir() .. '/dtm/link.lds')
    add_ldflags(' -Wl,--defsym,DTM_FLASH_BASE=' .. DTM_FLASH_BASE .. ' -Wl,--defsym,DTM_RAM_BASE=' .. DTM_RAM_BASE .. ' -Wl,--defsym,DTM_FLASH_SIZE=' .. DTM_FLASH_SIZE)

    add_includedirs(
        "core",
        "core/fs",
        "core/dev",
        "core/app",
        "core/kernel",
        "core/arch",
        "dtm",
        "dtm/ips",
        "dtm/common",
        "dtm/arch/arm/cortex-m4",
        "libcm"
    )

    add_files(
        "apps/*.c",
        "core/**/*.c",
        "core/**/*.S",
        "dtm/ips/*.c",
        "libcm/libcm.c",
        "dtm/chips/stm32f407/start.s"
    )
    
    add_defines("PUP_MPU_ENABLED")

    add_includedirs(
        "dtm/chips/stm32f407"
    )

    after_build(function (target)
        os.mkdir("output")
        os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/puppy.bin", target:targetfile())
    end)

function define_user_app_target(target_name, app_source, output_bin_name)
    target(target_name)
        set_kind("binary")
        set_extension(".elf")
        set_toolchains("arm-none-eabi")

        set_arch("cortex-m4")
        set_plat("cross")
        add_cxflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Dgcc -Wall -ffunction-sections -fdata-sections -fno-builtin-printf')
        add_cxflags('-fpie -fPIC -msingle-pic-base -mno-pic-data-is-text-relative')
        add_asflags('-c -mcpu=cortex-m4 -mthumb -mfloat-abi=soft -x assembler-with-cpp -ffunction-sections -fdata-sections')
        add_mode_compile_flags()
        add_ldflags('-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -nostartfiles -Wl,--gc-sections -n -pie -Wl,--no-dynamic-linker',{force = true})
        add_ldflags(' -T ' .. os.scriptdir() .. '/user_apps/linker/app.lds')

        add_includedirs("user_apps/include")

        add_files(
            app_source,
            "user_apps/runtime/user_libc.c",
            "user_apps/runtime/user_syscall.S"
        )

        after_build(function (target)
            os.mkdir("output")
            os.run("D:/Progrem/arm-gnu-toolchain-12.2.rel1/bin/arm-none-eabi-objcopy -O binary %s output/" .. output_bin_name, target:targetfile())
        end)
end

define_user_app_target("hello_app", "user_apps/apps/hello/main.c", "hello_app.bin")
define_user_app_target("fs_test_app", "user_apps/apps/fs_test/main.c", "fs_test_app.bin")
define_user_app_target("shell_app", "user_apps/apps/shell/main.c", "shell_app.bin")

target("firmware")
    set_kind("phony")
    set_toolchains("arm-none-eabi")
    set_arch("cortex-m4")
    set_plat("cross")
    add_deps("puppy", "hello_app", "fs_test_app", "shell_app")
    
    after_build(function (target)
        local mode = is_mode("debug") and "debug" or "release"
        print("Packing ROMFS and creating unified firmware...")
        os.run("python tools/merge_firmware.py --apps-dir output")
        os.exec("python tools/build_resource_report.py --mode %s", mode)
    end)
