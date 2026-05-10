cmake_minimum_required(VERSION 3.22)

project(bp_monitor C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HAL_GIT_TAG v1.1.10)
set(CMSIS_DEVICE_GIT_TAG v4.3.5)

include(FetchContent)

FetchContent_Declare(
  stm32f1_hal
  GIT_REPOSITORY https://github.com/STMicroelectronics/stm32f1xx_hal_driver.git
  GIT_TAG ${HAL_GIT_TAG}
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stm32f1xx_hal_driver")

FetchContent_Declare(
  stm32f1_device
  GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis_device_f1.git
  GIT_TAG ${CMSIS_DEVICE_GIT_TAG}
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmsis_device_f1")

FetchContent_GetProperties(stm32f1_hal)
if(NOT stm32f1_hal_POPULATED)
  FetchContent_Populate(stm32f1_hal)
endif()

FetchContent_GetProperties(stm32f1_device)
if(NOT stm32f1_device_POPULATED)
  FetchContent_Populate(stm32f1_device)
endif()

set(HAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stm32f1xx_hal_driver")
set(DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmsis_device_f1")
set(CMSIS_CORE_INC "${CMAKE_CURRENT_SOURCE_DIR}/cmsis_core/Include")
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F103C8Tx_FLASH.ld")

set(HAL_SOURCES
    "${HAL_DIR}/Src/stm32f1xx_hal.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_cortex.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_rcc.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_gpio.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_flash.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_pwr.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_i2c.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_tim.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_tim_ex.c"
    "${HAL_DIR}/Src/stm32f1xx_hal_uart.c")

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/*.c")

set(SYSTEM_SOURCES
    "${DEVICE_DIR}/Source/Templates/system_stm32f1xx.c")

set(STARTUP_ASM "${DEVICE_DIR}/Source/Templates/gcc/startup_stm32f103xb.s")

add_executable(bp_monitor
    ${STARTUP_ASM}
    ${SYSTEM_SOURCES}
    ${HAL_SOURCES}
    ${APP_SOURCES})

target_include_directories(bp_monitor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc"
    "${DEVICE_DIR}/Include"
    "${CMSIS_CORE_INC}"
    "${HAL_DIR}/Inc")

# CMSIS: STM32F103xB covers STM32F103C8/CB/R8/RB… (see stm32f1xx.h in cmsis_device_f1).
# C8T6 → keep STM32F103xB + 64 KiB linker script below (not a separate STM32F103x8 define).
target_compile_definitions(bp_monitor PRIVATE
    USE_HAL_DRIVER
    STM32F103xB)

target_compile_options(bp_monitor PRIVATE
    -mcpu=cortex-m3
    -mthumb
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
    -ffunction-sections
    -fdata-sections
    -Wall
    -Wno-unused-parameter
    $<$<CONFIG:Debug>:-Og -g3>
    $<$<CONFIG:Release>:-Os>)

target_link_options(bp_monitor PRIVATE
    -T${LINKER_SCRIPT}
    -mcpu=cortex-m3
    -mthumb
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=${CMAKE_BINARY_DIR}/bp_monitor.map
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-u,_printf_float)

set_property(TARGET bp_monitor PROPERTY LINK_DEPENDS ${LINKER_SCRIPT})

add_custom_command(TARGET bp_monitor POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bp_monitor>
            ${CMAKE_BINARY_DIR}/bp_monitor.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:bp_monitor>)
