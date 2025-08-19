cmake_minimum_required(VERSION 3.12)

set(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB HOST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/host*.c*")

if (${PICO_BOARD} STREQUAL "pico2")
  # Increase the CPU frequency to 150MHz for the pico2 board
  add_definitions(-DCPU_FREQ_KHZ=150000)
endif()

#
# host
#

add_executable(host ${HOST_SRC})
pico_add_extra_outputs(host)
target_link_libraries(host
  PRIVATE
    pico_multicore
    hal-MapleBus
    hal-System
    hal-Usb-Client
    pico_stdio_usb
    hostLib
)
target_compile_options(host PRIVATE
  -Wall
  -Werror
  -O3
)

target_include_directories(host
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/host>"
    "${PROJECT_SOURCE_DIR}/inc")


#
# zero_host
#

if ("${PICO_BOARD}" STREQUAL "pico")

  add_executable(zero_host ${HOST_SRC})
  pico_add_extra_outputs(zero_host)
  target_link_libraries(zero_host
    PRIVATE
      pico_multicore
      hal-MapleBus
      hal-System
      hal-Usb-Client
      pico_stdio_usb
      hostLib
  )
  target_compile_options(zero_host PRIVATE
    -Wall
    -Werror
    -O3
  )
  target_compile_definitions(zero_host
    PUBLIC
      SELECTED_NUMBER_OF_DEVICES=4
      P3_BUS_START_PIN=2
      P4_BUS_START_PIN=4
      )

  target_include_directories(zero_host
    PRIVATE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zero_host>"
      "${PROJECT_SOURCE_DIR}/inc")

endif()
