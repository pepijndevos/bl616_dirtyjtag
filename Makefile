SDK_DEMO_PATH ?= .
BL_SDK_BASE ?= $(SDK_DEMO_PATH)/bouffalo_sdk

export BL_SDK_BASE

CHIP ?= bl702
BOARD ?= tang_primer_20k
CROSS_COMPILE ?= riscv64-unknown-elf-
BOARD_DIR := $(abspath src/board)

include $(BL_SDK_BASE)/project.build

# The linker script omits the fw_header section, so the .bin is raw code at XIP 0x23000000.
# blisp injects its own boot header at flash 0x0 (flashoffset=0x2000) and writes the
# payload at flash 0x2000, which maps to XIP 0x23000000.
upload: $(BUILD_DIR)
	blisp write -c bl70x -p $(PORT) --reset build/build_out/dirtyjtag_tangprimer20k_bl702.bin

# Host-side regression tests (no hardware, no cross-toolchain).
TEST_BUILD_DIR := build/tests

.PHONY: test
test:
	cmake -B $(TEST_BUILD_DIR) -S tests
	cmake --build $(TEST_BUILD_DIR)
	ctest --test-dir $(TEST_BUILD_DIR) --output-on-failure

.PHONY: test-clean
test-clean:
	rm -rf $(TEST_BUILD_DIR)
