# SPDX-FileCopyrightText: 2026 ID8 Engineering AB
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

if(USE_DUMMY_CREDS)
  set(creds "src/creds/dummy.c")
else()
  if(NOT EXISTS ${APPLICATION_SOURCE_DIR}/src/creds/key.c OR
     NOT EXISTS ${APPLICATION_SOURCE_DIR}/src/creds/cert.c OR
     NOT EXISTS ${APPLICATION_SOURCE_DIR}/src/creds/ca.c)
    message(FATAL_ERROR
        "Credentials not found. From the root repository root, run "
        "'python3 app/src/creds/convert_keys.py' before building"
    )
  endif()

  set(creds "src/creds/ca.c" "src/creds/key.c" "src/creds/cert.c")
endif()

set(helpers "src/sensor.c" "src/modem.c" "src/aws.c")

target_sources(app PRIVATE src/main.c ${helpers} ${creds})
