# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(mosaic_libobjc2 LANGUAGES C)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic libobjc2 host runtime currently targets Windows")
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Mosaic libobjc2 requires clang-cl on Windows")
endif()

set(MOSAIC_LIBOBJC2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(LIBOBJC2_SOURCE_ROOT "${MOSAIC_LIBOBJC2_ROOT}")
include("${MOSAIC_LIBOBJC2_ROOT}/CMake/RuntimeSources.cmake")
set(MOSAIC_LIBOBJC2_DEFINES
    WIN32 _WINDOWS NO_PTHREADS=1 GNUSTEP
    __OBJC_RUNTIME_INTERNAL__=1 NO_LEGACY
    WINOBJC_HASH_SELECTOR_TYPES
    MOSAIC_LIBOBJC2_C_DISPATCH
    LIBOBJC2_NO_ASM_DISPATCH)

set(MOSAIC_LIBOBJC2_C_SOURCES
    ${LIBOBJC2_RUNTIME_SOURCES}
    ${LIBOBJC2_ENCODING_SOURCES}
    ${LIBOBJC2_GC_NONE_SOURCE}
    ${LIBOBJC2_DISPATCH_SOURCES}
    "${MOSAIC_LIBOBJC2_ROOT}/src/platform/windows/block_to_imp_winobjc.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/platform/windows/hooks_mosaic.c")
list(REMOVE_ITEM MOSAIC_LIBOBJC2_C_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/src/runtime/hooks.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/dispatch/hash_table.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/dispatch/toydispatch.c")
list(APPEND MOSAIC_LIBOBJC2_C_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/src/memory/legacy_malloc.c")

set(MOSAIC_LIBOBJC2_OBJC_SOURCES
    ${LIBOBJC2_RUNTIME_OBJC_SOURCES}
    ${LIBOBJC2_MEMORY_OBJC_SOURCES}
    ${LIBOBJC2_BLOCK_OBJC_SOURCES})

set(MOSAIC_LIBOBJC2_INCLUDE_FLAGS)
list(APPEND MOSAIC_LIBOBJC2_INCLUDE_FLAGS "/I${MOSAIC_LIBOBJC2_ROOT}")
foreach(_include IN LISTS LIBOBJC2_INTERNAL_INCLUDE_DIRS)
    list(APPEND MOSAIC_LIBOBJC2_INCLUDE_FLAGS "/I${_include}")
endforeach()

set(MOSAIC_LIBOBJC2_OBJC_OBJECTS)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/objc")
set(MOSAIC_LIBOBJC2_DEFINE_FLAGS)
foreach(_definition IN LISTS MOSAIC_LIBOBJC2_DEFINES)
    list(APPEND MOSAIC_LIBOBJC2_DEFINE_FLAGS "/D${_definition}")
endforeach()

file(GLOB_RECURSE MOSAIC_LIBOBJC2_HEADERS CONFIGURE_DEPENDS
    "${MOSAIC_LIBOBJC2_ROOT}/*.h")

foreach(_source IN LISTS MOSAIC_LIBOBJC2_OBJC_SOURCES)
    get_filename_component(_name "${_source}" NAME_WE)
    set(_object "${CMAKE_CURRENT_BINARY_DIR}/objc/${_name}.obj")
    add_custom_command(
        OUTPUT "${_object}"
        COMMAND "${CMAKE_C_COMPILER}" /nologo /W3 /c
            "${_source}" "/Fo${_object}" ${MOSAIC_LIBOBJC2_INCLUDE_FLAGS}
            ${MOSAIC_LIBOBJC2_DEFINE_FLAGS}
        DEPENDS "${_source}" ${MOSAIC_LIBOBJC2_HEADERS}
        VERBATIM)
    list(APPEND MOSAIC_LIBOBJC2_OBJC_OBJECTS "${_object}")
endforeach()

set_source_files_properties(${MOSAIC_LIBOBJC2_OBJC_OBJECTS}
    PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
add_library(mosaic_objc_runtime STATIC
    ${MOSAIC_LIBOBJC2_C_SOURCES}
    ${MOSAIC_LIBOBJC2_OBJC_OBJECTS})
add_library(Mosaic::ObjCRuntime ALIAS mosaic_objc_runtime)

target_include_directories(mosaic_objc_runtime
    PUBLIC "${MOSAIC_LIBOBJC2_ROOT}"
    PRIVATE ${LIBOBJC2_INTERNAL_INCLUDE_DIRS})
target_compile_definitions(mosaic_objc_runtime PRIVATE
    ${MOSAIC_LIBOBJC2_DEFINES})
target_compile_options(mosaic_objc_runtime PRIVATE /W3)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(
        "${MOSAIC_LIBOBJC2_ROOT}/tests/windows"
        "${CMAKE_CURRENT_BINARY_DIR}/tests-windows")
endif()
