if(NOT DEFINED ROOT OR NOT DEFINED CLANG OR NOT DEFINED OUT_DIR)
	message(FATAL_ERROR "ROOT, CLANG and OUT_DIR are required")
endif()
file(MAKE_DIRECTORY "${OUT_DIR}")
set(_object "${OUT_DIR}/objc_msgSend-arm64-tracing.o")
execute_process(
	COMMAND "${CLANG}" --target=arm64-apple-darwin -DWITH_TRACING=1
		"-I${ROOT}/src/internal/dispatch"
		-c "${ROOT}/src/dispatch/asm/objc_msgSend.S" -o "${_object}"
	RESULT_VARIABLE _result
	OUTPUT_VARIABLE _stdout
	ERROR_VARIABLE _stderr)
if(NOT _result EQUAL 0)
	message(FATAL_ERROR "ARM64 tracing assembly contract failed:\n${_stdout}\n${_stderr}")
endif()
if(NOT EXISTS "${_object}")
	message(FATAL_ERROR "ARM64 tracing assembly object was not produced")
endif()
