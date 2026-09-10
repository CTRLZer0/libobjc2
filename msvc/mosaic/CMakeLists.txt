# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md.
cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(mosaic_libobjc2 LANGUAGES C)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic libobjc2 host runtime currently targets Windows")
endif()

set(MOSAIC_LIBOBJC2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
option(MOSAIC_LIBOBJC2_WARNINGS_AS_ERRORS
    "Treat Mosaic contract warnings as errors" OFF)
option(MOSAIC_LIBOBJC2_BUILD_BENCHMARKS
    "Build Mosaic runtime microbenchmarks" OFF)

set(MOSAIC_LIBOBJC2_CLANG "" CACHE FILEPATH
    "clang used for the isolated GNUstep runtime build")
set(MOSAIC_LIBOBJC2_CLANGXX "" CACHE FILEPATH
    "clang++ used for the isolated GNUstep runtime build")
set(MOSAIC_LIBOBJC2_CLANG_CL "" CACHE FILEPATH
    "Legacy clang-cl hint; its sibling clang.exe is used when provided")
if(NOT MOSAIC_LIBOBJC2_CLANG AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    get_filename_component(_parent_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
    set(_parent_clang "${_parent_compiler_dir}/clang.exe")
    if(EXISTS "${_parent_clang}")
        set(MOSAIC_LIBOBJC2_CLANG "${_parent_clang}" CACHE FILEPATH "" FORCE)
    endif()
endif()

if(NOT MOSAIC_LIBOBJC2_CLANG AND MOSAIC_LIBOBJC2_CLANG_CL)
    get_filename_component(_clang_dir "${MOSAIC_LIBOBJC2_CLANG_CL}" DIRECTORY)
    set(_clang_candidate "${_clang_dir}/clang.exe")
    if(EXISTS "${_clang_candidate}")
        set(MOSAIC_LIBOBJC2_CLANG "${_clang_candidate}" CACHE FILEPATH "" FORCE)
    endif()
endif()

if(NOT MOSAIC_LIBOBJC2_CLANG)
    find_program(MOSAIC_LIBOBJC2_CLANG NAMES clang clang.exe)
endif()
if(NOT MOSAIC_LIBOBJC2_CLANGXX AND MOSAIC_LIBOBJC2_CLANG)
    get_filename_component(_clang_dir "${MOSAIC_LIBOBJC2_CLANG}" DIRECTORY)
    set(_clangxx_candidate "${_clang_dir}/clang++.exe")
    if(EXISTS "${_clangxx_candidate}")
        set(MOSAIC_LIBOBJC2_CLANGXX "${_clangxx_candidate}" CACHE FILEPATH "" FORCE)
    endif()
endif()
if(NOT MOSAIC_LIBOBJC2_CLANGXX)
    find_program(MOSAIC_LIBOBJC2_CLANGXX NAMES clang++ clang++.exe)
endif()

foreach(_compiler MOSAIC_LIBOBJC2_CLANG MOSAIC_LIBOBJC2_CLANGXX)
    if(NOT ${_compiler} OR NOT EXISTS "${${_compiler}}")
        message(FATAL_ERROR "${_compiler} was not found; LLVM 23.1.1 or newer is required")
    endif()
endforeach()
execute_process(
    COMMAND "${MOSAIC_LIBOBJC2_CLANG}" --version
    OUTPUT_VARIABLE MOSAIC_LIBOBJC2_CLANG_VERSION_TEXT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _clang_version_result)
if(NOT _clang_version_result EQUAL 0)
    message(FATAL_ERROR "Unable to execute ${MOSAIC_LIBOBJC2_CLANG}")
endif()
string(REGEX MATCH "clang version ([0-9]+\.[0-9]+\.[0-9]+)"
    _clang_match "${MOSAIC_LIBOBJC2_CLANG_VERSION_TEXT}")
set(MOSAIC_LIBOBJC2_CLANG_VERSION "${CMAKE_MATCH_1}")
if(NOT MOSAIC_LIBOBJC2_CLANG_VERSION OR
   MOSAIC_LIBOBJC2_CLANG_VERSION VERSION_LESS "23.1.1")
    message(FATAL_ERROR
        "Mosaic libobjc2 requires LLVM 23.1.1 or newer; found ${MOSAIC_LIBOBJC2_CLANG_VERSION}")
endif()
message(STATUS
    "Mosaic GNUstep libobjc2 compiler: ${MOSAIC_LIBOBJC2_CLANG} (${MOSAIC_LIBOBJC2_CLANG_VERSION})")

set(MOSAIC_LIBOBJC2_NINJA "" CACHE FILEPATH
    "Ninja executable used for the isolated GNUstep runtime build")
if(NOT MOSAIC_LIBOBJC2_NINJA AND CMAKE_GENERATOR MATCHES "Ninja")
    set(MOSAIC_LIBOBJC2_NINJA "${CMAKE_MAKE_PROGRAM}" CACHE FILEPATH "" FORCE)
endif()
if(NOT MOSAIC_LIBOBJC2_NINJA)
    find_program(MOSAIC_LIBOBJC2_NINJA NAMES ninja ninja.exe)
endif()
if(NOT MOSAIC_LIBOBJC2_NINJA)
    file(GLOB _vs_ninja
        "C:/Program Files/Microsoft Visual Studio/20*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
        "C:/Program Files (x86)/Microsoft Visual Studio/20*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")
    list(LENGTH _vs_ninja _vs_ninja_count)
    if(_vs_ninja_count GREATER 0)
        list(GET _vs_ninja 0 _ninja_candidate)
        set(MOSAIC_LIBOBJC2_NINJA "${_ninja_candidate}" CACHE FILEPATH "" FORCE)
    endif()
endif()
if(NOT MOSAIC_LIBOBJC2_NINJA OR NOT EXISTS "${MOSAIC_LIBOBJC2_NINJA}")
    message(FATAL_ERROR "Ninja is required for the isolated GNUstep runtime build")
endif()

if(CMAKE_CONFIGURATION_TYPES)
    set(_runtime_config "$<CONFIG>")
    set(_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}/runtime/$<CONFIG>")
else()
    if(CMAKE_BUILD_TYPE)
        set(_runtime_config "${CMAKE_BUILD_TYPE}")
    else()
        set(_runtime_config "Debug")
    endif()
    set(_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}/runtime/${_runtime_config}")
endif()
set(_runtime_archive "${_runtime_dir}/mosaic_objc_runtime.lib")
add_custom_target(mosaic_objc_runtime_build
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${MOSAIC_LIBOBJC2_ROOT}"
        "-DBINARY_DIR=${_runtime_dir}"
        "-DCONFIG=${_runtime_config}"
        "-DCLANG=${MOSAIC_LIBOBJC2_CLANG}"
        "-DCLANGXX=${MOSAIC_LIBOBJC2_CLANGXX}"
        "-DNINJA=${MOSAIC_LIBOBJC2_NINJA}"
        "-DWARNINGS_AS_ERRORS=${MOSAIC_LIBOBJC2_WARNINGS_AS_ERRORS}"
        -P "${CMAKE_CURRENT_LIST_DIR}/build-runtime.cmake"
    BYPRODUCTS "${_runtime_archive}"
    VERBATIM)

add_library(mosaic_objc_runtime_archive STATIC IMPORTED GLOBAL)
if(CMAKE_CONFIGURATION_TYPES)
    foreach(_config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${_config}" _config_upper)
        set_property(TARGET mosaic_objc_runtime_archive PROPERTY
            "IMPORTED_LOCATION_${_config_upper}"
            "${CMAKE_CURRENT_BINARY_DIR}/runtime/${_config}/mosaic_objc_runtime.lib")
    endforeach()
else()
    set_property(TARGET mosaic_objc_runtime_archive PROPERTY
        IMPORTED_LOCATION "${_runtime_archive}")
endif()

add_library(mosaic_objc_runtime INTERFACE)
add_dependencies(mosaic_objc_runtime mosaic_objc_runtime_build)
target_link_libraries(mosaic_objc_runtime INTERFACE mosaic_objc_runtime_archive ntdll)
target_compile_definitions(mosaic_objc_runtime INTERFACE __OBJC_RUNTIME_STATIC__=1)
target_include_directories(mosaic_objc_runtime INTERFACE
    "${MOSAIC_LIBOBJC2_ROOT}"
    "${_runtime_dir}"
    "${_runtime_dir}/objc")
add_library(Mosaic::ObjCRuntime ALIAS mosaic_objc_runtime)
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(
        "${MOSAIC_LIBOBJC2_ROOT}/tests/windows"
        "${CMAKE_CURRENT_BINARY_DIR}/tests-windows")
endif()
if(MOSAIC_LIBOBJC2_BUILD_BENCHMARKS)
    add_subdirectory(
        "${MOSAIC_LIBOBJC2_ROOT}/benchmarks/windows"
        "${CMAKE_CURRENT_BINARY_DIR}/benchmarks-windows")
endif()
