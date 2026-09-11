# SPDX-License-Identifier: MIT
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# CTRLZer0 work is licensed under MIT; see COPYING and NOTICE.md for licensing
# and provenance details.
if(NOT WIN32)
	return()
endif()

function(add_mosaic_runtime_test name source)
	add_executable(${name} ${source})
	target_link_libraries(${name} PRIVATE Mosaic::ObjCRuntime)
	if(MOSAIC_LIBOBJC2_WARNINGS_AS_ERRORS)
		if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
			target_compile_options(${name} PRIVATE /W4 /WX)
		else()
			target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
		endif()
	endif()
	add_test(NAME ${name} COMMAND ${name})
endfunction()

add_test(
	NAME mosaic_objc_public_headers_contract
	COMMAND "${CMAKE_COMMAND}"
		"-DROOT=${MOSAIC_LIBOBJC2_ROOT}"
		"-DRUNTIME_INCLUDE=${_runtime_dir}/objc"
		"-DCLANG=${MOSAIC_LIBOBJC2_CLANG}"
		"-DCLANGXX=${MOSAIC_LIBOBJC2_CLANGXX}"
		"-DOUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/header-contract"
		-P "${CMAKE_CURRENT_LIST_DIR}/header_compile_contract.cmake")

add_mosaic_runtime_test(
	mosaic_objc_runtime_contract
	runtime_contract.c)

add_mosaic_runtime_test(
	mosaic_objc_selector_contract
	selector_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_class_contract
	class_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_ivar_contract
	ivar_contract.c)
target_include_directories(mosaic_objc_ivar_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/runtime")
add_mosaic_runtime_test(
	mosaic_objc_weak_contract
	weak_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_memory_contract
	memory_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_association_contract
	association_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_protocol_contract
	protocol_contract.c)

add_mosaic_runtime_test(
	mosaic_objc_fast_paths_contract
	fast_paths_contract.c)

add_mosaic_runtime_test(
	mosaic_objc_sarray_contract
	sarray_contract.c)
target_include_directories(mosaic_objc_sarray_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/dispatch"
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/support")

add_mosaic_runtime_test(
	mosaic_objc_hash_table_contract
	hash_table_contract.c)
target_include_directories(mosaic_objc_hash_table_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/support"
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/exceptions")

add_mosaic_runtime_test(
	mosaic_objc_buffer_contract
	buffer_contract.c)
target_include_directories(mosaic_objc_buffer_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/support")

add_mosaic_runtime_test(
	mosaic_objc_blocks_contract
	blocks_contract.c)
target_include_directories(mosaic_objc_blocks_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/blocks")

add_mosaic_runtime_test(
	mosaic_objc_lazy_selector_contract
	lazy_selector_contract.c)
target_include_directories(mosaic_objc_lazy_selector_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/dispatch")

add_mosaic_runtime_test(
	mosaic_objc_bitfield_contract
	bitfield_contract.c)
target_include_directories(mosaic_objc_bitfield_contract PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/../../src/internal/runtime")
