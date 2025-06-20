cmake_minimum_required(VERSION 3.12)

set(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB HOST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/host*.c*")

if (${PICO_BOARD} STREQUAL "pico2")
  # Increase the CPU frequency to 150MHz for the pico2 board
  add_definitions(-DCPU_FREQ_KHZ=150000)
endif()

#
# host-4p
#

add_executable(host-4p ${HOST_SRC})
pico_add_extra_outputs(host-4p)
target_link_libraries(host-4p
  PRIVATE
    pico_multicore
    hal-MapleBus
    hal-System
    hal-Usb-Client
    pico_stdio_usb
    hostLib
)
target_compile_options(host-4p PRIVATE
  -Wall
  -Werror
  -O3
)
target_compile_definitions(host-4p PUBLIC SELECTED_NUMBER_OF_DEVICES=4)

target_include_directories(host-4p
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/host-4p>"
    "${PROJECT_SOURCE_DIR}/inc")

#
# host-2p
#

add_executable(host-2p ${HOST_SRC})
pico_add_extra_outputs(host-2p)
target_link_libraries(host-2p
  PRIVATE
    pico_multicore
    hal-MapleBus
    hal-System
    hal-Usb-Client
    pico_stdio_usb
    hostLib
)
target_compile_options(host-2p PRIVATE
  -Wall
  -Werror
  -O3
)
target_compile_definitions(host-2p PUBLIC SELECTED_NUMBER_OF_DEVICES=2)

target_include_directories(host-2p
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/host-4p>"
    "${PROJECT_SOURCE_DIR}/inc")

#
# host-1p
#

add_executable(host-1p ${HOST_SRC})
pico_add_extra_outputs(host-1p)
target_link_libraries(host-1p
  PRIVATE
    pico_multicore
    hal-MapleBus
    hal-System
    hal-Usb-Client
    pico_stdio_usb
    hostLib
)
target_compile_options(host-1p PRIVATE
  -Wall
  -Werror
  -O3
)
target_compile_definitions(host-1p PUBLIC SELECTED_NUMBER_OF_DEVICES=1)

target_include_directories(host-1p
  PRIVATE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/host-1p>"
    "${PROJECT_SOURCE_DIR}/inc")


#
# zero_host-4p
#

if ("${PICO_BOARD}" STREQUAL "pico")

  add_executable(zero_host-4p ${HOST_SRC})
  pico_add_extra_outputs(zero_host-4p)
  target_link_libraries(zero_host-4p
    PRIVATE
      pico_multicore
      hal-MapleBus
      hal-System
      hal-Usb-Client
      pico_stdio_usb
      hostLib
  )
  target_compile_options(zero_host-4p PRIVATE
    -Wall
    -Werror
    -O3
  )
  target_compile_definitions(zero_host-4p
    PUBLIC
      SELECTED_NUMBER_OF_DEVICES=4
      P3_BUS_START_PIN=2
      P4_BUS_START_PIN=4
      )

  target_include_directories(zero_host-4p
    PRIVATE
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/zero_host-4p>"
      "${PROJECT_SOURCE_DIR}/inc")

endif()
