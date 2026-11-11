cmake_minimum_required(VERSION 3.21)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/toolchain.cmake" CACHE PATH "" FORCE)
project(blinky LANGUAGES C ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(MCU cortex-m4)
set(LDSCRIPT ${CMAKE_SOURCE_DIR}/linker/memory.ld)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(fw.elf
    src/main.c
    src/spi.c
    src/protocol.c
    src/encoder.c
    src/control.c
    src/drive.c
    src/loops.c
    src/clock.c
    src/pwm.c
    src/svpwm.c
    src/drv8353.c
    src/system_stm32f4xx.c
    src/current_feedback.c
    src/isr_timing.c
    src/observer.c
    src/trap_gen.c
    src/minjerk_gen.c
    src/sysid/foc_sysid.c
    src/sysid/foc_trajectory.c
    startup/startup_stm32f411xe.s
)

target_include_directories(fw.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Include
    ${CMAKE_SOURCE_DIR}/Include/sysid
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)

target_compile_definitions(fw.elf PRIVATE
    STM32F411xE
)

target_compile_options(fw.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        -mcpu=${MCU}
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
        -ffreestanding
        -fno-builtin
        -fdata-sections
        -ffunction-sections
        -O2
        -g3
        -Wall
        -Wextra
    >
    $<$<COMPILE_LANGUAGE:ASM>:
        -mcpu=${MCU}
        -mthumb
        -x assembler-with-cpp
    >
)

target_link_options(fw.elf PRIVATE
    -mcpu=${MCU}
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -T${LDSCRIPT}
    -nostartfiles
    -specs=nano.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/fw.map
    -L${ARM_TOOLCHAIN_ROOT}/arm-none-eabi/lib/thumb/v7e-m+fp/hard
    -Wl,--start-group
        -lc
        -lgcc
        -lm
        -lnosys
    -Wl,--end-group
)

target_link_libraries(fw.elf PRIVATE gcc)
target_link_libraries(fw.elf PRIVATE m)

set_property(TARGET fw.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})

add_custom_command(TARGET fw.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:fw.elf> ${CMAKE_BINARY_DIR}/fw.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:fw.elf> ${CMAKE_BINARY_DIR}/fw.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:fw.elf>
)

# Board-only smoke test. This target never links motor-control peripherals.
add_executable(bringup.elf
    src/bringup_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)

target_include_directories(bringup.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(bringup.elf PRIVATE STM32F411xE)
target_compile_options(bringup.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        -mcpu=${MCU} -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
        -ffreestanding -fno-builtin -fdata-sections -ffunction-sections
        -Og -g3 -Wall -Wextra
    >
    $<$<COMPILE_LANGUAGE:ASM>:
        -mcpu=${MCU} -mthumb -x assembler-with-cpp
    >
)
target_link_options(bringup.elf PRIVATE
    -mcpu=${MCU} -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -T${LDSCRIPT} -nostartfiles -specs=nano.specs
    -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/bringup.map
    -L${ARM_TOOLCHAIN_ROOT}/arm-none-eabi/lib/thumb/v7e-m+fp/hard
    -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
)
target_link_libraries(bringup.elf PRIVATE gcc)
set_property(TARGET bringup.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})

add_custom_command(TARGET bringup.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:bringup.elf> ${CMAKE_BINARY_DIR}/bringup.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:bringup.elf> ${CMAKE_BINARY_DIR}/bringup.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:bringup.elf>
)

# Logic-only DRV8353RS communication test. This target deliberately excludes
# pwm.c, tim1.c, current_feedback.c, and all motor-control code.
add_executable(drv8353-bringup.elf
    src/drv8353_bringup_main.c
    src/drv8353.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)

target_include_directories(drv8353-bringup.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
)
target_compile_definitions(drv8353-bringup.elf PRIVATE STM32F411xE)
target_compile_options(drv8353-bringup.elf PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        -mcpu=${MCU} -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
        -ffreestanding -fno-builtin -fdata-sections -ffunction-sections
        -Og -g3 -Wall -Wextra
    >
    $<$<COMPILE_LANGUAGE:ASM>:
        -mcpu=${MCU} -mthumb -x assembler-with-cpp
    >
)
target_link_options(drv8353-bringup.elf PRIVATE
    -mcpu=${MCU} -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    -T${LDSCRIPT} -nostartfiles -specs=nano.specs
    -Wl,--gc-sections -Wl,-Map=${CMAKE_BINARY_DIR}/drv8353-bringup.map
    -L${ARM_TOOLCHAIN_ROOT}/arm-none-eabi/lib/thumb/v7e-m+fp/hard
    -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group
)
target_link_libraries(drv8353-bringup.elf PRIVATE gcc)
set_property(TARGET drv8353-bringup.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})

add_custom_command(TARGET drv8353-bringup.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:drv8353-bringup.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bringup.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:drv8353-bringup.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bringup.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-bringup.elf>
)

# Independent read-only timing diagnostic; preserves the original bring-up image.
add_executable(drv8353-spi-compare.elf
    src/drv8353_bringup_main.c
    src/drv8353.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_BRINGUP_INCLUDES drv8353-bringup.elf INCLUDE_DIRECTORIES)
get_target_property(DRV_BRINGUP_COMPILE_OPTIONS drv8353-bringup.elf COMPILE_OPTIONS)
get_target_property(DRV_BRINGUP_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
# Keep the map artifact separate as well as the executable/HEX/BIN files.
list(TRANSFORM DRV_BRINGUP_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-spi-compare.map")
target_include_directories(drv8353-spi-compare.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-spi-compare.elf PRIVATE STM32F411xE DRV_SPI_COMPARE)
target_compile_options(drv8353-spi-compare.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-spi-compare.elf PRIVATE ${DRV_BRINGUP_LINK_OPTIONS})
target_link_libraries(drv8353-spi-compare.elf PRIVATE gcc)
set_property(TARGET drv8353-spi-compare.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-spi-compare.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-spi-compare.elf>
            ${CMAKE_BINARY_DIR}/drv8353-spi-compare.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-spi-compare.elf>
            ${CMAKE_BINARY_DIR}/drv8353-spi-compare.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-spi-compare.elf>
)

# Reversible CSA-gain write test, independent of both read-only diagnostics.
add_executable(drv8353-spi-write-test.elf
    src/drv8353_bringup_main.c
    src/drv8353.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_WRITE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_WRITE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-spi-write-test.map")
target_include_directories(drv8353-spi-write-test.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-spi-write-test.elf PRIVATE STM32F411xE DRV_SPI_WRITE_TEST)
target_compile_options(drv8353-spi-write-test.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-spi-write-test.elf PRIVATE ${DRV_WRITE_LINK_OPTIONS})
target_link_libraries(drv8353-spi-write-test.elf PRIVATE gcc)
set_property(TARGET drv8353-spi-write-test.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-spi-write-test.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-spi-write-test.elf>
            ${CMAKE_BINARY_DIR}/drv8353-spi-write-test.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-spi-write-test.elf>
            ${CMAKE_BINARY_DIR}/drv8353-spi-write-test.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-spi-write-test.elf>
)

# Software-triggered ADC observation only; no motor-control peripherals linked.
add_executable(drv8353-analog.elf
    src/drv8353_analog_main.c
    src/drv8353.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_ANALOG_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_ANALOG_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-analog.map")
target_include_directories(drv8353-analog.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-analog.elf PRIVATE STM32F411xE)
target_compile_options(drv8353-analog.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-analog.elf PRIVATE ${DRV_ANALOG_LINK_OPTIONS})
target_link_libraries(drv8353-analog.elf PRIVATE gcc)
set_property(TARGET drv8353-analog.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-analog.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-analog.elf>
            ${CMAKE_BINARY_DIR}/drv8353-analog.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-analog.elf>
            ${CMAKE_BINARY_DIR}/drv8353-analog.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-analog.elf>
)

# No-PWM measurement of fresh zero offsets at the intended 40 V/V CSA gain.
add_executable(drv8353-current-zero.elf
    src/drv8353_analog_main.c
    src/drv8353.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_ZERO_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_ZERO_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-current-zero.map")
target_include_directories(drv8353-current-zero.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-current-zero.elf PRIVATE STM32F411xE DRV_ANALOG_ZERO_CAL)
target_compile_options(drv8353-current-zero.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-current-zero.elf PRIVATE ${DRV_ZERO_LINK_OPTIONS})
target_link_libraries(drv8353-current-zero.elf PRIVATE gcc)
set_property(TARGET drv8353-current-zero.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-current-zero.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-current-zero.elf>
            ${CMAKE_BINARY_DIR}/drv8353-current-zero.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-current-zero.elf>
            ${CMAKE_BINARY_DIR}/drv8353-current-zero.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-current-zero.elf>
)

# Manual Hall GPIO reader: ENABLE always low, with no DRV/PWM/control modules.
add_executable(hall-bringup.elf
    src/hall_bringup_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(HALL_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM HALL_LINK_OPTIONS REPLACE "drv8353-bringup.map" "hall-bringup.map")
target_include_directories(hall-bringup.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(hall-bringup.elf PRIVATE STM32F411xE)
target_compile_options(hall-bringup.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(hall-bringup.elf PRIVATE ${HALL_LINK_OPTIONS})
target_link_libraries(hall-bringup.elf PRIVATE gcc)
set_property(TARGET hall-bringup.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET hall-bringup.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:hall-bringup.elf>
            ${CMAKE_BINARY_DIR}/hall-bringup.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:hall-bringup.elf>
            ${CMAKE_BINARY_DIR}/hall-bringup.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:hall-bringup.elf>
)

# Nucleo logic-pin scope check; power-stage ENABLE stays low throughout.
add_executable(pwm-bringup.elf
    src/pwm_bringup_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(PWM_BRINGUP_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM PWM_BRINGUP_LINK_OPTIONS REPLACE "drv8353-bringup.map" "pwm-bringup.map")
target_include_directories(pwm-bringup.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(pwm-bringup.elf PRIVATE STM32F411xE)
target_compile_options(pwm-bringup.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(pwm-bringup.elf PRIVATE ${PWM_BRINGUP_LINK_OPTIONS})
target_link_libraries(pwm-bringup.elf PRIVATE gcc)
set_property(TARGET pwm-bringup.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET pwm-bringup.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:pwm-bringup.elf>
            ${CMAKE_BINARY_DIR}/pwm-bringup.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:pwm-bringup.elf>
            ${CMAKE_BINARY_DIR}/pwm-bringup.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:pwm-bringup.elf>
)

# Motor-disconnected low-side gate scope test; bounded SPI and EXTI shutdown.
add_executable(drv8353-gate.elf
    src/drv8353_gate_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_GATE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_GATE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-gate.map")
target_include_directories(drv8353-gate.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-gate.elf PRIVATE STM32F411xE)
target_compile_options(drv8353-gate.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-gate.elf PRIVATE ${DRV_GATE_LINK_OPTIONS})
target_link_libraries(drv8353-gate.elf PRIVATE gcc)
set_property(TARGET drv8353-gate.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-gate.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-gate.elf>
            ${CMAKE_BINARY_DIR}/drv8353-gate.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-gate.elf>
            ${CMAKE_BINARY_DIR}/drv8353-gate.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-gate.elf>
)

# Original INHA/B/C wiring plus software-controlled INL enables; no motor.
add_executable(drv8353-bridge.elf
    src/drv8353_gate_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_BRIDGE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_BRIDGE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-bridge.map")
target_include_directories(drv8353-bridge.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-bridge.elf PRIVATE STM32F411xE DRV_GATE_3PWM)
target_compile_options(drv8353-bridge.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-bridge.elf PRIVATE ${DRV_BRIDGE_LINK_OPTIONS})
target_link_libraries(drv8353-bridge.elf PRIVATE gcc)
set_property(TARGET drv8353-bridge.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-bridge.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-bridge.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-bridge.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-bridge.elf>
)

# Same protected three-PWM bridge, with fresh COAST zeros and current ADC logs.
# Observation only: no motor, current loop, or ampere protection threshold.
add_executable(drv8353-bridge-adc.elf
    src/drv8353_gate_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_BRIDGE_ADC_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_BRIDGE_ADC_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-bridge-adc.map")
target_include_directories(drv8353-bridge-adc.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-bridge-adc.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK)
target_compile_options(drv8353-bridge-adc.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-bridge-adc.elf PRIVATE ${DRV_BRIDGE_ADC_LINK_OPTIONS})
target_link_libraries(drv8353-bridge-adc.elf PRIVATE gcc)
set_property(TARGET drv8353-bridge-adc.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-bridge-adc.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-bridge-adc.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge-adc.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-bridge-adc.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge-adc.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-bridge-adc.elf>
)

# One injected three-channel ADC sequence per PWM period, with scope markers.
add_executable(drv8353-bridge-sync.elf
    src/drv8353_gate_main.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_SYNC_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_SYNC_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-bridge-sync.map")
target_include_directories(drv8353-bridge-sync.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-bridge-sync.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC)
target_compile_options(drv8353-bridge-sync.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-bridge-sync.elf PRIVATE ${DRV_SYNC_LINK_OPTIONS})
target_link_libraries(drv8353-bridge-sync.elf PRIVATE gcc)
set_property(TARGET drv8353-bridge-sync.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-bridge-sync.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-bridge-sync.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge-sync.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-bridge-sync.elf>
            ${CMAKE_BINARY_DIR}/drv8353-bridge-sync.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-bridge-sync.elf>
)

# Bounded stationary motor probe: 100mA nominal,2% drive,250ms per pair.
add_executable(drv8353-motor-probe.elf
    src/drv8353_gate_main.c
    src/motor_probe.c
    src/motor_probe_trace.c
    src/motor_probe_regular.c
    src/motor_probe_pwm.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_MOTOR_PROBE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_PROBE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-probe.map")
target_include_directories(drv8353-motor-probe.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-probe.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST)
target_compile_options(drv8353-motor-probe.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-probe.elf PRIVATE ${DRV_MOTOR_PROBE_LINK_OPTIONS})
target_link_libraries(drv8353-motor-probe.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-probe.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-probe.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-probe.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-probe.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-probe.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-probe.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-probe.elf>
)

# Reverse-current validation followed by one slow stepped magnetic-field turn.
add_executable(drv8353-motor-spin.elf
    src/drv8353_gate_main.c
    src/motor_probe.c
    src/motor_probe_trace.c
    src/motor_probe_regular.c
    src/motor_probe_pwm.c
    src/motor_probe_sequence.c
    src/clock.c
    src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s
)
get_target_property(DRV_MOTOR_SPIN_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_SPIN_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-spin.map")
target_include_directories(drv8353-motor-spin.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-spin.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST)
target_compile_options(drv8353-motor-spin.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-spin.elf PRIVATE ${DRV_MOTOR_SPIN_LINK_OPTIONS})
target_link_libraries(drv8353-motor-spin.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-spin.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-spin.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-spin.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-spin.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-spin.elf>
)

# Separate low-current torque trial; keep both 100mA baseline images unchanged.
get_target_property(DRV_MOTOR_SPIN_SOURCES drv8353-motor-spin.elf SOURCES)
add_executable(drv8353-motor-spin-250ma.elf ${DRV_MOTOR_SPIN_SOURCES})
get_target_property(DRV_MOTOR_SPIN_250_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_SPIN_250_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-spin-250ma.map")
target_include_directories(drv8353-motor-spin-250ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-spin-250ma.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST DRV_GATE_CRAWL_250MA DRV_GATE_HIZ_SPI)
target_compile_options(drv8353-motor-spin-250ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-spin-250ma.elf PRIVATE ${DRV_MOTOR_SPIN_250_LINK_OPTIONS})
target_link_libraries(drv8353-motor-spin-250ma.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-spin-250ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-spin-250ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-spin-250ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin-250ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-spin-250ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin-250ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-spin-250ma.elf>
)

# Next bounded torque step, retaining all earlier images and trip thresholds.
add_executable(drv8353-motor-spin-350ma.elf ${DRV_MOTOR_SPIN_SOURCES})
get_target_property(DRV_MOTOR_SPIN_350_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_SPIN_350_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-spin-350ma.map")
target_include_directories(drv8353-motor-spin-350ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-spin-350ma.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST DRV_GATE_CRAWL_350MA DRV_GATE_HIZ_SPI)
target_compile_options(drv8353-motor-spin-350ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-spin-350ma.elf PRIVATE ${DRV_MOTOR_SPIN_350_LINK_OPTIONS})
target_link_libraries(drv8353-motor-spin-350ma.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-spin-350ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-spin-350ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-spin-350ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin-350ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-spin-350ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-spin-350ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-spin-350ma.elf>
)

# Continuous field rotation, no finite-run cutoff or programmed coast gaps.
add_executable(drv8353-motor-continuous.elf ${DRV_MOTOR_SPIN_SOURCES} src/motor_continuous.c)
get_target_property(DRV_MOTOR_CONTINUOUS_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_CONTINUOUS_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-continuous.map")
target_include_directories(drv8353-motor-continuous.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-continuous.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST DRV_GATE_CRAWL_350MA DRV_GATE_HIZ_SPI DRV_GATE_CONTINUOUS)
target_compile_options(drv8353-motor-continuous.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-continuous.elf PRIVATE ${DRV_MOTOR_CONTINUOUS_LINK_OPTIONS})
target_link_libraries(drv8353-motor-continuous.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-continuous.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-continuous.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-continuous.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-continuous.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-continuous.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-continuous.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-continuous.elf>
)

# Smooth phase-current blends, preserving the stepped continuous baseline.
add_executable(drv8353-motor-smooth.elf ${DRV_MOTOR_SPIN_SOURCES} src/motor_smooth.c)
get_target_property(DRV_MOTOR_SMOOTH_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_SMOOTH_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-smooth.map")
target_include_directories(drv8353-motor-smooth.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-smooth.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST DRV_GATE_CRAWL_350MA DRV_GATE_HIZ_SPI DRV_GATE_CONTINUOUS DRV_GATE_SMOOTH)
target_compile_options(drv8353-motor-smooth.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-smooth.elf PRIVATE ${DRV_MOTOR_SMOOTH_LINK_OPTIONS})
target_link_libraries(drv8353-motor-smooth.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-smooth.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-smooth.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-smooth.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-smooth.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-smooth.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-smooth.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-smooth.elf>
)


# Slow higher-torque trial using the shared repo PI, preserving every earlier image.
add_executable(drv8353-motor-torque-700ma.elf ${DRV_MOTOR_SPIN_SOURCES} src/motor_smooth.c src/control.c)
get_target_property(DRV_MOTOR_TORQUE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM DRV_MOTOR_TORQUE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "drv8353-motor-torque-700ma.map")
target_include_directories(drv8353-motor-torque-700ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(drv8353-motor-torque-700ma.elf PRIVATE STM32F411xE DRV_GATE_3PWM DRV_GATE_ADC_CHECK DRV_GATE_SYNC DRV_GATE_MOTOR_TEST DRV_GATE_SPIN_TEST DRV_GATE_TORQUE_700MA DRV_GATE_HIZ_SPI DRV_GATE_CONTINUOUS DRV_GATE_SMOOTH)
target_compile_options(drv8353-motor-torque-700ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(drv8353-motor-torque-700ma.elf PRIVATE ${DRV_MOTOR_TORQUE_LINK_OPTIONS})
target_link_libraries(drv8353-motor-torque-700ma.elf PRIVATE gcc)
set_property(TARGET drv8353-motor-torque-700ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET drv8353-motor-torque-700ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:drv8353-motor-torque-700ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-torque-700ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:drv8353-motor-torque-700ma.elf>
            ${CMAKE_BINARY_DIR}/drv8353-motor-torque-700ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:drv8353-motor-torque-700ma.elf>
)


# Reuse the working motor stack directly; this entry point only adapts the bench.
add_executable(repo-motor.elf
    src/repo_motor_main.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_MOTOR_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_MOTOR_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-motor.map")
target_include_directories(repo-motor.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-motor.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH)
target_compile_options(repo-motor.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-motor.elf PRIVATE ${REPO_MOTOR_LINK_OPTIONS})
target_link_libraries(repo-motor.elf PRIVATE gcc m)
set_property(TARGET repo-motor.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-motor.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-motor.elf> ${CMAKE_BINARY_DIR}/repo-motor.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-motor.elf> ${CMAKE_BINARY_DIR}/repo-motor.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-motor.elf>)

# Count exactly ten electrical turns using the existing open_loop_step implementation.
add_executable(repo-pole-count.elf
    src/repo_motor_main.c src/repo_pole_count.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_POLE_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_POLE_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-pole-count.map")
target_include_directories(repo-pole-count.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-pole-count.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_COUNT_POLES)
target_compile_options(repo-pole-count.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-pole-count.elf PRIVATE ${REPO_POLE_LINK_OPTIONS})
target_link_libraries(repo-pole-count.elf PRIVATE gcc m)
set_property(TARGET repo-pole-count.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-pole-count.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-pole-count.elf> ${CMAKE_BINARY_DIR}/repo-pole-count.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-pole-count.elf> ${CMAKE_BINARY_DIR}/repo-pole-count.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-pole-count.elf>)

# Same counted test at twice the current, with a matching board ADC sample-time setting.
add_executable(repo-pole-count-1400ma.elf
    src/repo_motor_main.c src/repo_pole_count.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_POLE_1400_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_POLE_1400_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-pole-count-1400ma.map")
target_include_directories(repo-pole-count-1400ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-pole-count-1400ma.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_COUNT_POLES REPO_DOUBLE_CURRENT)
target_compile_options(repo-pole-count-1400ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-pole-count-1400ma.elf PRIVATE ${REPO_POLE_1400_LINK_OPTIONS})
target_link_libraries(repo-pole-count-1400ma.elf PRIVATE gcc m)
set_property(TARGET repo-pole-count-1400ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-pole-count-1400ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-pole-count-1400ma.elf> ${CMAKE_BINARY_DIR}/repo-pole-count-1400ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-pole-count-1400ma.elf> ${CMAKE_BINARY_DIR}/repo-pole-count-1400ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-pole-count-1400ma.elf>)

# Repeating single-revolution forward/back command, using the existing cosine/sine/PI path.
add_executable(repo-reciprocal-2000ma.elf
    src/repo_motor_main.c src/repo_reciprocate.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_RECIP_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_RECIP_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-reciprocal-2000ma.map")
target_include_directories(repo-reciprocal-2000ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-reciprocal-2000ma.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT)
target_compile_options(repo-reciprocal-2000ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-reciprocal-2000ma.elf PRIVATE ${REPO_RECIP_LINK_OPTIONS})
target_link_libraries(repo-reciprocal-2000ma.elf PRIVATE gcc m)
set_property(TARGET repo-reciprocal-2000ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-reciprocal-2000ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-reciprocal-2000ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-2000ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-reciprocal-2000ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-2000ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-reciprocal-2000ma.elf>)

# Same reciprocal motion at2.5A; preserve the verified2A image.
add_executable(repo-reciprocal-2500ma.elf
    src/repo_motor_main.c src/repo_reciprocate.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_RECIP_2500_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_RECIP_2500_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-reciprocal-2500ma.map")
target_include_directories(repo-reciprocal-2500ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-reciprocal-2500ma.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_2500MA)
target_compile_options(repo-reciprocal-2500ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-reciprocal-2500ma.elf PRIVATE ${REPO_RECIP_2500_LINK_OPTIONS})
target_link_libraries(repo-reciprocal-2500ma.elf PRIVATE gcc m)
set_property(TARGET repo-reciprocal-2500ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-reciprocal-2500ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-reciprocal-2500ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-2500ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-reciprocal-2500ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-2500ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-reciprocal-2500ma.elf>)

# Same slow reciprocal motion at5A;20V/V current-sense range.
add_executable(repo-reciprocal-5000ma.elf
    src/repo_motor_main.c src/repo_reciprocate.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_RECIP_5000_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_RECIP_5000_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-reciprocal-5000ma.map")
target_include_directories(repo-reciprocal-5000ma.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-reciprocal-5000ma.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_5000MA SHUNT_GAIN=20.0f)
target_compile_options(repo-reciprocal-5000ma.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-reciprocal-5000ma.elf PRIVATE ${REPO_RECIP_5000_LINK_OPTIONS})
target_link_libraries(repo-reciprocal-5000ma.elf PRIVATE gcc m)
set_property(TARGET repo-reciprocal-5000ma.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-reciprocal-5000ma.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-reciprocal-5000ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-5000ma.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-reciprocal-5000ma.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-5000ma.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-reciprocal-5000ma.elf>)

# Same verified5A profile, twice trajectory speed (10s per leg).
add_executable(repo-reciprocal-5000ma-2xspeed.elf
    src/repo_motor_main.c src/repo_reciprocate.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_RECIP_5000_2X_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_RECIP_5000_2X_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-reciprocal-5000ma-2xspeed.map")
target_include_directories(repo-reciprocal-5000ma-2xspeed.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-reciprocal-5000ma-2xspeed.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_5000MA REPO_RECIP_DOUBLE_SPEED SHUNT_GAIN=20.0f)
target_compile_options(repo-reciprocal-5000ma-2xspeed.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-reciprocal-5000ma-2xspeed.elf PRIVATE ${REPO_RECIP_5000_2X_LINK_OPTIONS})
target_link_libraries(repo-reciprocal-5000ma-2xspeed.elf PRIVATE gcc m)
set_property(TARGET repo-reciprocal-5000ma-2xspeed.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-reciprocal-5000ma-2xspeed.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-reciprocal-5000ma-2xspeed.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-5000ma-2xspeed.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-reciprocal-5000ma-2xspeed.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-5000ma-2xspeed.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-reciprocal-5000ma-2xspeed.elf>)

#6.25A and5s per leg; retain the gain20 configuration path.
add_executable(repo-reciprocal-6250ma-4xspeed.elf
    src/repo_motor_main.c src/repo_reciprocate.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
get_target_property(REPO_RECIP_6250_4X_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_RECIP_6250_4X_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-reciprocal-6250ma-4xspeed.map")
target_include_directories(repo-reciprocal-6250ma-4xspeed.elf PRIVATE ${DRV_BRINGUP_INCLUDES})
target_compile_definitions(repo-reciprocal-6250ma-4xspeed.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_5000MA REPO_RECIP_QUAD_SPEED REPO_CURRENT_6250MA SHUNT_GAIN=20.0f)
target_compile_options(repo-reciprocal-6250ma-4xspeed.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-reciprocal-6250ma-4xspeed.elf PRIVATE ${REPO_RECIP_6250_4X_LINK_OPTIONS})
target_link_libraries(repo-reciprocal-6250ma-4xspeed.elf PRIVATE gcc m)
set_property(TARGET repo-reciprocal-6250ma-4xspeed.elf APPEND PROPERTY LINK_DEPENDS ${LDSCRIPT})
add_custom_command(TARGET repo-reciprocal-6250ma-4xspeed.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-reciprocal-6250ma-4xspeed.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-6250ma-4xspeed.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-reciprocal-6250ma-4xspeed.elf> ${CMAKE_BINARY_DIR}/repo-reciprocal-6250ma-4xspeed.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-reciprocal-6250ma-4xspeed.elf>)

#Live current tuning reuses the verified board entry,PI,PWM and ADC.
set(REPO_TUNING_SOURCES
    src/repo_motor_main.c src/repo_reciprocate.c src/current_tuning.c src/repo_tune_mcuviewer.c
    src/tune_gain_store.c src/tune_gain_store_stm32.c
    third_party/mcuviewer_recorder/recorder.c src/loops.c src/pwm.c src/current_feedback.c
    src/control.c src/drv8353.c src/clock.c src/system_stm32f4xx.c
    startup/startup_stm32f411xe.s)
add_executable(repo-current-tuning.elf ${REPO_TUNING_SOURCES})
get_target_property(REPO_TUNING_LINK_OPTIONS drv8353-bringup.elf LINK_OPTIONS)
list(TRANSFORM REPO_TUNING_LINK_OPTIONS REPLACE "drv8353-bringup.map" "repo-current-tuning.map")
list(TRANSFORM REPO_TUNING_LINK_OPTIONS REPLACE "memory.ld" "memory-tuning.ld")
target_include_directories(repo-current-tuning.elf PRIVATE ${DRV_BRINGUP_INCLUDES} third_party/mcuviewer_recorder)
target_compile_definitions(repo-current-tuning.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_5000MA REPO_RECIP_QUAD_SPEED REPO_CURRENT_6250MA REPO_CURRENT_TUNING REPO_MCUVIEWER SHUNT_GAIN=20.0f)
target_compile_options(repo-current-tuning.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
# Optimize only the new RAM/capture adapter to bound its IRQ-masked copy.
# Original motor modules retain their existing build options.
set_source_files_properties(src/repo_tune_mcuviewer.c PROPERTIES COMPILE_OPTIONS "-O2")
target_link_options(repo-current-tuning.elf PRIVATE ${REPO_TUNING_LINK_OPTIONS})
target_link_libraries(repo-current-tuning.elf PRIVATE gcc m)
set_property(TARGET repo-current-tuning.elf APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker/memory-tuning.ld)
add_custom_command(TARGET repo-current-tuning.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-current-tuning.elf> ${CMAKE_BINARY_DIR}/repo-current-tuning.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-current-tuning.elf> ${CMAKE_BINARY_DIR}/repo-current-tuning.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-current-tuning.elf>)

#Keep the 12V standalone profile intact; this separate GEMS tuning image uses
#24V bus normalization/checks and permits fixed tones through 2kHz.
add_executable(repo-current-tuning-24v.elf ${REPO_TUNING_SOURCES})
get_target_property(REPO_TUNING_24V_LINK_OPTIONS repo-current-tuning.elf LINK_OPTIONS)
list(TRANSFORM REPO_TUNING_24V_LINK_OPTIONS REPLACE "repo-current-tuning.map" "repo-current-tuning-24v.map")
target_include_directories(repo-current-tuning-24v.elf PRIVATE ${DRV_BRINGUP_INCLUDES} third_party/mcuviewer_recorder)
target_compile_definitions(repo-current-tuning-24v.elf PRIVATE STM32F411xE REPO_MOTOR_BENCH REPO_RECIPROCAL REPO_DOUBLE_CURRENT REPO_CURRENT_5000MA REPO_RECIP_QUAD_SPEED REPO_CURRENT_6250MA REPO_CURRENT_TUNING REPO_MCUVIEWER REPO_TUNING_24V SHUNT_GAIN=20.0f)
target_compile_options(repo-current-tuning-24v.elf PRIVATE ${DRV_BRINGUP_COMPILE_OPTIONS})
target_link_options(repo-current-tuning-24v.elf PRIVATE ${REPO_TUNING_24V_LINK_OPTIONS})
target_link_libraries(repo-current-tuning-24v.elf PRIVATE gcc m)
set_property(TARGET repo-current-tuning-24v.elf APPEND PROPERTY LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker/memory-tuning.ld)
add_custom_command(TARGET repo-current-tuning-24v.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:repo-current-tuning-24v.elf> ${CMAKE_BINARY_DIR}/repo-current-tuning-24v.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:repo-current-tuning-24v.elf> ${CMAKE_BINARY_DIR}/repo-current-tuning-24v.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:repo-current-tuning-24v.elf>)
