# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
# and provenance details.

foreach(_required SOURCE_DIR BINARY_DIR CONFIG CLANG CLANGXX NINJA)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "Missing ${_required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${BINARY_DIR}")
set(_configure
    -S "${SOURCE_DIR}"
    -B "${BINARY_DIR}"
    -G Ninja
    "-DCMAKE_MAKE_PROGRAM=${NINJA}"
    "-DCMAKE_C_COMPILER=${CLANG}"
    "-DCMAKE_CXX_COMPILER=${CLANGXX}"
    "-DCMAKE_OBJC_COMPILER=${CLANG}"
    "-DCMAKE_OBJCXX_COMPILER=${CLANGXX}"
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DCMAKE_COMPILE_WARNING_AS_ERROR=${WARNINGS_AS_ERRORS}"
    -DBUILD_SHARED_LIBOBJC=OFF
    -DBUILD_STATIC_LIBOBJC=ON
    -DTESTS=OFF
    -DLIBOBJC_NAME=mosaic_objc_runtime)

execute_process(COMMAND "${CMAKE_COMMAND}" ${_configure}
    RESULT_VARIABLE _configure_result)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR "GNUstep libobjc2 configure failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}"
        --target objc-static --parallel
    RESULT_VARIABLE _build_result)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR "GNUstep libobjc2 build failed")
endif()
