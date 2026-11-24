SDK_DEMO_PATH ?= $(abspath .)
BL_SDK_BASE ?= $(SDK_DEMO_PATH)/bouffalo_sdk

export BL_SDK_BASE

CHIP ?= bl616
BOARD ?= bl616dk
CROSS_COMPILE ?= riscv64-unknown-elf-

# Separate build dir per chip so both targets can coexist
ifeq ($(CHIP),bl702)
BUILD_DIR ?= build_bl702
endif

include $(BL_SDK_BASE)/project.build
