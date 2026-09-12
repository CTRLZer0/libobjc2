#pragma once
#include "platform.h"

template<typename T>
class PoolAllocate
{
	static constexpr size_t ObjectsPerChunk = 4096;
	static constexpr size_t ChunkSize = sizeof(T) * ObjectsPerChunk;
	static inline size_t index = ObjectsPerChunk;
	static inline T *buffer = nullptr;
	public:
	static T *allocate()
	{
		if (index == ObjectsPerChunk)
		{
			T *newBuffer = static_cast<T*>(objc_platform_pages_allocate(ChunkSize));
			if (newBuffer == nullptr) { return nullptr; }
			buffer = newBuffer;
			index = 0;
		}
		return &buffer[index++];
	}
};


