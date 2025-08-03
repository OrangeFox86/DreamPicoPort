cmake_minimum_required(VERSION 3.12)

set(CMAKE_VERBOSE_MAKEFILE ON)

file(GLOB SRC "${CMAKE_CURRENT_SOURCE_DIR}/*.c*"
              "${CMAKE_CURRENT_SOURCE_DIR}/peripherals/*.c*"
              "${CMAKE_CURRENT_SOURCE_DIR}/tty_parsers/*.c*"
              "${CMAKE_CURRENT_SOURCE_DIR}/webusb_parsers/*.c*")

add_library(hostLib STATIC ${SRC})

if(NOT ENABLE_UNIT_TEST)
  target_compile_options(hostLib PRIVATE
    -Wall
    -Werror
    -O3
  )
endif()

target_include_directories(hostLib
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/core>"
    "${PROJECT_SOURCE_DIR}/inc"
    "${CMAKE_CURRENT_SOURCE_DIR}/peripherals"
    "${CMAKE_CURRENT_SOURCE_DIR}/tty_parsers"
    "${CMAKE_CURRENT_SOURCE_DIR}/webusb_parsers")

if (ENABLE_UNIT_TEST)
  add_subdirectory(test)
endif()