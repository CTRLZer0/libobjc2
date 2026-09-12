#ifndef __LIBOBJC_PLATFORM_H_INCLUDED__
#define __LIBOBJC_PLATFORM_H_INCLUDED__

#include <stddef.h>
#include "visibility.h"

#ifdef __cplusplus
extern "C" {
#endif

enum objc_platform_page_protection
{
	OBJC_PLATFORM_PAGE_NONE  = 0,
	OBJC_PLATFORM_PAGE_READ  = 1 << 0,
	OBJC_PLATFORM_PAGE_WRITE = 1 << 1,
	OBJC_PLATFORM_PAGE_EXEC  = 1 << 2
};

typedef void (*objc_platform_tls_cleanup_t)(void *);

#ifndef OBJC_PLATFORM_BACKEND_HEADER
#	if defined(__wasm__)
#		define OBJC_PLATFORM_BACKEND_HEADER "platform_wasm.h"
#	elif defined(_WIN32)
#		define OBJC_PLATFORM_BACKEND_HEADER "platform_windows.h"
#	elif defined(__unix__) || defined(__APPLE__)
#		define OBJC_PLATFORM_BACKEND_HEADER "platform_posix.h"
#	else
#		error "No libobjc2 platform type backend selected"
#	endif
#endif
#include OBJC_PLATFORM_BACKEND_HEADER

PRIVATE const char *objc_platform_name(void);
PRIVATE size_t objc_platform_page_size(void);
PRIVATE void *objc_platform_pages_allocate(size_t size);
PRIVATE int objc_platform_pages_protect(void *address, size_t size, unsigned protection);
PRIVATE void objc_platform_pages_release(void *address, size_t size);
PRIVATE void objc_platform_instruction_cache_flush(void *address, size_t size);
PRIVATE void *objc_platform_aligned_alloc_zero(size_t alignment, size_t size);
PRIVATE void objc_platform_aligned_free(void *pointer);
PRIVATE int objc_platform_tls_key_create(objc_platform_tls_key_t *key,
                                         objc_platform_tls_cleanup_t cleanup);
PRIVATE void *objc_platform_tls_get(const objc_platform_tls_key_t *key);
PRIVATE int objc_platform_tls_set(const objc_platform_tls_key_t *key, void *value);
PRIVATE void objc_platform_tls_key_destroy(objc_platform_tls_key_t *key);
PRIVATE int objc_platform_mutex_init_recursive(objc_platform_mutex_t *mutex);
PRIVATE void objc_platform_mutex_lock(objc_platform_mutex_t *mutex);
PRIVATE void objc_platform_mutex_unlock(objc_platform_mutex_t *mutex);
PRIVATE void objc_platform_mutex_destroy(objc_platform_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif