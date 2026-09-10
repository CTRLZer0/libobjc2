/*
 * Copyright (c) 2009 Remy Demarest
 * Portions Copyright (c) 2009 David Chisnall
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */
#import "objc/blocks_runtime.h"
#include "objc/blocks_private.h"
#import "objc/runtime.h"
#import "objc/memory/arc.h"
#include "blocks_runtime.h"
#include "gc_ops.h"
#include "visibility.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>


static void *_HeapBlockByRef = (void*)1;


OBJC_PUBLIC bool _Block_has_signature(void *b)
{
	const struct Block_layout *block = (struct Block_layout*)b;
	return ((NULL != block) && (block->flags & BLOCK_HAS_SIGNATURE));
}
/**
 * Returns the Objective-C type encoding for the block.
 */
OBJC_PUBLIC const char * _Block_signature(void *b)
{
	const struct Block_layout *block = (struct Block_layout*)b;
	if ((NULL == block) || !(block->flags & BLOCK_HAS_SIGNATURE))
	{
		return NULL;
	}
	if (!(block->flags & BLOCK_HAS_COPY_DISPOSE))
	{
		return ((struct Block_descriptor_basic*)block->descriptor)->encoding;
	}
	return block->descriptor->encoding;
}

static int increment24(int *ref)
{
	int old = __atomic_load_n(ref, __ATOMIC_RELAXED);
	for (;;)
	{
		const int count = old & BLOCK_REFCOUNT_MASK;
		if (count == BLOCK_REFCOUNT_MASK) { return count; }
		const int desired = old + 1;
		if (__atomic_compare_exchange_n(ref, &old, desired, true,
				__ATOMIC_RELAXED, __ATOMIC_RELAXED)) { return count + 1; }
	}
}

static int decrement24(int *ref)
{
	int old = __atomic_load_n(ref, __ATOMIC_RELAXED);
	for (;;)
	{
		const int count = old & BLOCK_REFCOUNT_MASK;
		if (count == BLOCK_REFCOUNT_MASK) { return count; }
		if (count == 0) { abort(); }
		const int desired = old - 1;
		if (__atomic_compare_exchange_n(ref, &old, desired, true,
				__ATOMIC_RELEASE, __ATOMIC_RELAXED))
		{
			if ((count - 1) == 0) { __atomic_thread_fence(__ATOMIC_ACQUIRE); }
			return count - 1;
		}
	}
}

static void retainBlockRefcount(int *ref)
{
	const int old = __atomic_fetch_add(ref, 1, __ATOMIC_RELAXED);
	if ((old <= 0) || (old == INT_MAX)) { abort(); }
}

static bool tryIncrementBlockRefcount(int *ref)
{
	int old = __atomic_load_n(ref, __ATOMIC_RELAXED);
	for (;;)
	{
		if ((old <= 0) || (old == INT_MAX)) { return false; }
		const int desired = old + 1;
		if (__atomic_compare_exchange_n(ref, &old, desired, true,
				__ATOMIC_RELAXED, __ATOMIC_RELAXED)) { return true; }
	}
}

static bool releaseBlockRefcount(int *ref)
{
	const int old = __atomic_fetch_sub(ref, 1, __ATOMIC_RELEASE);
	if (old <= 0) { abort(); }
	if (old != 1) { return false; }
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	return true;
}

/* Certain field types require runtime assistance when being copied to the
 * heap.  The following function is used to copy fields of types: blocks,
 * pointers to byref structures, and objects (including
 * __attribute__((NSObject)) pointers.  BLOCK_FIELD_IS_WEAK is orthogonal to
 * the other choices which are mutually exclusive.  Only in a Block copy helper
 * will one see BLOCK_FIELD_IS_BYREF.
 */
OBJC_PUBLIC void _Block_object_assign(void *destAddr, const void *object, const int flags)
{
	if (IS_SET(flags, BLOCK_FIELD_IS_BYREF))
	{
		struct block_byref_obj *src = (struct block_byref_obj *)object;
		struct block_byref_obj **dst = destAddr;
		src = __atomic_load_n(&src->forwarding, __ATOMIC_ACQUIRE);
		if ((__atomic_load_n(&src->flags, __ATOMIC_RELAXED) & BLOCK_REFCOUNT_MASK) == 0)
		{
			struct block_byref_obj *candidate = gc->malloc(src->size);
			if (candidate == NULL) { abort(); }
			memcpy(candidate, src, src->size);
			candidate->isa = _HeapBlockByRef;
			candidate->flags += 2;
			if (IS_SET(src->flags, BLOCK_HAS_COPY_DISPOSE))
			{
				src->byref_keep(candidate, src);
			}
			candidate->forwarding = candidate;
			struct block_byref_obj *expected = src;
			if (!__atomic_compare_exchange_n(&src->forwarding, &expected, candidate, false,
					__ATOMIC_RELEASE, __ATOMIC_ACQUIRE))
			{
				if (IS_SET(candidate->flags, BLOCK_HAS_COPY_DISPOSE) && candidate->byref_dispose)
				{
					candidate->byref_dispose(candidate);
				}
				gc->free(candidate);
				increment24(&expected->flags);
				*dst = expected;
			}
			else { *dst = candidate; }
		}
		else
		{
			*dst = src;
			increment24(&src->flags);
		}
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_WEAK))
	{
		*(const void **)destAddr = object;
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_BLOCK))
	{
		*(void **)destAddr = _Block_copy(object);
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_OBJECT) && !IS_SET(flags, BLOCK_BYREF_CALLER))
	{
		*(id *)destAddr = objc_retain((id)object);
		return;
	}
	*(const void **)destAddr = object;
}

/* Similarly a compiler generated dispose helper needs to call back for each
 * field of the byref data structure.  (Currently the implementation only packs
 * one field into the byref structure but in principle there could be more).
 * The same flags used in the copy helper should be used for each call
 * generated to this function:
 */
OBJC_PUBLIC void _Block_object_dispose(const void *object, const int flags)
{
	if (IS_SET(flags, BLOCK_FIELD_IS_BYREF))
	{
		struct block_byref_obj *src = (struct block_byref_obj*)object;
		src = __atomic_load_n(&src->forwarding, __ATOMIC_ACQUIRE);
		if (src->isa == _HeapBlockByRef)
		{
			const int count = __atomic_load_n(&src->flags, __ATOMIC_RELAXED) & BLOCK_REFCOUNT_MASK;
			if ((count != 0) && (decrement24(&src->flags) == 0))
			{
				if (IS_SET(src->flags, BLOCK_HAS_COPY_DISPOSE) && src->byref_dispose)
				{
					src->byref_dispose(src);
				}
				gc->free(src);
			}
		}
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_WEAK))
	{
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_BLOCK))
	{
		_Block_release(object);
		return;
	}
	if (IS_SET(flags, BLOCK_FIELD_IS_OBJECT) && !IS_SET(flags, BLOCK_BYREF_CALLER))
	{
		objc_release((id)object);
	}
}

// Copy a block to the heap if it is still on the stack, otherwise retain it.
OBJC_PUBLIC void *_Block_copy(const void *src)
{
	if (src == NULL) { return NULL; }
	struct Block_layout *self = (struct Block_layout*)src;
	extern void _NSConcreteStackBlock;
	extern void _NSConcreteMallocBlock;

	if (self->isa == &_NSConcreteStackBlock)
	{
		struct Block_layout *ret = gc->malloc(self->descriptor->size);
		if (ret == NULL) { return NULL; }
		memcpy(ret, self, self->descriptor->size);
		ret->isa = &_NSConcreteMallocBlock;
		if (self->flags & BLOCK_HAS_COPY_DISPOSE)
		{
			self->descriptor->copy_helper(ret, self);
		}
		ret->reserved = 1;
		return ret;
	}
	if (self->isa == &_NSConcreteMallocBlock)
	{
		retainBlockRefcount(&self->reserved);
		return self;
	}
	return self;
}

OBJC_PUBLIC void _Block_release(const void *src)
{
	if (src == NULL) { return; }
	struct Block_layout *self = (struct Block_layout*)src;
	extern void _NSConcreteStackBlock;
	extern void _NSConcreteMallocBlock;

	if (&_NSConcreteStackBlock == self->isa)
	{
		fprintf(stderr, "Block_release called upon a stack Block: %p, ignored\n", self);
		return;
	}
	if ((&_NSConcreteMallocBlock == self->isa) && releaseBlockRefcount(&self->reserved))
	{
		if (self->flags & BLOCK_HAS_COPY_DISPOSE)
		{
			self->descriptor->dispose_helper(self);
		}
		objc_delete_weak_refs((id)self);
		gc->free(self);
	}
}

OBJC_PUBLIC bool _Block_isDeallocating(const void *arg)
{
	if (arg == NULL) { return true; }
	const struct Block_layout *block = (const struct Block_layout*)arg;
	extern void _NSConcreteMallocBlock;
	if (block->isa != &_NSConcreteMallocBlock) { return false; }
	return __atomic_load_n(&block->reserved, __ATOMIC_ACQUIRE) <= 0;
}

OBJC_PUBLIC bool _Block_tryRetain(const void *arg)
{
	if (arg == NULL) { return false; }
	struct Block_layout *block = (struct Block_layout*)arg;
	extern void _NSConcreteMallocBlock;
	if (block->isa != &_NSConcreteMallocBlock) { return true; }
	return tryIncrementBlockRefcount(&block->reserved);
}
