/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include "objc/mosaic.h"
#include "objc/blocks_runtime.h"
#include "objc/blocks_private.h"
#include "blocks_runtime.h"

#define CHECK(expr, code) do { \
    if (!(expr)) { fprintf(stderr, "check failed: %s\n", #expr); return (code); } \
} while (0)

enum { THREAD_COUNT = 8, BLOCK_ITERATIONS = 50000 };

extern void _NSConcreteStackBlock;
static volatile LONG failures;
static volatile LONG promotion_ready;
static volatile LONG assignment_ready;
static volatile LONG byref_dispose_calls;
static HANDLE byref_release_event;
struct block_thread_context
{
    struct Block_layout *heap_block;
};

struct byref_thread_context
{
    struct block_byref_obj *stack_byref;
};

static void record_failure(void)
{
    InterlockedIncrement(&failures);
}

static void byref_keep_barrier(
    struct block_byref_obj *dst,
    const struct block_byref_obj *src)
{
    (void)dst;
    (void)src;
    InterlockedIncrement(&promotion_ready);
    while (InterlockedCompareExchange(&promotion_ready, 0, 0) < THREAD_COUNT)
    {
        SwitchToThread();
    }
}
static void byref_dispose_probe(struct block_byref_obj *src)
{
    (void)src;
    InterlockedIncrement(&byref_dispose_calls);
}

static DWORD WINAPI block_worker(void *opaque)
{
    struct block_thread_context *ctx = opaque;
    for (int i = 0; i < BLOCK_ITERATIONS; ++i)
    {
        void *copy = _Block_copy(ctx->heap_block);
        if (copy != ctx->heap_block)
        {
            record_failure();
            return 1;
        }
        if (!_Block_tryRetain(ctx->heap_block))
        {
            record_failure();
            _Block_release(copy);
            return 2;
        }
        _Block_release(ctx->heap_block);
        _Block_release(copy);
    }
    return 0;
}
static DWORD WINAPI byref_worker(void *opaque)
{
    struct byref_thread_context *ctx = opaque;
    struct block_byref_obj *copy = NULL;
    _Block_object_assign(&copy, ctx->stack_byref, BLOCK_FIELD_IS_BYREF);
    if ((copy == NULL) || (copy == ctx->stack_byref) || (copy->forwarding != copy))
    {
        record_failure();
    }
    InterlockedIncrement(&assignment_ready);
    if (WaitForSingleObject(byref_release_event, INFINITE) != WAIT_OBJECT_0)
    {
        record_failure();
    }
    if (copy != NULL)
    {
        _Block_object_dispose(copy, BLOCK_FIELD_IS_BYREF);
    }
    return 0;
}

static int wait_for_threads(HANDLE *threads)
{
    DWORD result = WaitForMultipleObjects(THREAD_COUNT, threads, TRUE, 30000);
    for (int i = 0; i < THREAD_COUNT; ++i) { CloseHandle(threads[i]); }
    return result == WAIT_OBJECT_0 ? 0 : 1;
}
static int test_block_refcount(void)
{
    struct Block_descriptor descriptor = {
        0, sizeof(struct Block_layout), NULL, NULL, NULL
    };
    struct Block_layout stack_block = {
        &_NSConcreteStackBlock, 0, 0, NULL, &descriptor
    };
    CHECK(!_Block_isDeallocating(&stack_block), 10);
    CHECK(_Block_tryRetain(&stack_block), 11);
    CHECK(stack_block.reserved == 0, 12);

    struct Block_layout *heap_block = _Block_copy(&stack_block);
    CHECK(heap_block != NULL, 13);
    CHECK(heap_block != &stack_block, 14);
    CHECK(heap_block->reserved == 1, 15);

    struct block_thread_context ctx = { heap_block };
    HANDLE threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        threads[i] = CreateThread(NULL, 0, block_worker, &ctx, 0, NULL);
        CHECK(threads[i] != NULL, 16);
    }
    CHECK(wait_for_threads(threads) == 0, 17);
    CHECK(failures == 0, 18);
    CHECK(!_Block_isDeallocating(heap_block), 19);
    CHECK(InterlockedCompareExchange(
        (volatile LONG *)&heap_block->reserved, 0, 0) == 1, 20);

    _Block_release(heap_block);
    return 0;
}

static int test_byref_promotion(void)
{
    struct block_byref_obj byref = {0};
    byref.forwarding = &byref;
    byref.flags = BLOCK_HAS_COPY_DISPOSE;
    byref.size = sizeof(byref);
    byref.byref_keep = byref_keep_barrier;
    byref.byref_dispose = byref_dispose_probe;

    promotion_ready = 0;
    assignment_ready = 0;
    byref_dispose_calls = 0;
    byref_release_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    CHECK(byref_release_event != NULL, 30);
    struct byref_thread_context ctx = { &byref };
    HANDLE threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; ++i)
    {
        threads[i] = CreateThread(NULL, 0, byref_worker, &ctx, 0, NULL);
        CHECK(threads[i] != NULL, 31);
    }

    DWORD deadline = GetTickCount() + 10000;
    while (InterlockedCompareExchange(&assignment_ready, 0, 0) < THREAD_COUNT)
    {
        CHECK(GetTickCount() < deadline, 32);
        SwitchToThread();
    }

    struct block_byref_obj *forwarded =
        InterlockedCompareExchangePointer((void *volatile *)&byref.forwarding, NULL, NULL);
    CHECK(forwarded != NULL && forwarded != &byref, 33);
    LONG flags = InterlockedCompareExchange((volatile LONG *)&forwarded->flags, 0, 0);
    CHECK((flags & BLOCK_REFCOUNT_MASK) == THREAD_COUNT + 1, 34);

    SetEvent(byref_release_event);
    CHECK(SetEvent(byref_release_event) != 0, 35);
    CHECK(wait_for_threads(threads) == 0, 36);
    CHECK(failures == 0, 37);

    flags = InterlockedCompareExchange(
        (volatile LONG *)&forwarded->flags, 0, 0);
    CHECK((flags & BLOCK_REFCOUNT_MASK) == 1, 38);

    _Block_object_dispose(&byref, BLOCK_FIELD_IS_BYREF);
    CHECK(InterlockedCompareExchange(&byref_dispose_calls, 0, 0) == THREAD_COUNT, 39);
    CloseHandle(byref_release_event);
    byref_release_event = NULL;
    return 0;
}

static int test_legacy_weak_flags(void);

int main(void)
{
    mosaic_objc_runtime_initialize();
    failures = 0;

    int result = test_block_refcount();
    if (result != 0) { return result; }
    result = test_byref_promotion();
    if (result != 0) { return result; }
    result = test_legacy_weak_flags();
    if (result != 0) { return result; }

    puts("blocks-contract: ok");
    return 0;
}

static int test_legacy_weak_flags(void)
{
    void *sentinel = (void *)(uintptr_t)0x1234;
    void *dest = NULL;
    int object_weak = BLOCK_FIELD_IS_OBJECT | BLOCK_FIELD_IS_WEAK;
    _Block_object_assign(&dest, sentinel, object_weak);
    CHECK(dest == sentinel, 40);
    _Block_object_dispose(dest, object_weak);

    struct Block_descriptor descriptor = {
        0, sizeof(struct Block_layout), NULL, NULL, NULL
    };
    struct Block_layout stack_block = {
        &_NSConcreteStackBlock, 0, 0, NULL, &descriptor
    };
    struct Block_layout *heap_block = _Block_copy(&stack_block);
    CHECK(heap_block != NULL, 41);
    LONG before = InterlockedCompareExchange(
        (volatile LONG *)&heap_block->reserved, 0, 0);
    int block_weak = BLOCK_FIELD_IS_BLOCK | BLOCK_FIELD_IS_WEAK;
    dest = NULL;
    _Block_object_assign(&dest, heap_block, block_weak);
    CHECK(dest == heap_block, 42);
    CHECK(InterlockedCompareExchange((volatile LONG *)&heap_block->reserved, 0, 0) == before, 43);
    _Block_object_dispose(dest, block_weak);
    CHECK(InterlockedCompareExchange((volatile LONG *)&heap_block->reserved, 0, 0) == before, 44);
    _Block_release(heap_block);
    return 0;
}
