# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md.
# Central source manifest for libobjc2.
# Callers must define LIBOBJC2_SOURCE_ROOT before including this file.

if(NOT LIBOBJC2_SOURCE_ROOT)
    message(FATAL_ERROR "LIBOBJC2_SOURCE_ROOT must be set before RuntimeSources.cmake")
endif()

set(LIBOBJC2_INTERNAL_INCLUDE_DIRS
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/runtime"
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/dispatch"
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/memory"
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/blocks"
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/exceptions"
    "${LIBOBJC2_SOURCE_ROOT}/src/internal/support")

set(LIBOBJC2_RUNTIME_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/abi_version.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/alias_table.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/caps.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/category_loader.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/class_table.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/fast_paths.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/hooks.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/ivar.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/loader.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/protocol.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/runtime.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/statics_loader.c")

set(LIBOBJC2_RUNTIME_OBJC_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/mutation.m"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/Protocol2.m"
    "${LIBOBJC2_SOURCE_ROOT}/src/runtime/properties.m")
set(LIBOBJC2_DISPATCH_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/dtable.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/hash_table.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/sarray2.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/selector_table.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/sendmsg2.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/toydispatch.c")

set(LIBOBJC2_DISPATCH_ASM_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/asm/block_trampolines.S"
    "${LIBOBJC2_SOURCE_ROOT}/src/dispatch/asm/objc_msgSend.S")

set(LIBOBJC2_MEMORY_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/memory/legacy_malloc.c")
set(LIBOBJC2_MEMORY_OBJC_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/memory/arc.m"
    "${LIBOBJC2_SOURCE_ROOT}/src/memory/associate.m")
set(LIBOBJC2_GC_BOEHM_SOURCE
    "${LIBOBJC2_SOURCE_ROOT}/src/memory/gc_boehm.c")
set(LIBOBJC2_GC_NONE_SOURCE
    "${LIBOBJC2_SOURCE_ROOT}/src/memory/gc_none.c")

set(LIBOBJC2_BLOCK_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/blocks/block_to_imp.c")
set(LIBOBJC2_BLOCK_OBJC_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/blocks/blocks_runtime.m"
    "${LIBOBJC2_SOURCE_ROOT}/src/blocks/NSBlocks.m")
set(LIBOBJC2_ENCODING_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/encoding/encoding2.c")

set(LIBOBJC2_EXCEPTION_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/exceptions/eh_personality.c")
set(LIBOBJC2_EXCEPTION_CXX_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/exceptions/objcxx_eh.cc")

set(LIBOBJC2_WINDOWS_SOURCES
    "${LIBOBJC2_SOURCE_ROOT}/src/platform/windows/block_to_imp_winobjc.c"
    "${LIBOBJC2_SOURCE_ROOT}/src/platform/windows/hooks_mosaic.c")

set(LIBOBJC2_CORE_C_SOURCES
    ${LIBOBJC2_RUNTIME_SOURCES}
    ${LIBOBJC2_DISPATCH_SOURCES}
    ${LIBOBJC2_MEMORY_SOURCES}
    ${LIBOBJC2_BLOCK_SOURCES}
    ${LIBOBJC2_ENCODING_SOURCES}
    ${LIBOBJC2_EXCEPTION_SOURCES})

set(LIBOBJC2_CORE_OBJC_SOURCES
    ${LIBOBJC2_RUNTIME_OBJC_SOURCES}
    ${LIBOBJC2_MEMORY_OBJC_SOURCES}
    ${LIBOBJC2_BLOCK_OBJC_SOURCES})
