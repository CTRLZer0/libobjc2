#include "platform.h"
#include <stdio.h>

#if defined(_WIN32)
#include "safewindows.h"
static volatile LONG cleanup_count;
static void record_cleanup(void *value)
{
	if (value != NULL) { InterlockedIncrement(&cleanup_count); }
}
static DWORD WINAPI tls_thread(void *raw)
{
	objc_platform_tls_key_t *key = raw;
	static int value = 42;
	return objc_platform_tls_set(key, &value) == 0 ? 0 : 1;
}
#else
#include <pthread.h>
#include <stdatomic.h>
static _Atomic int cleanup_count;
static void record_cleanup(void *value)
{
	if (value != NULL) { atomic_fetch_add(&cleanup_count, 1); }
}
static void *tls_thread(void *raw)
{
	objc_platform_tls_key_t *key = raw;
	static int value = 42;
	return objc_platform_tls_set(key, &value) == 0 ? NULL : (void *)1;
}
#endif
int main(void)
{
	objc_platform_tls_key_t key = {0};
	if (objc_platform_tls_key_create(&key, record_cleanup) != 0) { return 10; }
#if defined(_WIN32)
	HANDLE thread = CreateThread(NULL, 0, tls_thread, &key, 0, NULL);
	if (thread == NULL) { return 11; }
	if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) { return 12; }
	DWORD result = 0;
	if (!GetExitCodeThread(thread, &result) || result != 0) { return 13; }
	CloseHandle(thread);
	if (cleanup_count != 1) { return 14; }
#else
	pthread_t thread;
	if (pthread_create(&thread, NULL, tls_thread, &key) != 0) { return 11; }
	void *result = NULL;
	if (pthread_join(thread, &result) != 0 || result != NULL) { return 12; }
	if (atomic_load(&cleanup_count) != 1) { return 13; }
#endif
	objc_platform_tls_key_destroy(&key);
	puts("platform-tls-thread-contract: ok");
	return 0;
}