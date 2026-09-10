#pragma once
#include <stdlib.h>
#include "allocation.h"

template<typename T>
T *allocate_zeroed(size_t extraSpace = 0)
{
	size_t allocationSize;
	if (!objc2_flexible_array_size(sizeof(T), extraSpace, 1, &allocationSize))
	{
		return nullptr;
	}
	return static_cast<T*>(calloc(1, allocationSize));
}

template<typename T>
T *allocate_zeroed_array(size_t elements)
{
	size_t allocationSize;
	if (!objc2_size_multiply(elements, sizeof(T), &allocationSize))
	{
		return nullptr;
	}
	return static_cast<T*>(calloc(1, allocationSize));
}


