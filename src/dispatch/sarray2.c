#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "sarray2.h"
#include "visibility.h"

const static SparseArray EmptyArray = { 0, 0, .data[0 ... 255] = 0 };
const static SparseArray EmptyArray8 = { 8, 0, .data[0 ... 255] = (void*)&EmptyArray};
const static SparseArray EmptyArray16 = { 16, 0, .data[0 ... 255] = (void*)&EmptyArray8};
const static SparseArray EmptyArray24 = { 24, 0, .data[0 ... 255] = (void*)&EmptyArray16};

#define MAX_INDEX(sarray) (0xff)

// Tweak this value to trade speed for memory usage.  Bigger values use more
// memory, but give faster lookups.  
#define base_shift 8
#define base_mask ((UINT32_C(1) << base_shift) - 1)

static void *EmptyChildForShift(uint32_t shift)
{
	switch(shift)
	{
		default: UNREACHABLE("Broken sparse array");
		case 8:
			return (void*)&EmptyArray;
		case 16:
			return (void*)&EmptyArray8;
		case 24:
			return (void*)&EmptyArray16;
	}
}

static void init_pointers(SparseArray * sarray)
{
	if(sarray->shift != 0)
	{
		void *data = EmptyChildForShift(sarray->shift);
		for(unsigned i=0 ; i<=MAX_INDEX(sarray) ; i++)
		{
			sarray->data[i] = data;
		}
	}
}

static inline int valid_depth(uint32_t depth)
{
	return (depth >= base_shift) && (depth <= 32) && ((depth % base_shift) == 0);
}

PRIVATE SparseArray * SparseArrayNewWithDepth(uint32_t depth)
{
	if (!valid_depth(depth)) { return NULL; }
	SparseArray *sarray = calloc(1, sizeof(SparseArray));
	if (sarray == NULL) { return NULL; }
	sarray->refCount = 1;
	sarray->shift = depth - base_shift;
	init_pointers(sarray);
	return sarray;
}

PRIVATE SparseArray *SparseArrayNew()
{
	return SparseArrayNewWithDepth(32);
}
PRIVATE SparseArray *SparseArrayExpandingArray(SparseArray *sarray, uint32_t new_depth)
{
	if ((sarray == NULL) || !valid_depth(new_depth)) { return NULL; }
	const uint32_t current_depth = sarray->shift + base_shift;
	if (new_depth == current_depth) { return sarray; }
	if (new_depth != current_depth + base_shift) { return NULL; }
	// Expanding a child sarray has undefined results.
	assert(sarray->refCount == 1);
	SparseArray *new = calloc(1, sizeof(SparseArray));
	if (new == NULL) { return NULL; }
	new->refCount = 1;
	new->shift = sarray->shift + base_shift;
	new->data[0] = sarray;
	void *data = EmptyChildForShift(new->shift);
	for (unsigned i = 1; i <= MAX_INDEX(sarray); ++i)
	{
		new->data[i] = data;
	}
	return new;
}

static inline int is_empty_child(const SparseArray *child)
{
	return (child == &EmptyArray) || (child == &EmptyArray8) ||
	       (child == &EmptyArray16) || (child == &EmptyArray24);
}

static void *SparseArrayFindFrom(SparseArray *sarray, uint32_t start,
                                 uint32_t prefix, uint32_t *found)
{
	const uint32_t shift = sarray->shift;
	const uint32_t first = (start >> shift) & UINT32_C(0xff);
	for (uint32_t slot = first; slot <= UINT32_C(0xff); ++slot)
	{
		const uint32_t slot_prefix = prefix | (slot << shift);
		if (shift == 0)
		{
			void *value = sarray->data[slot];
			if ((value != SARRAY_EMPTY) && (slot_prefix >= start))
			{
				*found = slot_prefix;
				return value;
			}
			continue;
		}

		SparseArray *child = sarray->data[slot];
		if (is_empty_child(child)) { continue; }
		const uint32_t child_start = (slot == first) ? start : slot_prefix;
		void *value = SparseArrayFindFrom(child, child_start, slot_prefix, found);
		if (value != SARRAY_EMPTY) { return value; }
	}
	return SARRAY_EMPTY;
}

PRIVATE void *SparseArrayNext(SparseArray *sarray, uint32_t *idx)
{
	if ((sarray == NULL) || (idx == NULL) || (*idx == UINT32_MAX))
	{
		return SARRAY_EMPTY;
	}
	const uint32_t start = *idx + 1;
	void *value = SparseArrayFindFrom(sarray, start, 0, idx);
	if (value == SARRAY_EMPTY) { *idx = UINT32_MAX; }
	return value;
}

PRIVATE void SparseArrayInsert(SparseArray * sarray, uint32_t index, void *value)
{
	if (sarray->shift > 0)
	{
		uint32_t i = MASK_INDEX(index);
		SparseArray *child = sarray->data[i];
		if (is_empty_child(child))
		{
			// Insert missing nodes
			SparseArray * newsarray = calloc(1, sizeof(SparseArray));
			newsarray->refCount = 1;
			if (base_shift >= sarray->shift)
			{
				newsarray->shift = 0;
			}
			else
			{
				newsarray->shift = sarray->shift - base_shift;
			}
			init_pointers(newsarray);
			sarray->data[i] = newsarray;
			child = newsarray;
		}
		else if (child->refCount > 1)
		{
			// Copy the copy-on-write part of the tree
			sarray->data[i] = SparseArrayCopy(child);
			SparseArrayDestroy(child);
			child = sarray->data[i];
		}
		SparseArrayInsert(child, index, value);
	}
	else
	{
		sarray->data[MASK_INDEX(index)] = value;
	}
}

PRIVATE SparseArray *SparseArrayCopy(SparseArray * sarray)
{
	SparseArray *copy = calloc(1, sizeof(SparseArray));
	memcpy(copy, sarray, sizeof(SparseArray));
	copy->refCount = 1;
	// If the sarray has children, increase their refcounts and link them
	if (sarray->shift > 0)
	{
		for (unsigned int i = 0 ; i<=MAX_INDEX(sarray); i++)
		{
			SparseArray *child = copy->data[i];
			if (!is_empty_child(child))
			{
				__sync_fetch_and_add(&child->refCount, 1);
			}
			// Non-lazy copy.  Uncomment if debugging 
			// copy->data[i] = SparseArrayCopy(copy->data[i]);
		}
	}
	return copy;
}

PRIVATE void SparseArrayDestroy(SparseArray * sarray)
{
	// Don't really delete this sarray if its ref count is > 0
	if (sarray == &EmptyArray ||
	    sarray == &EmptyArray8 ||
	    sarray == &EmptyArray16 ||
	    sarray == &EmptyArray24 ||
		(__sync_sub_and_fetch(&sarray->refCount, 1) > 0))
 	{
		return;
	}

	if(sarray->shift > 0)
	{
		for(uint32_t i=0 ; i<data_size ; i++)
		{
			SparseArrayDestroy((SparseArray*)sarray->data[i]);
		}
	}
	free(sarray);
}

#if 0
// Unused function, but helpful when debugging.
PRIVATE int SparseArraySize(SparseArray *sarray)
{
	int size = 0;
	if (sarray->shift == 0)
	{
		return 256*sizeof(void*) + sizeof(SparseArray);
	}
	size += 256*sizeof(void*) + sizeof(SparseArray);
	for(unsigned i=0 ; i<=MAX_INDEX(sarray) ; i++)
	{
		SparseArray *child = sarray->data[i];
		if (child == &EmptyArray || 
		    child == &EmptyArray8 || 
		    child == &EmptyArray16)
		{
			continue;
		}
		size += SparseArraySize(child);
	}
	return size;
}
#endif
