# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(main C CXX ASM)
pico_sdk_init()
add_executable(main main.c inc/ssd1306.c)
target_link_libraries(main pico_stdlib hardware_adc hardware_pio hardware_pwm hardware_i2c)
pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_generate_pio_header(main ${CMAKE_CURRENT_LIST_DIR}/pio_matrix.pio)

pico_add_extra_outputs(main)