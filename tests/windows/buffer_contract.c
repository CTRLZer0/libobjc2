/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 */
#include "test_support.h"
#include <stdint.h>

#define BUFFER_TYPE void *
#include "buffer.h"

int main(void)
{
	for (unsigned int i = 0; i < 512; ++i)
	{
		CHECK(append_buffered_object((void*)(uintptr_t)(i + 1)) != 0);
	}
	CHECK(buffered_objects == 512);
	for (unsigned int i = 0; i < buffered_objects; ++i)
	{
		CHECK(buffered_object_at_index(i) == (void*)(uintptr_t)(i + 1));
	}

	for (unsigned int i = 1; i < buffered_objects; i += 3)
	{
		CHECK(set_buffered_object_at_index(NULL, i) != 0);
	}
	compact_buffer();

	unsigned int expectedCount = 0;
	for (unsigned int i = 0; i < 512; ++i)
	{
		if ((i % 3) != 1)
		{
			CHECK(buffered_object_at_index(expectedCount) ==
			      (void*)(uintptr_t)(i + 1));
			expectedCount++;
		}
	}
	CHECK(buffered_objects == expectedCount);
	CHECK(buffered_object_overflow != NULL);
	CHECK(buffered_object_overflow_space >= (512 - BUFFER_SIZE));
	free(buffered_object_overflow);
	return 0;
}
