# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
# and provenance details.
if(NOT WIN32)
    return()
endif()

add_executable(mosaic_objc_runtime_hotpaths runtime_hotpaths.c)
target_link_libraries(mosaic_objc_runtime_hotpaths PRIVATE Mosaic::ObjCRuntime)

if(MOSAIC_LIBOBJC2_WARNINGS_AS_ERRORS)
    if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(mosaic_objc_runtime_hotpaths PRIVATE /W4 /WX)
    else()
        target_compile_options(mosaic_objc_runtime_hotpaths PRIVATE -Wall -Wextra -Werror)
    endif()
endif()
