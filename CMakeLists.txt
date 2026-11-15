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

if(CHIP STREQUAL "bl702")
# Build the BL702 USB device driver from src/ instead of the SDK copy, so
# fixes to it live in this repository.
get_target_property(lhal_sources liblhal SOURCES)
list(FILTER lhal_sources EXCLUDE REGEX "/bflb_usb_v1\\.c$")
set_property(TARGET liblhal PROPERTY SOURCES ${lhal_sources})
target_sources(app PRIVATE src/bflb_usb_v1.c)
endif()

sdk_set_main_file(src/main.c)

project(bl616_dirtyjtag)
