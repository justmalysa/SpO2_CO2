# SPDX-License-Identifier: Apache-2.0

list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/max30102
  ${CMAKE_CURRENT_SOURCE_DIR}/stc31
  )

set(EXTRA_MODULES_PATHS ${ZEPHYR_EXTRA_MODULES})
list(TRANSFORM EXTRA_MODULES_PATHS APPEND "/zephyr")
list(APPEND DTS_ROOT ${EXTRA_MODULES_PATHS})

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spo2_co2_fw)

target_sources(app PRIVATE
               src/main.c
               src/display.c
               src/button.c
               src/spo2.c
               src/co2.c)