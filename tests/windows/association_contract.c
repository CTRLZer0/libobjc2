/* SPDX-License-Identifier: MIT */
#include "test_support.h"
#include "objc/runtime.h"
#include "objc/objc-arc.h"
#include "objc/mosaic.h"
#include <windows.h>

#define THREAD_COUNT 8
#define RACE_ROUNDS 64
#define READ_RACE_ITERATIONS 100000

static volatile LONG retain_count;
static volatile LONG release_count;
static volatile LONG barrier_arrived;
static volatile LONG barrier_open;
static volatile LONG dealloc_reentry_enabled;
static volatile LONG dealloc_reentry_calls;
static volatile LONG read_race_stop;
static volatile LONG read_race_errors;
static id dealloc_reentry_holder;
static char dealloc_reentry_key;

static id retain_object(id self, SEL _cmd)
{
	(void)_cmd;
	InterlockedIncrement(&retain_count);
	if (InterlockedCompareExchange(&barrier_open, 0, 0) == 0)
	{
		InterlockedIncrement(&barrier_arrived);
		while (InterlockedCompareExchange(&barrier_open, 0, 0) == 0)
		{
			SwitchToThread();
		}
	}
	return self;
}

static void release_object(id self, SEL _cmd)
{
	(void)self;
	(void)_cmd;
	InterlockedIncrement(&release_count);
	if (InterlockedCompareExchange(&dealloc_reentry_enabled, 0, 0) != 0)
	{
		InterlockedIncrement(&dealloc_reentry_calls);
		objc_setAssociatedObject(dealloc_reentry_holder, &dealloc_reentry_key, self,
			OBJC_ASSOCIATION_ASSIGN);
	}
}

struct worker_context
{
	id holder;
	id value;
	const void *key;
};

static DWORD WINAPI set_same_key(void *opaque)
{
	struct worker_context *context = opaque;
	objc_setAssociatedObject(context->holder, context->key, context->value,
		OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	return 0;
}

struct reader_context
{
	id holder;
	id expected;
	const void *key;
};

static DWORD WINAPI read_reused_slot(void *opaque)
{
	struct reader_context *context = opaque;
	while (InterlockedCompareExchange(&read_race_stop, 0, 0) == 0)
	{
		id value = objc_getAssociatedObject(context->holder, context->key);
		if ((value != nil) && (value != context->expected))
		{
			InterlockedIncrement(&read_race_errors);
			InterlockedExchange(&read_race_stop, 1);
			break;
		}
	}
	return 0;
}

static void install_memory_methods(Class cls)
{
	CHECK(class_addMethod(cls, sel_registerName("retain"),
		__builtin_bit_cast(IMP, &retain_object), "@@:"));
	CHECK(class_addMethod(cls, sel_registerName("release"),
		__builtin_bit_cast(IMP, &release_object), "v@:"));
}

static void run_same_key_race(id holder, id value, const void *key)
{
	HANDLE threads[THREAD_COUNT];
	struct worker_context context = { holder, value, key };
	barrier_arrived = 0;
	barrier_open = 0;

	for (unsigned i = 0; i < THREAD_COUNT; ++i)
	{
		threads[i] = CreateThread(NULL, 0, set_same_key, &context, 0, NULL);
		CHECK(threads[i] != NULL);
	}
	while (InterlockedCompareExchange(&barrier_arrived, 0, 0) != THREAD_COUNT)
	{
		SwitchToThread();
	}
	InterlockedExchange(&barrier_open, 1);
	CHECK(WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, 5000) == WAIT_OBJECT_0);
	for (unsigned i = 0; i < THREAD_COUNT; ++i)
	{
		CloseHandle(threads[i]);
	}
	void *pool = objc_autoreleasePoolPush();
	CHECK(objc_getAssociatedObject(holder, key) == value);
	objc_autoreleasePoolPop(pool);
	CHECK(retain_count - release_count == 1);
	objc_removeAssociatedObjects(holder);
	CHECK(retain_count == release_count);
}

static void run_reused_slot_read_race(id holder, id first, id second)
{
	static char firstKey;
	static char secondKey;
	HANDLE readers[4];
	struct reader_context contexts[4] = {
		{holder, first, &firstKey}, {holder, second, &secondKey},
		{holder, first, &firstKey}, {holder, second, &secondKey}};

	objc_setAssociatedObject(holder, &firstKey, first, OBJC_ASSOCIATION_ASSIGN);
	objc_setAssociatedObject(holder, &firstKey, nil, OBJC_ASSOCIATION_ASSIGN);
	read_race_stop = 0;
	read_race_errors = 0;
	for (unsigned i = 0; i < 4; ++i)
	{
		readers[i] = CreateThread(NULL, 0, read_reused_slot, &contexts[i], 0, NULL);
		CHECK(readers[i] != NULL);
	}
	for (unsigned i = 0; i < READ_RACE_ITERATIONS; ++i)
	{
		objc_setAssociatedObject(holder, &firstKey, first, OBJC_ASSOCIATION_ASSIGN);
		objc_setAssociatedObject(holder, &firstKey, nil, OBJC_ASSOCIATION_ASSIGN);
		objc_setAssociatedObject(holder, &secondKey, second, OBJC_ASSOCIATION_ASSIGN);
		objc_setAssociatedObject(holder, &secondKey, nil, OBJC_ASSOCIATION_ASSIGN);
		if (InterlockedCompareExchange(&read_race_errors, 0, 0) != 0) { break; }
	}
	InterlockedExchange(&read_race_stop, 1);
	CHECK(WaitForMultipleObjects(4, readers, TRUE, 5000) == WAIT_OBJECT_0);
	for (unsigned i = 0; i < 4; ++i) { CloseHandle(readers[i]); }
	CHECK(read_race_errors == 0);
}

int main(void)
{
	static char shared_key;
	mosaic_objc_runtime_initialize();
	Class holderClass = objc_allocateClassPair(Nil, "MosaicAssociationHolder", 0);
	Class valueClass = objc_allocateClassPair(Nil, "MosaicAssociationValue", 0);
	CHECK(holderClass != Nil && valueClass != Nil);
	install_memory_methods(valueClass);
	objc_registerClassPair(holderClass);
	objc_registerClassPair(valueClass);

	id holder = class_createInstance(holderClass, 0);
	id value = class_createInstance(valueClass, 0);
	id secondValue = class_createInstance(valueClass, 0);
	CHECK(holder != nil && value != nil && secondValue != nil);
	for (unsigned round = 0; round < RACE_ROUNDS; ++round)
	{
		run_same_key_race(holder, value, &shared_key);
	}
	run_reused_slot_read_race(holder, value, secondValue);

	char keys[32];
	barrier_open = 1;
	void *pool = objc_autoreleasePoolPush();
	for (unsigned i = 0; i < 32; ++i)
	{
		objc_setAssociatedObject(holder, &keys[i], value, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
		CHECK(objc_getAssociatedObject(holder, &keys[i]) == value);
	}
	objc_autoreleasePoolPop(pool);
	objc_removeAssociatedObjects(holder);
	CHECK(retain_count == release_count);

	id removeReentryHolder = class_createInstance(holderClass, 0);
	id removeReentryValue = class_createInstance(valueClass, 0);
	CHECK(removeReentryHolder != nil && removeReentryValue != nil);
	dealloc_reentry_holder = removeReentryHolder;
	dealloc_reentry_calls = 0;
	objc_setAssociatedObject(removeReentryHolder, &dealloc_reentry_key,
		removeReentryValue, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	InterlockedExchange(&dealloc_reentry_enabled, 1);
	objc_removeAssociatedObjects(removeReentryHolder);
	InterlockedExchange(&dealloc_reentry_enabled, 0);
	CHECK(dealloc_reentry_calls == 1);
	CHECK(objc_getAssociatedObject(removeReentryHolder, &dealloc_reentry_key) == nil);
	dealloc_reentry_holder = nil;
	object_dispose(removeReentryValue);
	object_dispose(removeReentryHolder);

	id reentryHolder = class_createInstance(holderClass, 0);
	id reentryValue = class_createInstance(valueClass, 0);
	CHECK(reentryHolder != nil && reentryValue != nil);
	dealloc_reentry_holder = reentryHolder;
	dealloc_reentry_calls = 0;
	objc_setAssociatedObject(reentryHolder, &dealloc_reentry_key, reentryValue,
		OBJC_ASSOCIATION_RETAIN_NONATOMIC);
	InterlockedExchange(&dealloc_reentry_enabled, 1);
	object_dispose(reentryHolder);
	InterlockedExchange(&dealloc_reentry_enabled, 0);
	CHECK(dealloc_reentry_calls == 1);
	dealloc_reentry_holder = nil;
	object_dispose(reentryValue);

	object_dispose(value);
	object_dispose(holder);
	return 0;
}
