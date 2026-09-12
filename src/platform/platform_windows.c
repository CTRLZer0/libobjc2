#include "platform.h"
#include "safewindows.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#if defined(WINAPI_FAMILY) && WINAPI_FAMILY != WINAPI_FAMILY_DESKTOP_APP && _WIN32_WINNT >= 0x0A00
#define OBJC_VIRTUAL_ALLOC VirtualAllocFromApp
#define OBJC_VIRTUAL_PROTECT VirtualProtectFromApp
#else
#define OBJC_VIRTUAL_ALLOC VirtualAlloc
#define OBJC_VIRTUAL_PROTECT VirtualProtect
#endif

PRIVATE const char *objc_platform_name(void)
{
	return "windows";
}

PRIVATE size_t objc_platform_page_size(void)
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return (size_t)info.dwPageSize;
}

PRIVATE void *objc_platform_pages_allocate(size_t size)
{
	return OBJC_VIRTUAL_ALLOC(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

PRIVATE int objc_platform_pages_protect(void *address, size_t size, unsigned protection)
{
	DWORD native = PAGE_NOACCESS;
	if (protection & OBJC_PLATFORM_PAGE_WRITE)
	{
		native = PAGE_READWRITE;
	}
	else if (protection & OBJC_PLATFORM_PAGE_READ)
	{
		native = PAGE_READONLY;
	}
	if (protection & OBJC_PLATFORM_PAGE_EXEC)
	{
		switch (native)
		{
			case PAGE_NOACCESS: native = PAGE_EXECUTE; break;
			case PAGE_READONLY: native = PAGE_EXECUTE_READ; break;
			case PAGE_READWRITE: native = PAGE_EXECUTE_READWRITE; break;
		}
	}
	DWORD oldProtection = 0;
	return OBJC_VIRTUAL_PROTECT(address, size, native, &oldProtection) ? 0 : -1;
}

PRIVATE void objc_platform_pages_release(void *address, size_t size)
{
	(void)size;
	if (address != NULL) { (void)VirtualFree(address, 0, MEM_RELEASE); }
}

PRIVATE void objc_platform_instruction_cache_flush(void *address, size_t size)
{
	(void)FlushInstructionCache(GetCurrentProcess(), address, size);
}

PRIVATE void *objc_platform_aligned_alloc_zero(size_t alignment, size_t size)
{
	void *memory = _aligned_malloc(size, alignment);
	if (memory != NULL) { memset(memory, 0, size); }
	return memory;
}

PRIVATE void objc_platform_aligned_free(void *pointer)
{
	_aligned_free(pointer);
}

struct objc_windows_tls_value
{
	objc_platform_tls_cleanup_t cleanup;
	void *value;
};

static VOID CALLBACK objc_windows_tls_cleanup(void *raw)
{
	struct objc_windows_tls_value *entry = raw;
	if (entry == NULL) { return; }
	objc_platform_tls_cleanup_t cleanup = entry->cleanup;
	void *value = entry->value;
	free(entry);
	if (cleanup != NULL && value != NULL) { cleanup(value); }
}

PRIVATE int objc_platform_tls_key_create(objc_platform_tls_key_t *key,
                                         objc_platform_tls_cleanup_t cleanup)
{
	if (key == NULL) { return -1; }
	DWORD slot = FlsAlloc(objc_windows_tls_cleanup);
	if (slot == FLS_OUT_OF_INDEXES) { return -1; }
	key->slot = slot;
	key->cleanup = cleanup;
	return 0;
}

PRIVATE void *objc_platform_tls_get(const objc_platform_tls_key_t *key)
{
	if (key == NULL) { return NULL; }
	struct objc_windows_tls_value *entry = FlsGetValue(key->slot);
	return entry == NULL ? NULL : entry->value;
}

PRIVATE int objc_platform_tls_set(const objc_platform_tls_key_t *key, void *value)
{
	if (key == NULL) { return -1; }
	struct objc_windows_tls_value *entry = FlsGetValue(key->slot);
	if (value == NULL)
	{
		if (!FlsSetValue(key->slot, NULL)) { return -1; }
		free(entry);
		return 0;
	}
	if (entry == NULL)
	{
		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) { return -1; }
		entry->cleanup = key->cleanup;
	}
	entry->value = value;
	if (!FlsSetValue(key->slot, entry))
	{
		free(entry);
		return -1;
	}
	return 0;
}

PRIVATE void objc_platform_tls_key_destroy(objc_platform_tls_key_t *key)
{
	if (key == NULL) { return; }
	struct objc_windows_tls_value *entry = FlsGetValue(key->slot);
	if (entry != NULL)
	{
		(void)FlsSetValue(key->slot, NULL);
		free(entry);
	}
	(void)FlsFree(key->slot);
	key->slot = FLS_OUT_OF_INDEXES;
	key->cleanup = NULL;
}

PRIVATE int objc_platform_mutex_init_recursive(objc_platform_mutex_t *mutex)
{
	if (mutex == NULL) { return -1; }
	InitializeCriticalSection(&mutex->native);
	return 0;
}

PRIVATE void objc_platform_mutex_lock(objc_platform_mutex_t *mutex)
{
	EnterCriticalSection(&mutex->native);
}

PRIVATE void objc_platform_mutex_unlock(objc_platform_mutex_t *mutex)
{
	LeaveCriticalSection(&mutex->native);
}

PRIVATE void objc_platform_mutex_destroy(objc_platform_mutex_t *mutex)
{
	if (mutex != NULL) { DeleteCriticalSection(&mutex->native); }
}
