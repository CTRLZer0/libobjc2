/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <stdint.h>
#include "bitfield.h"

int main(void)
{
	CHECK(objc_bitfield_test(0, 0) == NO);

	const uint64_t inlineBits = (sizeof(uintptr_t) * 8) - 1;
	uintptr_t inlineStorage = (uintptr_t)1;
	inlineStorage |= (uintptr_t)1 << 1;
	inlineStorage |= (uintptr_t)1 << inlineBits;
	CHECK(objc_bitfield_test(inlineStorage, 0) == YES);
	CHECK(objc_bitfield_test(inlineStorage, 1) == NO);
	CHECK(objc_bitfield_test(inlineStorage, inlineBits - 1) == YES);
	CHECK(objc_bitfield_test(inlineStorage, inlineBits) == NO);
	CHECK(objc_bitfield_test(inlineStorage, UINT64_MAX) == NO);

	struct
	{
		int32_t length;
		int32_t values[2];
	} overflow = { 2, { (int32_t)(UINT32_C(1) << 3), 1 } };
	uintptr_t overflowStorage = (uintptr_t)&overflow;
	CHECK((overflowStorage & 1) == 0);
	CHECK(objc_bitfield_test(overflowStorage, 3) == YES);
	CHECK(objc_bitfield_test(overflowStorage, 2) == NO);
	CHECK(objc_bitfield_test(overflowStorage, 32) == YES);
	CHECK(objc_bitfield_test(overflowStorage, 64) == NO);

	struct { int32_t length; int32_t values[1]; } invalid = { -1, { -1 } };
	CHECK(objc_bitfield_test((uintptr_t)&invalid, 0) == NO);
	return 0;
}
