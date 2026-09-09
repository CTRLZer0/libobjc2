# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(mosaic_libobjc2 LANGUAGES C)
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic libobjc2 host runtime currently targets Windows")
endif()

set(MOSAIC_LIBOBJC2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(LIBOBJC2_SOURCE_ROOT "${MOSAIC_LIBOBJC2_ROOT}")
include("${MOSAIC_LIBOBJC2_ROOT}/CMake/RuntimeSources.cmake")

set(MOSAIC_LIBOBJC2_CLANG_CL "" CACHE FILEPATH
    "Private clang-cl used to compile the Mosaic Objective-C runtime")
if(NOT MOSAIC_LIBOBJC2_CLANG_CL)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(MOSAIC_LIBOBJC2_CLANG_CL "${CMAKE_C_COMPILER}")
    else()
        find_program(MOSAIC_LIBOBJC2_CLANG_CL NAMES clang-cl)
    endif()
endif()

if(NOT MOSAIC_LIBOBJC2_CLANG_CL OR NOT EXISTS "${MOSAIC_LIBOBJC2_CLANG_CL}")
    message(FATAL_ERROR
        "clang-cl was not found. Set MOSAIC_LIBOBJC2_CLANG_CL to LLVM 23.1.1 or newer.")
endif()
execute_process(
    COMMAND "${MOSAIC_LIBOBJC2_CLANG_CL}" --version
    OUTPUT_VARIABLE MOSAIC_LIBOBJC2_CLANG_VERSION_TEXT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE MOSAIC_LIBOBJC2_CLANG_VERSION_RESULT)
if(NOT MOSAIC_LIBOBJC2_CLANG_VERSION_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to execute ${MOSAIC_LIBOBJC2_CLANG_CL}")
endif()
string(REGEX MATCH "clang version ([0-9]+\.[0-9]+\.[0-9]+)"
    _clang_match "${MOSAIC_LIBOBJC2_CLANG_VERSION_TEXT}")
set(MOSAIC_LIBOBJC2_CLANG_VERSION "${CMAKE_MATCH_1}")
if(NOT MOSAIC_LIBOBJC2_CLANG_VERSION)
    message(FATAL_ERROR "Unable to determine clang-cl version")
endif()
if(MOSAIC_LIBOBJC2_CLANG_VERSION VERSION_LESS "23.1.1")
    message(FATAL_ERROR
        "Mosaic libobjc2 requires LLVM 23.1.1 or newer; found ${MOSAIC_LIBOBJC2_CLANG_VERSION}")
endif()
message(STATUS
    "Mosaic libobjc2 private compiler: ${MOSAIC_LIBOBJC2_CLANG_CL} (${MOSAIC_LIBOBJC2_CLANG_VERSION})")

set(MOSAIC_LIBOBJC2_DEFINES
    WIN32 _WINDOWS NO_PTHREADS=1 GNUSTEP
    __OBJC_RUNTIME_INTERNAL__=1 NO_LEGACY
    WINOBJC_HASH_SELECTOR_TYPES
    MOSAIC_LIBOBJC2_C_DISPATCH
    LIBOBJC2_NO_ASM_DISPATCH)
set(MOSAIC_LIBOBJC2_SOURCES
    ${LIBOBJC2_RUNTIME_SOURCES}
    ${LIBOBJC2_RUNTIME_OBJC_SOURCES}
    ${LIBOBJC2_MEMORY_OBJC_SOURCES}
    ${LIBOBJC2_BLOCK_OBJC_SOURCES}
    ${LIBOBJC2_ENCODING_SOURCES}
    ${LIBOBJC2_GC_NONE_SOURCE}
    ${LIBOBJC2_DISPATCH_SOURCES}
    "${MOSAIC_LIBOBJC2_ROOT}/src/platform/windows/block_to_imp_winobjc.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/platform/windows/hooks_mosaic.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/memory/legacy_malloc.c")
list(REMOVE_ITEM MOSAIC_LIBOBJC2_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/src/runtime/hooks.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/dispatch/hash_table.c"
    "${MOSAIC_LIBOBJC2_ROOT}/src/dispatch/toydispatch.c")

set(MOSAIC_LIBOBJC2_INCLUDE_FLAGS "/I${MOSAIC_LIBOBJC2_ROOT}")
foreach(_include IN LISTS LIBOBJC2_INTERNAL_INCLUDE_DIRS)
    list(APPEND MOSAIC_LIBOBJC2_INCLUDE_FLAGS "/I${_include}")
endforeach()

set(MOSAIC_LIBOBJC2_DEFINE_FLAGS)
foreach(_definition IN LISTS MOSAIC_LIBOBJC2_DEFINES)
    list(APPEND MOSAIC_LIBOBJC2_DEFINE_FLAGS "/D${_definition}")
endforeach()

file(GLOB_RECURSE MOSAIC_LIBOBJC2_HEADERS CONFIGURE_DEPENDS
    "${MOSAIC_LIBOBJC2_ROOT}/*.h")
set(MOSAIC_LIBOBJC2_OBJECTS)
foreach(_source IN LISTS MOSAIC_LIBOBJC2_SOURCES)
    file(RELATIVE_PATH _relative "${MOSAIC_LIBOBJC2_ROOT}" "${_source}")
    string(REGEX REPLACE "[^A-Za-z0-9_]" "_" _object_name "${_relative}")
    set(_object_dir "${CMAKE_CURRENT_BINARY_DIR}/objects/$<CONFIG>")
    set(_object "${_object_dir}/${_object_name}.obj")

    add_custom_command(
        OUTPUT "${_object}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_object_dir}"
        COMMAND "${MOSAIC_LIBOBJC2_CLANG_CL}" /nologo /W3 /c
            "${_source}" "/Fo${_object}"
            ${MOSAIC_LIBOBJC2_INCLUDE_FLAGS}
            ${MOSAIC_LIBOBJC2_DEFINE_FLAGS}
            "$<$<CONFIG:Debug>:/Od>"
            "$<$<CONFIG:Debug>:/Zi>"
            "$<$<CONFIG:Debug>:/MDd>"
            "$<$<CONFIG:Release>:/O2>"
            "$<$<CONFIG:Release>:/MD>"
            "$<$<CONFIG:Release>:/DNDEBUG>"
        DEPENDS "${_source}" ${MOSAIC_LIBOBJC2_HEADERS}
        COMMAND_EXPAND_LISTS
        VERBATIM)
    list(APPEND MOSAIC_LIBOBJC2_OBJECTS "${_object}")
endforeach()

set_source_files_properties(${MOSAIC_LIBOBJC2_OBJECTS}
    PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
add_library(mosaic_objc_runtime STATIC ${MOSAIC_LIBOBJC2_OBJECTS})
add_library(Mosaic::ObjCRuntime ALIAS mosaic_objc_runtime)
set_target_properties(mosaic_objc_runtime PROPERTIES LINKER_LANGUAGE C)
target_include_directories(mosaic_objc_runtime INTERFACE
    "${MOSAIC_LIBOBJC2_ROOT}")

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(
        "${MOSAIC_LIBOBJC2_ROOT}/tests/windows"
        "${CMAKE_CURRENT_BINARY_DIR}/tests-windows")
endif()
