/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#include "test_support.h"
#include "sarray2.h"

int main(void)
{
	static int first, high, copy_only, expanded;
	CHECK(SparseArrayNewWithDepth(0) == NULL);
	CHECK(SparseArrayNewWithDepth(12) == NULL);
	CHECK(SparseArrayNewWithDepth(40) == NULL);
	SparseArray *array = SparseArrayNewWithDepth(32);
	CHECK(array != NULL);
	SparseArrayInsert(array, 1u, &first);
	SparseArrayInsert(array, UINT32_C(0xff000001), &high);
	CHECK(SparseArrayLookup(array, 1u) == &first);
	CHECK(SparseArrayLookup(array, UINT32_C(0xff000001)) == &high);

	uint32_t index = 0;
	CHECK(SparseArrayNext(array, &index) == &first);
	CHECK(index == 1u);
	CHECK(SparseArrayNext(array, &index) == &high);
	CHECK(index == UINT32_C(0xff000001));

	SparseArray *copy = SparseArrayCopy(array);
	CHECK(copy != NULL);
	SparseArrayInsert(copy, UINT32_C(0x12345678), &copy_only);
	CHECK(SparseArrayLookup(copy, UINT32_C(0x12345678)) == &copy_only);
	CHECK(SparseArrayLookup(array, UINT32_C(0x12345678)) == NULL);

	SparseArray *small = SparseArrayNewWithDepth(8);
	CHECK(small != NULL);
	SparseArrayInsert(small, 7u, &expanded);
	CHECK(SparseArrayExpandingArray(small, 8) == small);
	SparseArray *larger = SparseArrayExpandingArray(small, 16);
	CHECK(larger != NULL && larger != small);
	CHECK(SparseArrayLookup(larger, 7u) == &expanded);
	CHECK(SparseArrayExpandingArray(larger, 16) == larger);
	CHECK(SparseArrayExpandingArray(larger, 8) == NULL);

	SparseArray *direct = SparseArrayNewWithDepth(8);
	CHECK(direct != NULL);
	SparseArrayInsert(direct, 9u, &expanded);
	SparseArray *direct24 = SparseArrayExpandingArray(direct, 24);
	CHECK(direct24 != NULL && direct24 != direct);
	CHECK(direct24->shift == 16u);
	CHECK(SparseArrayLookup(direct24, 9u) == &expanded);

	SparseArrayDestroy(copy);
	SparseArrayDestroy(array);
	SparseArrayDestroy(larger);
	SparseArrayDestroy(direct24);
	return 0;
}
