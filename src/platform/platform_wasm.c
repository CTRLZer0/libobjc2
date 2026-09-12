#include "platform.h"

#include <stdlib.h>
#include <string.h>

PRIVATE const char *objc_platform_name(void)
{
	return "wasm";
}

PRIVATE size_t objc_platform_page_size(void)
{
	return 65536;
}

PRIVATE void *objc_platform_pages_allocate(size_t size)
{
	return calloc(1, size);
}

PRIVATE int objc_platform_pages_protect(void *address, size_t size, unsigned protection)
{
	(void)address;
	(void)size;
	(void)protection;
	return 0;
}

PRIVATE void objc_platform_pages_release(void *address, size_t size)
{
	(void)size;
	free(address);
}

PRIVATE void objc_platform_instruction_cache_flush(void *address, size_t size)
{
	(void)address;
	(void)size;
}

PRIVATE void *objc_platform_aligned_alloc_zero(size_t alignment, size_t size)
{
	void *memory = NULL;
	if (posix_memalign(&memory, alignment, size) != 0) { return NULL; }
	memset(memory, 0, size);
	return memory;
}

PRIVATE void objc_platform_aligned_free(void *pointer)
{
	free(pointer);
}

PRIVATE int objc_platform_tls_key_create(objc_platform_tls_key_t *key,
                                         objc_platform_tls_cleanup_t cleanup)
{
	(void)key;
	(void)cleanup;
	return -1;
}

PRIVATE void *objc_platform_tls_get(const objc_platform_tls_key_t *key)
{
	(void)key;
	return NULL;
}

PRIVATE int objc_platform_tls_set(const objc_platform_tls_key_t *key, void *value)
{
	(void)key;
	(void)value;
	return -1;
}

PRIVATE void objc_platform_tls_key_destroy(objc_platform_tls_key_t *key)
{
	(void)key;
}


PRIVATE int objc_platform_mutex_init_recursive(objc_platform_mutex_t *mutex)
{
	if (mutex == NULL) { return -1; }
	memset(mutex, 0, sizeof(*mutex));
	return 0;
}

PRIVATE void objc_platform_mutex_lock(objc_platform_mutex_t *mutex)
{
	(void)mutex;
}

PRIVATE void objc_platform_mutex_unlock(objc_platform_mutex_t *mutex)
{
	(void)mutex;
}

PRIVATE void objc_platform_mutex_destroy(objc_platform_mutex_t *mutex)
{
	if (mutex != NULL) { memset(mutex, 0, sizeof(*mutex)); }
}
