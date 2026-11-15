# Host-side regression tests. No SDK, no hardware, no cross-compiler.
#
#   make test
#
# Compiles the real firmware sources against small fakes. Only the CherryUSB
# headers are used from the SDK; those are plain C structs and macros with no
# chip dependencies.

cmake_minimum_required(VERSION 3.15)
project(dirtyjtag_host_tests C)

enable_testing()
add_compile_options(-Wall -Wextra -Wno-unused-parameter -g)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(CU ${CMAKE_CURRENT_SOURCE_DIR}/../bouffalo_sdk/components/usb/cherryusb)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# usb_config.h comes from tests/ (it neutralises .noncacheable so ASan can
# see buffer overruns), hence tests/ precedes ROOT.
set(INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SRC}
    ${ROOT}
    ${CU}/common
    ${CU}/core
    ${CU}/class/cdc
)

# jtag_gpio.c and uart_bridge.c are replaced by fakes: these tests cover
# command parsing and descriptors, not the pin driver.
add_library(fakes STATIC fakes.c)
target_include_directories(fakes PUBLIC ${INCS})

function(host_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE ${INCS})
  target_link_libraries(${name} fakes)
  # ASan turns buffer overruns in the firmware into hard failures.
  target_compile_options(${name} PRIVATE -fsanitize=address)
  target_link_options(${name} PRIVATE -fsanitize=address)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

host_test(test_protocol test_protocol.c ${SRC}/dirtyjtag.c)

# Known-bug probe: ASan aborts, so it runs in its own binary. Currently
# fails, reproducing the tx_buf overrun in cmd_handle().
host_test(test_tx_buf_bounds test_tx_buf_bounds.c ${SRC}/dirtyjtag.c)

# Pure data — no fakes needed beyond the desc_register capture.
host_test(test_descriptors test_descriptors.c ${SRC}/usb_descriptors.c)
