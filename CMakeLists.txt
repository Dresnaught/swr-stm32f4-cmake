cmake_minimum_required(VERSION 3.20)
project(swr-stm32f4-cmake C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_definitions(STM32F401xC)

include_directories(
    Core/Inc
    Core/Src
    Drivers/CMSIS/Core/Include
    Drivers/CMSIS/Device
)

file(GLOB_RECURSE SOURCES
  "Core/Src/*.c"
  "Core/Src/*.s"
  "Core/Src/*.S"
)

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_options(${PROJECT_NAME} PRIVATE
    "-T${CMAKE_CURRENT_SOURCE_DIR}/LinkerScript.ld"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map"
    "-Wl,--gc-sections"
    "--specs=nano.specs"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-none-eabi-objcopy -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-objcopy -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-size ${PROJECT_NAME}.elf
    COMMENT "Building flash image binaries and displaying memory sizes..."
)

# Flash via stlink-tools
add_custom_target(flash
    COMMAND st-flash --reset write ${PROJECT_NAME}.bin 0x08000000
    DEPENDS ${PROJECT_NAME}.elf
)
