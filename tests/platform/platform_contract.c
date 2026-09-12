#include "platform.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition, code) \
	do { if (!(condition)) { fprintf(stderr, "platform-contract:%d\n", code); return code; } } while (0)

static int test_identity(void)
{
	const char *name = objc_platform_name();
	CHECK(name != NULL && name[0] != '\0', 1);
#if defined(__FreeBSD__)
	CHECK(strcmp(name, "freebsd") == 0, 2);
#elif defined(__linux__) && !defined(__ANDROID__)
	CHECK(strcmp(name, "linux") == 0, 3);
#elif defined(_WIN32)
	CHECK(strcmp(name, "windows") == 0, 4);
#endif
	return 0;
}

static int test_pages(void)
{
	size_t page = objc_platform_page_size();
	CHECK(page >= 4096 && (page & (page - 1)) == 0, 10);
	unsigned char *memory = objc_platform_pages_allocate(page * 2);
	CHECK(memory != NULL, 11);
	memory[0] = 0x5a;
	memory[page] = 0xa5;
	CHECK(objc_platform_pages_protect(memory, page,
		OBJC_PLATFORM_PAGE_READ) == 0, 12);
	CHECK(objc_platform_pages_protect(memory, page,
		OBJC_PLATFORM_PAGE_READ | OBJC_PLATFORM_PAGE_WRITE) == 0, 13);
	memory[0] ^= 0xff;
	objc_platform_instruction_cache_flush(memory, page);
	CHECK(objc_platform_pages_protect(memory + page, page,
		OBJC_PLATFORM_PAGE_READ | OBJC_PLATFORM_PAGE_EXEC) == 0, 14);
	objc_platform_pages_release(memory, page * 2);
	return 0;
}

static int test_aligned_allocation(void)
{
	enum { alignment = 64, bytes = 257 };
	unsigned char *memory = objc_platform_aligned_alloc_zero(alignment, bytes);
	CHECK(memory != NULL, 20);
	CHECK(((uintptr_t)memory & (alignment - 1)) == 0, 21);
	for (size_t i = 0; i < bytes; ++i)
	{
		CHECK(memory[i] == 0, 22);
	}
	memory[bytes - 1] = 0x7f;
	objc_platform_aligned_free(memory);
	return 0;
}
static int tls_cleanup_count;

static void tls_cleanup(void *value)
{
	if (value != NULL) { ++tls_cleanup_count; }
}

static int test_tls(void)
{
	objc_platform_tls_key_t key = {0};
	int first = 1;
	int second = 2;
	CHECK(objc_platform_tls_key_create(&key, tls_cleanup) == 0, 30);
	CHECK(objc_platform_tls_get(&key) == NULL, 31);
	CHECK(objc_platform_tls_set(&key, &first) == 0, 32);
	CHECK(objc_platform_tls_get(&key) == &first, 33);
	CHECK(objc_platform_tls_set(&key, &second) == 0, 34);
	CHECK(objc_platform_tls_get(&key) == &second, 35);
	CHECK(objc_platform_tls_set(&key, NULL) == 0, 36);
	CHECK(objc_platform_tls_get(&key) == NULL, 37);
	objc_platform_tls_key_destroy(&key);
	CHECK(tls_cleanup_count == 0, 38);
	return 0;
}

static int test_recursive_mutex(void)
{
	objc_platform_mutex_t mutex = {0};
	CHECK(objc_platform_mutex_init_recursive(&mutex) == 0, 40);
	objc_platform_mutex_lock(&mutex);
	objc_platform_mutex_lock(&mutex);
	objc_platform_mutex_unlock(&mutex);
	objc_platform_mutex_unlock(&mutex);
	objc_platform_mutex_destroy(&mutex);
	return 0;
}

int main(void)
{
	int result = test_identity();
	if (result != 0) { return result; }
	result = test_pages();
	if (result != 0) { return result; }
	result = test_aligned_allocation();
	if (result != 0) { return result; }
	result = test_tls();
	if (result != 0) { return result; }
	result = test_recursive_mutex();
	if (result != 0) { return result; }
	puts("platform-contract: ok");
	return 0;
}
