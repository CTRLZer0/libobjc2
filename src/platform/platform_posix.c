#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

PRIVATE const char *objc_platform_name(void)
{
#if defined(__FreeBSD__)
	return "freebsd";
#elif defined(__linux__)
	return "linux";
#elif defined(__APPLE__)
	return "darwin";
#elif defined(__ANDROID__)
	return "android";
#else
	return "posix";
#endif
}

PRIVATE size_t objc_platform_page_size(void)
{
	long size = sysconf(_SC_PAGESIZE);
	return size > 0 ? (size_t)size : 0;
}

PRIVATE void *objc_platform_pages_allocate(size_t size)
{
	void *memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
	                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return memory == MAP_FAILED ? NULL : memory;
}

PRIVATE int objc_platform_pages_protect(void *address, size_t size, unsigned protection)
{
	int native = PROT_NONE;
	if (protection & OBJC_PLATFORM_PAGE_READ) { native |= PROT_READ; }
	if (protection & OBJC_PLATFORM_PAGE_WRITE) { native |= PROT_WRITE; }
	if (protection & OBJC_PLATFORM_PAGE_EXEC) { native |= PROT_EXEC; }
	return mprotect(address, size, native);
}

PRIVATE void objc_platform_pages_release(void *address, size_t size)
{
	if (address != NULL && size != 0) { (void)munmap(address, size); }
}

PRIVATE void objc_platform_instruction_cache_flush(void *address, size_t size)
{
#if defined(__clang__) || defined(__GNUC__)
	__builtin___clear_cache((char *)address, (char *)address + size);
#else
	(void)address;
	(void)size;
#endif
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
	if (key == NULL) { return -1; }
	return pthread_key_create(&key->native, cleanup);
}

PRIVATE void *objc_platform_tls_get(const objc_platform_tls_key_t *key)
{
	return key == NULL ? NULL : pthread_getspecific(key->native);
}

PRIVATE int objc_platform_tls_set(const objc_platform_tls_key_t *key, void *value)
{
	return key == NULL ? -1 : pthread_setspecific(key->native, value);
}

PRIVATE void objc_platform_tls_key_destroy(objc_platform_tls_key_t *key)
{
	if (key != NULL) { (void)pthread_key_delete(key->native); }
}

PRIVATE int objc_platform_mutex_init_recursive(objc_platform_mutex_t *mutex)
{
	if (mutex == NULL) { return -1; }
	pthread_mutexattr_t attributes;
	if (pthread_mutexattr_init(&attributes) != 0) { return -1; }
	int result = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
	if (result == 0) { result = pthread_mutex_init(&mutex->native, &attributes); }
	(void)pthread_mutexattr_destroy(&attributes);
	return result;
}

PRIVATE void objc_platform_mutex_lock(objc_platform_mutex_t *mutex)
{
	(void)pthread_mutex_lock(&mutex->native);
}

PRIVATE void objc_platform_mutex_unlock(objc_platform_mutex_t *mutex)
{
	(void)pthread_mutex_unlock(&mutex->native);
}

PRIVATE void objc_platform_mutex_destroy(objc_platform_mutex_t *mutex)
{
	if (mutex != NULL) { (void)pthread_mutex_destroy(&mutex->native); }
}
