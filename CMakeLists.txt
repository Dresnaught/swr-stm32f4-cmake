cmake_minimum_required(VERSION 3.20)
project(swr-stm32f4-cmake C ASM)

# Enable Neovim LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. STM32F401RCT6 Architecture Flags
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

add_compile_options(
  ${MCU_FLAGS}
  -Wall
  -Wextra
  -O2
  -ffunction-sections
  -fdata-sections
)
add_link_options(
  ${MCU_FLAGS}
  -specs=nano.specs
  -Wl,--gc-sections
  -T${CMAKE_CURRENT_SOURCE_DIR}/LinkerScript.ld
)

# 2. Critical Macro for STM32F401RC (tells stm32f4xx.h to pick stm32f401xc.h)
add_compile_definitions(STM32F401xC)

# 3. Direct Directory Mappings
include_directories(
    Core/Inc
    Drivers/CMSIS/Core/Include
    Drivers/CMSIS/Device
)

file(GLOB_RECURSE SOURCES "Core/Src/*.c" "Core/Src/*.s")

add_executable(${PROJECT_NAME}.elf ${SOURCES})

# Post-build formatting transformations
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-size ${PROJECT_NAME}.elf
)

# Flash via stlink-tools
add_custom_target(flash
    COMMAND st-flash write ${PROJECT_NAME}.bin 0x08000000
    DEPENDS ${PROJECT_NAME}.elf
)
