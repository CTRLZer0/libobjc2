#include "visibility.h"
#include "objc/runtime.h"
#include "gc_ops.h"
#include "class.h"
#include "platform.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

// Alignment of object allocations.  The reference-count word precedes the
// object and thus sits at the head of the block, so aligning the block to a
// cache line puts each object's reference count on its own line: two distinct
// objects are then >= one line apart and never share a line for their counts.
// That eliminates the false-sharing cliff where independent threads
// retaining/releasing adjacent small objects ping-pong a shared line
// (measured: distinct-object retain/release at 4 threads 127ns -> 24ns).
//
// This is the default.  The cost is memory: cache-line alignment rounds every
// small allocation up to the alignment, which roughly DOUBLES the footprint of
// the smallest objects (measured: 32 -> 64 bytes RSS for a 16-byte instance).
// Build with -DOBJC_ALLOC_ALIGN=16 (32 on Windows, the minimum vector ivars
// need) to trade the multicore retain/release scaling back for that memory.
#ifndef OBJC_ALLOC_ALIGN
#  define OBJC_ALLOC_ALIGN 64
#endif
// The alignment reaches posix_memalign (and _aligned_malloc on Windows), which
// require a power of two, so reject a bad -DOBJC_ALLOC_ALIGN at compile time
// rather than failing every allocation at run time.
_Static_assert((OBJC_ALLOC_ALIGN & (OBJC_ALLOC_ALIGN - 1)) == 0,
               "OBJC_ALLOC_ALIGN must be a power of two");
_Static_assert(OBJC_ALLOC_ALIGN >= _Alignof(max_align_t),
               "OBJC_ALLOC_ALIGN must be at least alignof(max_align_t)");

static id allocate_class(Class cls, size_t extraBytes)
{
	size_t size = (size_t)cls->instance_size;
	if (extraBytes > SIZE_MAX - size) { return NULL; }
	size += extraBytes;
	if (sizeof(intptr_t) > SIZE_MAX - size) { return NULL; }
	size += sizeof(intptr_t);

	intptr_t *addr = objc_platform_aligned_alloc_zero(OBJC_ALLOC_ALIGN, size);
	if (addr == NULL) { return NULL; }
	return (id)(addr + 1);
}

static void free_object(id obj)
{
	if (obj == nil) { return; }
	objc_platform_aligned_free((void*)(((intptr_t*)obj) - 1));
}

static void *alloc(size_t size)
{
	return calloc(1, size);
}

void objc_registerThreadWithCollector(void) {}
void objc_unregisterThreadWithCollector(void) {}
void objc_assertRegisteredThreadWithCollector() {}

PRIVATE struct gc_ops gc_ops_none = 
{
	.allocate_class = allocate_class,
	.free_object    = free_object,
	.malloc         = alloc,
	.free           = free
};
PRIVATE struct gc_ops *gc = &gc_ops_none;

void objc_set_collection_threshold(size_t threshold) {}
void objc_set_collection_ratio(size_t ratio) {}
void objc_collect(unsigned long options) {}
BOOL objc_collectingEnabled(void) { return NO; }
static BOOL compare_and_swap(id predicate, id replacement, volatile id *objectLocation,
                             int successOrder, int failureOrder)
{
	id expected = predicate;
	return __atomic_compare_exchange_n(objectLocation, &expected, replacement, 0,
		successOrder, failureOrder);
}

BOOL objc_atomicCompareAndSwapPtr(id predicate, id replacement, volatile id *objectLocation)
{
	return compare_and_swap(predicate, replacement, objectLocation,
		__ATOMIC_RELAXED, __ATOMIC_RELAXED);
}
BOOL objc_atomicCompareAndSwapPtrBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return compare_and_swap(predicate, replacement, objectLocation,
		__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

BOOL objc_atomicCompareAndSwapGlobal(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapGlobalBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtrBarrier(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapInstanceVariable(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtr(predicate, replacement, objectLocation);
}
BOOL objc_atomicCompareAndSwapInstanceVariableBarrier(id predicate, id replacement, volatile id *objectLocation)
{
	return objc_atomicCompareAndSwapPtrBarrier(predicate, replacement, objectLocation);
}

id objc_assign_strongCast(id val, id *ptr)
{
	*ptr = val;
	return val;
}

id objc_assign_global(id val, id *ptr)
{
	*ptr = val;
	return val;
}

id objc_assign_ivar(id val, id dest, ptrdiff_t offset)
{
	*(id*)((char*)dest+offset) = val;
	return val;
}

void *objc_memmove_collectable(void *dst, const void *src, size_t size)
{
	return memmove(dst, src, size);
}
id objc_read_weak(id *location)
{
	return *location;
}
id objc_assign_weak(id value, id *location)
{
	*location = value;
	return value;
}
id objc_allocate_object(Class cls, int extra)
{
	return class_createInstance(cls, extra);
}

BOOL objc_collecting_enabled(void) { return NO; }
void objc_startCollectorThread(void) {}
void objc_clear_stack(unsigned long options) {}
BOOL objc_is_finalized(void *ptr) { return NO; }
void objc_finalizeOnMainThread(Class cls) {}
