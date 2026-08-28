SDK_DEMO_PATH ?= $(abspath .)
BL_SDK_BASE ?= $(SDK_DEMO_PATH)/bouffalo_sdk

export BL_SDK_BASE

CHIP ?= bl616
BOARD ?= bl616dk
CROSS_COMPILE ?= riscv64-unknown-elf-

include $(BL_SDK_BASE)/project.build
