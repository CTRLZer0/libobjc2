cmake_minimum_required(VERSION 3.24)
project(mosaic_libobjc2 LANGUAGES C)

if(NOT WIN32)
    message(FATAL_ERROR "Mosaic libobjc2 host runtime currently targets Windows")
endif()
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Mosaic libobjc2 requires clang-cl on Windows")
endif()

set(MOSAIC_LIBOBJC2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(MOSAIC_LIBOBJC2_DEFINES
    WIN32 _WINDOWS NO_PTHREADS=1 GNUSTEP
    __OBJC_RUNTIME_INTERNAL__=1 NO_LEGACY
    WINOBJC_HASH_SELECTOR_TYPES
    MOSAIC_LIBOBJC2_C_DISPATCH
    LIBOBJC2_NO_ASM_DISPATCH)

set(MOSAIC_LIBOBJC2_C_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/abi_version.c"
    "${MOSAIC_LIBOBJC2_ROOT}/alias_table.c"
    "${MOSAIC_LIBOBJC2_ROOT}/msvc/block_to_imp_winobjc.c"
    "${MOSAIC_LIBOBJC2_ROOT}/caps.c"
    "${MOSAIC_LIBOBJC2_ROOT}/category_loader.c"
    "${MOSAIC_LIBOBJC2_ROOT}/class_table.c"
    "${MOSAIC_LIBOBJC2_ROOT}/dtable.c"
)

list(APPEND MOSAIC_LIBOBJC2_C_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/encoding2.c"
    "${MOSAIC_LIBOBJC2_ROOT}/gc_none.c"
    "${MOSAIC_LIBOBJC2_ROOT}/msvc/hooks_mosaic.c"
    "${MOSAIC_LIBOBJC2_ROOT}/ivar.c"
    "${MOSAIC_LIBOBJC2_ROOT}/legacy_malloc.c"
    "${MOSAIC_LIBOBJC2_ROOT}/loader.c"
    "${MOSAIC_LIBOBJC2_ROOT}/protocol.c"
    "${MOSAIC_LIBOBJC2_ROOT}/runtime.c"
    "${MOSAIC_LIBOBJC2_ROOT}/sarray2.c"
    "${MOSAIC_LIBOBJC2_ROOT}/selector_table.c"
    "${MOSAIC_LIBOBJC2_ROOT}/sendmsg2.c"
    "${MOSAIC_LIBOBJC2_ROOT}/statics_loader.c")

set(MOSAIC_LIBOBJC2_OBJC_SOURCES
    "${MOSAIC_LIBOBJC2_ROOT}/arc.m"
    "${MOSAIC_LIBOBJC2_ROOT}/associate.m"
    "${MOSAIC_LIBOBJC2_ROOT}/blocks_runtime.m"
    "${MOSAIC_LIBOBJC2_ROOT}/mutation.m"
    "${MOSAIC_LIBOBJC2_ROOT}/NSBlocks.m"
    "${MOSAIC_LIBOBJC2_ROOT}/properties.m"
    "${MOSAIC_LIBOBJC2_ROOT}/Protocol2.m")

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
            "${_source}" "/Fo${_object}" "/I${MOSAIC_LIBOBJC2_ROOT}"
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

target_include_directories(mosaic_objc_runtime PUBLIC
    "${MOSAIC_LIBOBJC2_ROOT}")
target_compile_definitions(mosaic_objc_runtime PRIVATE
    ${MOSAIC_LIBOBJC2_DEFINES})
target_compile_options(mosaic_objc_runtime PRIVATE /W3)

include(CTest)
if(BUILD_TESTING)
    add_executable(mosaic_objc_runtime_contract
        "${CMAKE_CURRENT_LIST_DIR}/tests/runtime_contract.c")
    target_link_libraries(mosaic_objc_runtime_contract PRIVATE
        Mosaic::ObjCRuntime)
    add_test(NAME mosaic_objc_runtime_contract
        COMMAND mosaic_objc_runtime_contract)
endif()
