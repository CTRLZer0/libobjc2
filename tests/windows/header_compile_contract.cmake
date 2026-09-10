# SPDX-License-Identifier: MIT
# Compile public headers independently in C and C++.
if(NOT ROOT OR NOT RUNTIME_INCLUDE OR NOT CLANG OR NOT CLANGXX OR NOT OUT_DIR)
	message(FATAL_ERROR "header contract is missing configuration")
endif()

file(MAKE_DIRECTORY "${OUT_DIR}")
set(headers
	runtime-types.h runtime-selector.h runtime-property.h runtime-protocol.h
	runtime-association.h runtime-dispatch.h runtime-small-object.h runtime-block.h
	runtime-class.h runtime-object.h
	runtime-ivar.h runtime-method.h runtime.h message.h slot.h
	objc-arc.h objc-auto.h encoding.h hooks.h
	objc-exception.h developer.h capabilities.h
	blocks_runtime.h Availability.h mosaic.h
	objc.h objc-api.h objc-class.h objc-runtime.h)

foreach(header IN LISTS headers)
	string(MAKE_C_IDENTIFIER "${header}" ident)
	foreach(language IN ITEMS c cxx)
		if(language STREQUAL "c")
			set(compiler "${CLANG}")
			set(extension c)
		else()
			set(compiler "${CLANGXX}")
			set(extension cpp)
		endif()
		set(source "${OUT_DIR}/${ident}.${extension}")
		file(WRITE "${source}"
			"#include <objc/${header}>\n#include <objc/${header}>\nint ${ident}_${language}(void) { return 0; }\n")
		execute_process(
			COMMAND "${compiler}" -fsyntax-only -Wall -Wextra -Werror
				-D__OBJC_RUNTIME_STATIC__=1
				"-I${ROOT}" "-I${RUNTIME_INCLUDE}" "${source}"
			RESULT_VARIABLE result
			OUTPUT_VARIABLE output
			ERROR_VARIABLE error)
		if(NOT result EQUAL 0)
			message(FATAL_ERROR
				"${header} failed as ${language}:\n${output}${error}")
		endif()
	endforeach()
endforeach()

message(STATUS "public header contract passed in C and C++")
