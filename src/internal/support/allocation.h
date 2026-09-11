/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#ifndef LIBOBJC2_ALLOCATION_H
#define LIBOBJC2_ALLOCATION_H

#include <stddef.h>
#include <stdint.h>

static inline int objc2_size_add(size_t left, size_t right, size_t *result)
{
	if (NULL == result) { return 0; }
	if (left > SIZE_MAX - right) { return 0; }
	*result = left + right;
	return 1;
}

static inline int objc2_size_multiply(size_t left, size_t right, size_t *result)
{
	if (NULL == result) { return 0; }
	if ((right != 0) && (left > SIZE_MAX / right)) { return 0; }
	*result = left * right;
	return 1;
}

static inline int objc2_flexible_array_size(size_t headerSize,
                                             size_t count,
                                             size_t elementSize,
                                             size_t *result)
{
	if (NULL == result) { return 0; }
	size_t elementsSize;
	if (!objc2_size_multiply(count, elementSize, &elementsSize) ||
	    (headerSize > SIZE_MAX - elementsSize)) { return 0; }
	*result = headerSize + elementsSize;
	return 1;
}

#endif // LIBOBJC2_ALLOCATION_H
