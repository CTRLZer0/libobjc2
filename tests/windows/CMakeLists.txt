# SPDX-License-Identifier: AGPL-3.0-only
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
