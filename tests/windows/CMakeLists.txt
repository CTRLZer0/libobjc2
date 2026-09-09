# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
# Original CTRLZer0 work; see LICENSE-CTRLZERO and NOTICE.md for licensing
# and provenance details.
if(NOT WIN32)
	return()
endif()

function(add_mosaic_runtime_test name source)
	add_executable(${name} ${source})
	target_link_libraries(${name} PRIVATE Mosaic::ObjCRuntime)
	add_test(NAME ${name} COMMAND ${name})
endfunction()

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
add_mosaic_runtime_test(
	mosaic_objc_weak_contract
	weak_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_memory_contract
	memory_contract.c)
add_mosaic_runtime_test(
	mosaic_objc_protocol_contract
	protocol_contract.c)
