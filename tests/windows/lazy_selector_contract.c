/* SPDX-License-Identifier: MIT */
#include "test_support.h"
#include "objc/mosaic.h"
#include "selector.h"
#include <windows.h>
#include <string.h>

#define THREAD_COUNT 16
#define SLOT_COUNT 512

static SEL slots[SLOT_COUNT];
static volatile LONG readyCount;
static volatile LONG startFlag;
static volatile LONG errorCount;
static const char selectorName[] = "mosaicLazySelectorRace";

struct worker_context
{
	unsigned index;
};

static DWORD WINAPI register_cached_selectors(void *opaque)
{
	struct worker_context *context = opaque;
	InterlockedIncrement(&readyCount);
	while (InterlockedCompareExchange(&startFlag, 0, 0) == 0)
	{
		SwitchToThread();
	}
	for (unsigned n = 0; n < SLOT_COUNT; ++n)
	{
		unsigned i = (context->index & 1) ? (SLOT_COUNT - 1 - n) : n;
		SEL selector = objc2_get_or_register_selector(&slots[i], selectorName);
		if ((selector == NULL) || (strcmp(sel_getName(selector), selectorName) != 0))
		{
			InterlockedIncrement(&errorCount);
			break;
		}
	}
	return 0;
}

int main(void)
{
	mosaic_objc_runtime_initialize();
	HANDLE threads[THREAD_COUNT];
	struct worker_context contexts[THREAD_COUNT];

	for (unsigned i = 0; i < THREAD_COUNT; ++i)
	{
		contexts[i].index = i;
		threads[i] = CreateThread(NULL, 0, register_cached_selectors,
			&contexts[i], 0, NULL);
		CHECK(threads[i] != NULL);
	}
	while (InterlockedCompareExchange(&readyCount, 0, 0) != THREAD_COUNT)
	{
		SwitchToThread();
	}
	InterlockedExchange(&startFlag, 1);
	CHECK(WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, 5000) == WAIT_OBJECT_0);
	for (unsigned i = 0; i < THREAD_COUNT; ++i) { CloseHandle(threads[i]); }
	CHECK(errorCount == 0);

	SEL expected = sel_registerName(selectorName);
	CHECK(expected != NULL);
	for (unsigned i = 0; i < SLOT_COUNT; ++i)
	{
		CHECK(objc2_get_or_register_selector(&slots[i], selectorName) == expected);
	}
	return 0;
}
