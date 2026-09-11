/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#ifndef LIBOBJC2_RUNTIME_BITFIELD_H
#define LIBOBJC2_RUNTIME_BITFIELD_H

#include <stdint.h>
#include "objc/runtime/types.h"

/** Overflow bitmap used when inline class metadata cannot hold all fields. */
struct objc_bitfield
{
	int32_t length;
	int32_t values[0];
};

static inline BOOL objc_bitfield_test(uintptr_t bitfield, uint64_t field)
{
	if (bitfield == 0) { return NO; }
	if (bitfield & 1)
	{
		const uint64_t inlineBits = (sizeof(uintptr_t) * 8) - 1;
		if (field >= inlineBits) { return NO; }
		uintptr_t bit = (uintptr_t)1 << (field + 1);
		return (bitfield & bit) == bit;
	}
	struct objc_bitfield *overflow = (struct objc_bitfield*)bitfield;
	if (overflow->length <= 0) { return NO; }
	uint64_t word = field / 32;
	if (word >= (uint64_t)overflow->length) { return NO; }
	uint32_t bit = UINT32_C(1) << (field % 32);
	return (((uint32_t)overflow->values[word]) & bit) == bit;
}

#endif // LIBOBJC2_RUNTIME_BITFIELD_H
