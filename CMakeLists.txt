cmake_minimum_required(VERSION 3.15)

set(CONFIG_FREERTOS 1)
set(CONFIG_CHERRYUSB 1)
set(CONFIG_CHERRYUSB_DEVICE 1)
set(CONFIG_CHERRYUSB_DEVICE_CDC_ACM 1)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_include_directories(. src)
if(NOT CHIP STREQUAL "bl702")
sdk_add_compile_definitions(-DCONFIG_USB_HS)
endif()

target_sources(app PRIVATE
    src/usb_descriptors.c
    src/dirtyjtag.c
    src/jtag_gpio.c
    src/uart_bridge.c
)

sdk_set_main_file(src/main.c)

project(bl616_dirtyjtag)
