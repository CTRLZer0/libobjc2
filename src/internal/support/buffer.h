/**
 * buffer.h defines a simple dynamic array that is used to store temporary
 * values for later processing.  Define BUFFER_TYPE before including this file.
 */

#include <limits.h>
#include <stdlib.h>
#include "allocation.h"

#define BUFFER_SIZE 128
static BUFFER_TYPE buffered_object_buffer[BUFFER_SIZE];
static BUFFER_TYPE *buffered_object_overflow;
static unsigned int buffered_objects;
static size_t buffered_object_overflow_space;

static int set_buffered_object_at_index(BUFFER_TYPE value, unsigned int index)
{
	if (index < BUFFER_SIZE)
	{
		buffered_object_buffer[index] = value;
		return 1;
	}

	size_t overflowIndex = (size_t)index - BUFFER_SIZE;
	size_t newSpace = buffered_object_overflow_space;
	if (newSpace == 0) { newSpace = BUFFER_SIZE; }
	while (overflowIndex >= newSpace)
	{
		if (newSpace > SIZE_MAX / 2) { return 0; }
		newSpace *= 2;
	}
	if (newSpace != buffered_object_overflow_space)
	{
		size_t bytes;
		if (!objc2_size_multiply(newSpace, sizeof(BUFFER_TYPE), &bytes)) { return 0; }
		BUFFER_TYPE *newBuffer = realloc(buffered_object_overflow, bytes);
		if (newBuffer == NULL) { return 0; }
		buffered_object_overflow = newBuffer;
		buffered_object_overflow_space = newSpace;
	}
	buffered_object_overflow[overflowIndex] = value;
	return 1;
}

static int append_buffered_object(BUFFER_TYPE value)
{
	if (buffered_objects == UINT_MAX) { return 0; }
	if (!set_buffered_object_at_index(value, buffered_objects)) { return 0; }
	buffered_objects++;
	return 1;
}

static BUFFER_TYPE buffered_object_at_index(unsigned int i)
{
	if (i < BUFFER_SIZE) { return buffered_object_buffer[i]; }
	return buffered_object_overflow[i - BUFFER_SIZE];
}

static void compact_buffer(void)
{
	unsigned int size = buffered_objects;
	unsigned int insert = 0;
	for (unsigned int i = 0; i < size; i++)
	{
		BUFFER_TYPE value = buffered_object_at_index(i);
		if (value != NULL)
		{
			if (!set_buffered_object_at_index(value, insert++)) { abort(); }
		}
	}
	buffered_objects = insert;
}