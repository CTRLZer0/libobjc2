#if defined(__clang__) && !defined(__OBJC_RUNTIME_INTERNAL__)
#pragma clang system_header
#endif

#ifndef __LIBOBJC_RUNTIME_SMALL_OBJECT_H_INCLUDED__
#define __LIBOBJC_RUNTIME_SMALL_OBJECT_H_INCLUDED__

#include "runtime-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Registers a class for small objects.  Small objects are stored inside a
 * pointer.  If the class can be registered, then this returns YES.  The second
 * argument specifies the bit pattern to use to identify the small object.
 */
OBJC_PUBLIC
BOOL objc_registerSmallObjectClass_np(Class cls, uintptr_t classId);

/**
 * The mask identifying the bits that can be used in an object pointer to
 * identify a small object.  On 32-bit systems, we use the low bit.  On 64-bit
 * systems, we use the low 3 bits.  In both cases, the lowest bit must be 1.
 * This restriction may be relaxed in the future on 64-bit systems.
 */
#ifndef UINTPTR_MAX
#	define OBJC_SMALL_OBJECT_MASK ((sizeof(void*) == 4) ? 1 : 7)
#elif UINTPTR_MAX < UINT64_MAX
#	define OBJC_SMALL_OBJECT_MASK 1
#else
#	define OBJC_SMALL_OBJECT_MASK 7
#endif
/**
 * The number of bits reserved for the class identifier in a small object.
 */
#ifndef UINTPTR_MAX
#	define OBJC_SMALL_OBJECT_SHIFT ((sizeof(void*) == 4) ? 1 : 3)
#elif UINTPTR_MAX < UINT64_MAX
#	define OBJC_SMALL_OBJECT_SHIFT 1
#else
#	define OBJC_SMALL_OBJECT_SHIFT 3
#endif

#ifdef __cplusplus
}
#endif

#endif // __LIBOBJC_RUNTIME_SMALL_OBJECT_H_INCLUDED__
