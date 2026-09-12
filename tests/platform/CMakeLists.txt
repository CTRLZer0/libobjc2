set(LIBOBJC2_PLATFORM_TEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
find_package(Threads REQUIRED)

if(DEFINED OBJC_PLATFORM_BACKEND AND NOT OBJC_PLATFORM_BACKEND STREQUAL "auto")
	set(_platform_backend "${OBJC_PLATFORM_BACKEND}")
elseif(WIN32)
	set(_platform_backend windows)
elseif(UNIX)
	set(_platform_backend posix)
else()
	return()
endif()

function(add_platform_contract name source)
	add_executable(${name}
		${source}
		"${LIBOBJC2_PLATFORM_TEST_ROOT}/src/platform/platform_${_platform_backend}.c")
	target_include_directories(${name} PRIVATE
		"${LIBOBJC2_PLATFORM_TEST_ROOT}"
		"${LIBOBJC2_PLATFORM_TEST_ROOT}/src/internal/platform"
		"${LIBOBJC2_PLATFORM_TEST_ROOT}/src/internal/support"
		"${LIBOBJC2_PLATFORM_TEST_ROOT}/src/internal/exceptions")
	target_link_libraries(${name} PRIVATE Threads::Threads)
	target_compile_definitions(${name} PRIVATE
		"OBJC_PLATFORM_BACKEND_HEADER=\"platform_${_platform_backend}.h\"")
	if(MSVC)
		target_compile_options(${name} PRIVATE /W4)
	else()
		target_compile_options(${name} PRIVATE -Wall -Wextra -Werror)
	endif()
	add_test(NAME ${name} COMMAND ${name})
	set_tests_properties(${name} PROPERTIES LABELS "platform")
endfunction()

add_platform_contract(mosaic_objc_platform_contract platform_contract.c)
add_platform_contract(mosaic_objc_platform_tls_thread_contract tls_thread_contract.c)