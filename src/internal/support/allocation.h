/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#ifndef LIBOBJC2_ALLOCATION_H
#define LIBOBJC2_ALLOCATION_H

#include <stddef.h>
#include <stdint.h>

static inline int objc2_flexible_array_size(size_t headerSize,
                                             size_t count,
                                             size_t elementSize,
                                             size_t *result)
{
	if (NULL == result) { return 0; }
	if ((elementSize != 0) && (count > (SIZE_MAX - headerSize) / elementSize))
	{
		return 0;
	}
	*result = headerSize + count * elementSize;
	return 1;
}

#endif // LIBOBJC2_ALLOCATION_H
